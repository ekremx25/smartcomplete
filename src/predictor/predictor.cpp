#include "predictor/predictor.h"
#include "predictor/ai_reranker.h"
#include "predictor/grammar_rules.h"
#include "predictor/text_utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>

namespace linuxcomplete {

namespace {

size_t utf8_codepoint_length(const std::string& text) {
    size_t count = 0;
    for (size_t i = 0; i < text.size(); ++count) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            i += std::min<size_t>(2, text.size() - i);
        } else if ((c & 0xF0) == 0xE0) {
            i += std::min<size_t>(3, text.size() - i);
        } else if ((c & 0xF8) == 0xF0) {
            i += std::min<size_t>(4, text.size() - i);
        } else {
            i += 1;
        }
    }
    return count;
}

// All scoring rules now live in data/rules/*.txt — no hardcoded rules in this file.
// - en_phrase_bonus_rules.txt: positive + negative pair bonuses (merged from old english_filter_penalty)
// - en_context_bonus_rules.txt: triple bonuses
// - en_grammar_pair_rules.txt, en_grammar_triple_rules.txt: general grammar rules
// Lookup is O(1) via grammar_rules.h indexed APIs.

} // namespace

Predictor::Predictor(const PredictorConfig& config) : config_(config) {}
Predictor::~Predictor() { save(); }

// Delegates to shared text_utils — single source of truth.
std::string Predictor::to_lower_ascii(std::string word) { return text_utils::to_lower_ascii(std::move(word)); }
std::string Predictor::capitalize_for_display(const std::string& word) { return text_utils::capitalize_for_display(word); }
std::string Predictor::fold_for_matching(std::string text) { return text_utils::fold_for_matching(std::move(text)); }

bool Predictor::starts_with_folded(const std::string& text, const std::string& prefix) {
    if (prefix.empty()) {
        return true;
    }

    return fold_for_matching(text).rfind(fold_for_matching(prefix), 0) == 0;
}

std::vector<std::string> Predictor::split_text_tokens(const std::string& text) {
    std::istringstream iss(text);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

void Predictor::merge_candidate(std::unordered_map<std::string, Candidate>& merged,
                                const Candidate& candidate) {
    std::string key = to_lower_ascii(candidate.word);
    auto& slot = merged[key];
    if (slot.word.empty()) {
        slot = candidate;
        return;
    }
    slot.score += candidate.score;
}

Language Predictor::preferred_language(const std::string& /*prefix*/) const {
    return Language::English;
}

std::vector<Candidate> Predictor::sentence_starter_predictions(const std::string& prefix,
                                                               int max_results) const {
    static const std::vector<std::string> starters = {
        "i", "the", "this", "we", "you", "it", "they", "today", "hello", "thanks",
        "actually", "honestly", "maybe", "well", "so", "because", "if", "when",
        "sorry", "please", "good", "love"
    };

    std::vector<Candidate> results;
    results.reserve(starters.size());
    for (size_t i = 0; i < starters.size(); ++i) {
        const auto& word = starters[i];
        if (!starts_with_folded(word, prefix)) {
            continue;
        }

        int score = static_cast<int>((starters.size() - i) * 120);
        auto it = user_freq_.find(to_lower_ascii(word));
        if (it != user_freq_.end()) {
            score += it->second * 40;
        }
        results.push_back({word, score});
    }

    std::sort(results.begin(), results.end());
    if (results.size() > static_cast<size_t>(max_results)) {
        results.resize(max_results);
    }
    return results;
}

std::vector<Candidate> Predictor::contraction_predictions(const std::string& prefix,
                                                          int max_results) const {
    if (preferred_language(prefix) != Language::English) {
        return {};
    }

    static const std::vector<std::string> contractions = {
        "i'm", "i'll", "i'd", "i've", "don't", "can't", "won't", "didn't",
        "isn't", "aren't", "it's", "that's", "there's", "you're", "we're",
        "they're", "you'll", "we'll", "they'll", "haven't", "hasn't", "wouldn't",
        "shouldn't", "couldn't", "wasn't", "weren't", "doesn't", "there'll",
        "there'd", "you've", "we've", "they've", "who's", "what's", "where's"
    };

    std::vector<Candidate> results;
    results.reserve(contractions.size());
    for (size_t i = 0; i < contractions.size(); ++i) {
        const auto& word = contractions[i];
        if (!starts_with_folded(word, prefix)) {
            continue;
        }

        int score = static_cast<int>((contractions.size() - i) * 100);
        auto it = user_freq_.find(to_lower_ascii(word));
        if (it != user_freq_.end()) {
            score += it->second * 50;
        }
        results.push_back({word, score});
    }

    std::sort(results.begin(), results.end());
    if (results.size() > static_cast<size_t>(max_results)) {
        results.resize(max_results);
    }
    return results;
}

bool Predictor::init() {
    bool success = false;

    // Load English dictionary
    std::string en_dict = config_.dict_dir + "/en_US.txt";
    std::string en_freq = config_.dict_dir + "/../frequency/en_freq.txt";
    if (std::filesystem::exists(en_dict)) {
        success |= dictionary_.load(en_dict, en_freq, Language::English);
    }

    // Load user dictionary
    if (!config_.user_dict_path.empty()) {
        dictionary_.load_user_dict(config_.user_dict_path);
    }

    // Load ngram data
    std::string ngram_dir = config_.dict_dir + "/../ngram";
    std::string rules_dir = config_.dict_dir + "/../rules";
    std::string en_bigrams = ngram_dir + "/en_bigrams.txt";

    if (std::filesystem::exists(rules_dir)) {
        set_rules_data_dir(rules_dir);
    }

    if (std::filesystem::exists(en_bigrams)) {
        system_ngram_.load(en_bigrams);
    }

    // Load user learned bigrams
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "";
    user_ngram_path_ = home + "/.local/share/linuxcomplete/user/learned_bigrams.txt";
    user_freq_path_ = home + "/.local/share/linuxcomplete/user/user_freq.txt";

    if (std::filesystem::exists(user_ngram_path_)) {
        user_ngram_.load(user_ngram_path_);
    }

    // Load phrase completions
    std::string en_phrases = ngram_dir + "/en_phrases.txt";
    if (std::filesystem::exists(en_phrases)) {
        phrase_model_.load(en_phrases);
    }

    // Load emoji data
    std::string emoji_dir = config_.dict_dir + "/../emoji";
    std::string en_emoji = emoji_dir + "/en_emoji.tsv";
    if (std::filesystem::exists(en_emoji)) {
        emoji_model_ = EmojiModel(en_emoji);
    }

    // Load user word frequencies
    load_user_freq();

    std::cout << "[SmartComplete] Predictor initialized with "
              << dictionary_.trie().size() << " words, "
              << (system_ngram_.size() + user_ngram_.size()) << " bigrams, "
              << phrase_model_.size() << " phrases, "
              << emoji_model_.size() << " emojis, "
              << user_freq_.size() << " user freq entries" << std::endl;

    return success;
}

std::vector<Candidate> Predictor::predict(const std::string& prefix) const {
    if (utf8_codepoint_length(prefix) < static_cast<size_t>(config_.min_prefix_length)) {
        return {};
    }

    auto trie_results = dictionary_.trie().search(prefix, config_.max_candidates * 12);
    std::unordered_map<std::string, Candidate> merged_results;

    for (auto& candidate : trie_results) {
        std::string key = to_lower_ascii(candidate.word);
        auto it = user_freq_.find(key);
        if (it != user_freq_.end()) {
            candidate.score += it->second * 50;
        }
        merged_results[key] = candidate;
    }

    auto context_candidates = merge_context_predictions(20);
    for (const auto& candidate : context_candidates) {
        if (!starts_with_folded(candidate.word, prefix)) {
            continue;
        }

        merge_candidate(merged_results, candidate);
    }

    if (sentence_start_) {
        for (const auto& candidate : sentence_starter_predictions(prefix, config_.max_candidates * 2)) {
            merge_candidate(merged_results, candidate);
        }
    }

    for (const auto& candidate : contraction_predictions(prefix, config_.max_candidates * 2)) {
        merge_candidate(merged_results, candidate);
    }

    for (const auto& candidate : typo_correction_predictions(prefix, config_.max_candidates * 2)) {
        merge_candidate(merged_results, candidate);
    }

    std::vector<Candidate> results;
    results.reserve(merged_results.size());
    for (const auto& [_, candidate] : merged_results) {
        results.push_back(candidate);
    }

    std::sort(results.begin(), results.end());

    if (sentence_start_) {
        for (auto& candidate : results) {
            candidate.word = capitalize_for_display(candidate.word);
        }
    }

    if (results.size() > static_cast<size_t>(config_.max_candidates)) {
        results.resize(config_.max_candidates);
    }

    auto filtered = filter_candidates(prefix, std::move(results));
    AiReranker reranker({config_.ai_rerank_enabled,
                         config_.ai_smart_fallback,
                         config_.ai_debug_logging,
                         config_.ai_model,
                         config_.ai_timeout_ms,
                         config_.ai_uncertainty_gap_threshold,
                         static_cast<size_t>(std::max(1, config_.ai_max_cache_entries)),
                         config_.ai_api_base,
                         config_.ai_api_key_env});
    return reranker.rerank(filtered,
                           {prefix, previous_word_, last_word_, sentence_start_, false});
}

std::vector<Candidate> Predictor::filter_candidates(const std::string& prefix,
                                                    std::vector<Candidate> candidates) const {
    if (candidates.empty()) {
        return candidates;
    }

    const int best_score = candidates.front().score;
    const int min_score = std::max(10, best_score / 8);
    const std::string folded_prefix = fold_for_matching(prefix);

    // If the buffer itself is a valid dictionary word, require a higher bar
    // for extension candidates. Prevents showing "wifehood" when you type "wife".
    const bool buffer_is_complete_word = dictionary_.trie().contains(prefix);
    const int extension_floor = buffer_is_complete_word
        ? config_.complete_word_extension_min_score
        : config_.min_candidate_score;

    std::vector<Candidate> filtered;
    filtered.reserve(candidates.size());

    for (auto& candidate : candidates) {
        const std::string folded_word = fold_for_matching(candidate.word);
        if (!folded_prefix.empty() && folded_word.rfind(folded_prefix, 0) != 0) {
            continue;
        }

        const bool is_contraction = candidate.word.find('\'') != std::string::npos;

        if (candidate.score < min_score && !is_contraction) {
            continue;
        }

        // Absolute floor — no rare/noise words with trivial scores.
        if (candidate.score < extension_floor && !is_contraction) {
            continue;
        }

        if (is_contraction && !last_word_.empty()) {
            candidate.score += 120;
        }

        candidate.score += phrase_bonus(candidate.word);
        candidate.score += english_context_bonus(candidate.word);

        if (!last_word_.empty() && to_lower_ascii(candidate.word) == to_lower_ascii(last_word_)) {
            candidate.score -= 180;
        }

        if (candidate.word == "more" && to_lower_ascii(last_word_) == "more") {
            candidate.score -= 250;
        }

        // Penalty rules (negative scores) are now part of the phrase bonus index,
        // so they've already been applied via phrase_bonus() above.

        filtered.push_back(candidate);
    }

    // Previously this fell back to the unfiltered list when nothing passed —
    // that's what surfaced noise like "wifehood". Now we honor the filter:
    // if nothing is good enough, show nothing.

    std::sort(filtered.begin(), filtered.end());
    if (filtered.size() > static_cast<size_t>(config_.max_candidates)) {
        filtered.resize(config_.max_candidates);
    }
    return filtered;
}

int Predictor::phrase_bonus(const std::string& candidate) const {
    const std::string prev = to_lower_ascii(last_word_);
    const std::string cand = to_lower_ascii(candidate);

    // O(1) hash map lookups for both rule sets.
    return lookup_phrase_bonus(prev, cand) + lookup_pair_score(prev, cand);
}

int Predictor::english_context_bonus(const std::string& candidate) const {
    const std::string prev2 = to_lower_ascii(previous_word_);
    const std::string prev1 = to_lower_ascii(last_word_);
    const std::string cand = to_lower_ascii(candidate);

    // O(1) hash map lookups for both rule sets.
    return lookup_context_bonus(prev2, prev1, cand) + lookup_triple_score(prev2, prev1, cand);
}

std::vector<Candidate> Predictor::typo_correction_predictions(const std::string& prefix,
                                                              int max_results) const {
    if (preferred_language(prefix) != Language::English) {
        return {};
    }

    const auto& typo_map = english_typo_rules();

    const std::string folded_prefix = fold_for_matching(prefix);
    std::vector<Candidate> results;
    results.reserve(typo_map.size());

    for (const auto& [typo, correction] : typo_map) {
        if (folded_prefix.empty()) {
            continue;
        }

        if (typo.rfind(folded_prefix, 0) != 0) {
            continue;
        }

        int score = 600 - static_cast<int>(typo.size() * 10);
        if (!last_word_.empty() && correction == "miss" && to_lower_ascii(last_word_) == "i") {
            score += 120;
        }
        if (!last_word_.empty() && correction == "wanna") {
            score += 40;
        }
        if (!last_word_.empty() && correction == "tomorrow") {
            score += 30;
        }
        if (!last_word_.empty() && (correction == "sorry" || correction == "thanks")) {
            score += 20;
        }

        results.push_back({correction, score});
    }

    std::sort(results.begin(), results.end());
    if (results.size() > static_cast<size_t>(max_results)) {
        results.resize(max_results);
    }
    return results;
}

std::vector<Candidate> Predictor::merge_context_predictions(int max_results) const {
    std::unordered_map<std::string, Candidate> merged;

    auto absorb = [&merged](const std::vector<NgramCandidate>& source, int weight) {
        for (const auto& candidate : source) {
            std::string key = to_lower_ascii(candidate.word);
            auto& slot = merged[key];
            if (slot.word.empty()) {
                slot.word = candidate.word;
            }
            slot.score += candidate.score * weight;
        }
    };

    if (!last_word_.empty()) {
        absorb(system_ngram_.predict_next(last_word_, max_results * 2), 12);
        absorb(user_ngram_.predict_next(last_word_, max_results * 2), 6);
    }

    if (!previous_word_.empty()) {
        absorb(system_ngram_.predict_next(previous_word_, max_results), 2);
        absorb(user_ngram_.predict_next(previous_word_, max_results), 4);
    }

    std::vector<Candidate> results;
    results.reserve(merged.size());
    for (const auto& [_, candidate] : merged) {
        results.push_back(candidate);
    }

    std::sort(results.begin(), results.end());
    if (results.size() > static_cast<size_t>(max_results)) {
        results.resize(max_results);
    }

    return results;
}

std::vector<Candidate> Predictor::predict_next_word() const {
    std::unordered_map<std::string, Candidate> merged_results;
    const std::string last_word_lower = to_lower_ascii(last_word_);

    if (!previous_word_.empty() && !last_word_.empty()) {
        auto phrases = phrase_model_.predict(previous_word_, last_word_, 3);
        for (const auto& phrase : phrases) {
            auto tokens = split_text_tokens(phrase.completion);
            if (tokens.empty()) {
                continue;
            }

            merge_candidate(merged_results, {phrase.completion, phrase.score + 99000});
        }
    }

    auto context = merge_context_predictions(config_.max_candidates);
    for (const auto& c : context) {
        if (!last_word_lower.empty() && to_lower_ascii(c.word) == last_word_lower) {
            continue;
        }
        merge_candidate(merged_results, c);
    }

    std::vector<Candidate> results;
    results.reserve(merged_results.size());
    for (const auto& [_, candidate] : merged_results) {
        results.push_back(candidate);
    }

    if (results.empty()) {
        results = sentence_starter_predictions("", config_.max_candidates);
    }

    std::sort(results.begin(), results.end());
    if (results.size() > static_cast<size_t>(config_.max_candidates)) {
        results.resize(config_.max_candidates);
    }

    if (sentence_start_) {
        for (auto& candidate : results) {
            candidate.word = capitalize_for_display(candidate.word);
        }
    }

    AiReranker reranker({config_.ai_rerank_enabled,
                         config_.ai_smart_fallback,
                         config_.ai_debug_logging,
                         config_.ai_model,
                         config_.ai_timeout_ms,
                         config_.ai_uncertainty_gap_threshold,
                         static_cast<size_t>(std::max(1, config_.ai_max_cache_entries)),
                         config_.ai_api_base,
                         config_.ai_api_key_env});
    return reranker.rerank(results,
                           {"", previous_word_, last_word_, sentence_start_, true});
}

std::vector<Candidate> Predictor::predict_emoji(const std::string& prefix) const {
    auto emojis = emoji_model_.predict(prefix, config_.max_candidates);
    std::vector<Candidate> results;
    for (const auto& e : emojis) {
        // Show "emoji shortcode" in candidate list
        results.push_back({e.emoji + " " + e.shortcode, e.score});
    }
    return results;
}

void Predictor::on_word_accepted(const std::string& word) {
    register_completed_word(word, true);
}

void Predictor::on_text_accepted(const std::string& text) {
    for (const auto& token : split_text_tokens(text)) {
        register_completed_word(token, false);
    }
}

void Predictor::learn_word(const std::string& word) {
    if (word.length() >= 2 && !dictionary_.trie().contains(word)) {
        dictionary_.trie().insert(word, 5);
        dictionary_.record_user_word(word, 1);
    }
}

void Predictor::replace_current_word(const std::string& word) {
    register_completed_word(word, false);
}

void Predictor::replace_last_committed_and_current(const std::string& word) {
    last_word_ = previous_word_;
    register_completed_word(word, false);
}

void Predictor::on_word_boundary() {
    register_completed_word(buffer_, false);
}

void Predictor::on_sentence_boundary() {
    buffer_.clear();
    previous_word_.clear();
    last_word_.clear();
    sentence_start_ = true;
}

void Predictor::save() {
    if (!config_.user_dict_path.empty()) {
        dictionary_.save_user_dict(config_.user_dict_path);
    }
    if (!user_ngram_path_.empty()) {
        user_ngram_.save(user_ngram_path_);
    }
    save_user_freq();
}

void Predictor::load_user_freq() {
    if (user_freq_path_.empty()) return;
    std::ifstream file(user_freq_path_);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string word;
        int freq;
        if (iss >> word >> freq) {
            user_freq_[word] = freq;
        }
    }
    std::cout << "[LinuxComplete] Loaded " << user_freq_.size() << " user freq entries" << std::endl;
}

void Predictor::save_user_freq() const {
    if (user_freq_path_.empty() || user_freq_.empty()) return;

    auto parent = std::filesystem::path(user_freq_path_).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(user_freq_path_);
    if (!file.is_open()) return;

    // Sort by frequency descending — most used first
    std::vector<std::pair<std::string, int>> sorted_freq(user_freq_.begin(), user_freq_.end());
    std::sort(sorted_freq.begin(), sorted_freq.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& [word, freq] : sorted_freq) {
        file << word << " " << freq << "\n";
    }
}

void Predictor::push_char(const std::string& ch) {
    buffer_ += ch;
}

void Predictor::pop_char() {
    if (!buffer_.empty()) {
        size_t i = buffer_.size() - 1;
        while (i > 0 && (buffer_[i] & 0xC0) == 0x80) {
            i--;
        }
        buffer_.erase(i);
    }
}

void Predictor::clear_buffer() {
    buffer_.clear();
}

bool Predictor::should_predict() const {
    return utf8_codepoint_length(buffer_) >= static_cast<size_t>(config_.min_prefix_length);
}

bool Predictor::is_program_disabled(const std::string& program) const {
    if (program.empty()) return false;
    // Case-insensitive match: exact match, or pattern is a prefix of program name.
    // Prefix match catches custom window class variants like "kittyfloat", "kittyterm", etc.
    const std::string prog_lower = text_utils::to_lower_ascii(program);
    for (const auto& p : config_.disabled_programs) {
        const std::string pat = text_utils::to_lower_ascii(p);
        if (pat.empty()) continue;
        if (pat == prog_lower) return true;
        if (prog_lower.rfind(pat, 0) == 0) return true; // starts with pattern
    }
    return false;
}

bool Predictor::can_predict_next() const {
    return buffer_.empty() && (sentence_start_ || !last_word_.empty());
}

void Predictor::register_completed_word(const std::string& word, bool boost_trie) {
    if (word.empty()) {
        buffer_.clear();
        return;
    }

    if (boost_trie) {
        dictionary_.trie().boost(word, 1);
    }

    std::string lower = to_lower_ascii(word);
    ++user_freq_[lower];

    if (!last_word_.empty()) {
        user_ngram_.learn(last_word_, word);
    }

    previous_word_ = last_word_;
    last_word_ = word;
    buffer_.clear();
    sentence_start_ = false;
}

} // namespace linuxcomplete
