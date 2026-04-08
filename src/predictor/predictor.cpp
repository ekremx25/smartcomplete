#include "predictor/predictor.h"
#include "predictor/ai_reranker.h"
#include "predictor/grammar_rules.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>

namespace linuxcomplete {

namespace {

struct PairRule {
    const char* prev;
    const char* cand;
    int score;
};

struct TripleRule {
    const char* prev2;
    const char* prev1;
    const char* cand;
    int score;
};

bool is_one_of(const std::string& value, std::initializer_list<const char*> options) {
    for (const char* option : options) {
        if (value == option) {
            return true;
        }
    }
    return false;
}

int lookup_pair_rule(const std::vector<PairRule>& rules,
                     const std::string& prev,
                     const std::string& cand) {
    for (const auto& rule : rules) {
        if (prev == rule.prev && cand == rule.cand) {
            return rule.score;
        }
    }
    return 0;
}

int lookup_triple_rule(const std::vector<TripleRule>& rules,
                       const std::string& prev2,
                       const std::string& prev1,
                       const std::string& cand) {
    for (const auto& rule : rules) {
        if (prev2 == rule.prev2 && prev1 == rule.prev1 && cand == rule.cand) {
            return rule.score;
        }
    }
    return 0;
}

const std::vector<PairRule>& english_phrase_rules() {
    static const std::vector<PairRule> rules = {
        {"miss", "you", 220}, {"love", "you", 260}, {"love", "watching", 240},
        {"love", "to", 210}, {"like", "watching", 240}, {"like", "to", 210},
        {"enjoy", "watching", 250}, {"a", "lot", 220}, {"a", "new", 140},
        {"a", "little", 140}, {"an", "apple", 130}, {"an", "idea", 140},
        {"an", "honest", 130}, {"an", "amazing", 150}, {"an", "important", 150},
        {"a", "beautiful", 120}, {"a", "good", 110}, {"start", "to", 190},
        {"begin", "to", 190}, {"try", "to", 210}, {"if", "you", 170},
        {"if", "i", 160}, {"when", "you", 170}, {"when", "i", 160},
        {"what", "are", 180}, {"what", "is", 170}, {"how", "are", 220},
        {"how", "it", 220}, {"how", "this", 200}, {"how", "to", 210},
        {"where", "are", 210}, {"why", "are", 180}, {"why", "did", 170},
        {"because", "you", 170}, {"because", "it", 150}, {"so", "i", 170},
        {"so", "you", 150}, {"but", "i", 160}, {"and", "i", 150},
        {"really", "love", 170}, {"really", "miss", 170}, {"really", "need", 160},
        {"very", "much", 180}, {"so", "much", 180}, {"feel", "like", 180},
        {"feel", "better", 130}, {"feel", "free", 130}, {"kind", "of", 150},
        {"sort", "of", 150}, {"you", "more", -140}, {"you", "so", 140},
        {"you", "too", 150}, {"love", "us", -320}, {"my", "love", 280},
        {"my", "life", 280}, {"my", "wife", -120}, {"be", "there", 180},
        {"next", "month", 220}, {"because", "i", 170}, {"because", "we", 170},
        {"i", "will", 160}, {"i", "need", 170}, {"i", "miss", 220},
        {"i", "love", 190}, {"i", "like", 180}, {"i", "enjoy", 170},
        {"i", "am", 220}, {"i", "i'm", -400}, {"i", "hope", 140},
        {"i", "have", 160}, {"i", "was", 140}, {"i", "stayed", 150},
        {"i", "lived", 150}, {"i", "went", 150}, {"i", "don't", 150},
        {"am", "your", 230}, {"are", "your", 200}, {"you", "are", 220},
        {"you", "have", 180}, {"you", "need", 150}, {"are", "you", 220},
        {"did", "you", 180}, {"do", "you", 170}, {"can", "you", 170},
        {"could", "you", 220}, {"would", "you", 210}, {"may", "i", 220},
        {"my", "question", -140},
        {"we", "are", 210}, {"we", "have", 170}, {"they", "are", 210},
        {"they", "have", 170}, {"he", "is", 220}, {"he", "has", 180},
        {"she", "is", 220}, {"she", "has", 180}, {"it", "is", 220},
        {"it", "has", 170}, {"it", "works", 240}, {"it", "working", -220},
        {"this", "is", 210}, {"these", "are", 210}, {"those", "are", 210},
        {"to", "watch", 220}, {"to", "be", 170}, {"to", "go", 160},
        {"to", "see", 170}, {"to", "do", 150}, {"to", "make", 150},
        {"to", "have", 150}, {"to", "use", 190}, {"to", "fix", 180},
        {"want", "to", 200}, {"need", "to", 210}, {"have", "to", 200},
        {"wanna", "be", 140}, {"don't", "wanna", 120}, {"don't", "know", 200},
        {"don't", "think", 180}, {"can't", "wait", 210},
        {"can't", "believe", 180}, {"should", "be", 180},
        {"should", "have", 170}, {"could", "be", 170}, {"would", "be", 180},
        {"will", "be", 190}, {"will", "have", 170}, {"see", "you", 210},
        {"thank", "you", 210}, {"talk", "later", 180}, {"good", "morning", 200},
        {"good", "night", 200}, {"good", "afternoon", 170},
        {"good", "evening", 170}, {"take", "care", 220}, {"on", "my", 170},
        {"my", "way", 210}, {"i'm", "your", 260}, {"i'm", "you're", -320},
        {"your", "husband", 240}, {"your", "wife", 240}, {"your", "love", 180},
        {"your", "life", 160}, {"yours", "husband", -260},
        {"yours", "wife", -260}, {"right", "now", 170}, {"let", "me", 160},
        {"give", "you", 150}, {"please", "help", 220}, {"please", "tell", 170},
        {"please", "let", 170}, {"would", "like", 200}, {"mind", "if", 180},
        {"ask", "you", 180}, {"ask", "a", 220}, {"a", "question", 260},
        {"a", "questions", -280}, {"we", "love", 190}, {"we", "need", 150},
        {"we", "can", 130}, {"in", "the", 180}, {"on", "the", 170},
        {"at", "the", 170}, {"for", "you", 180}, {"with", "you", 180},
        {"about", "you", 150}, {"from", "you", 140}, {"go", "to", 190},
        {"come", "back", 170}, {"look", "at", 180}, {"listen", "to", 190},
        {"was", "in", 230}, {"were", "in", 220}, {"stayed", "in", 240},
        {"lived", "in", 240}, {"went", "to", 240}, {"last", "year", 260},
        {"last", "night", 220}, {"last", "week", 220}, {"last", "month", 220},
        {"wait", "for", 180}, {"afraid", "of", 170},
        {"interested", "in", 170}, {"different", "from", 160},
        {"married", "to", 190}, {"love", "my", 160}, {"more", "than", 170},
        {"there", "is", 180}, {"there", "are", 180}, {"their", "house", 170},
        {"their", "love", 140}, {"their", "life", 130}, {"they're", "here", 170},
        {"they're", "coming", 170}, {"watching", "supernatural", 220},
        {"watch", "supernatural", 180}
    };
    return rules;
}

const std::vector<TripleRule>& english_context_rules() {
    static const std::vector<TripleRule> rules = {
        {"i", "don't", "know", 260}, {"i", "don't", "think", 240},
        {"i", "would", "like", 240}, {"i", "want", "to", 240},
        {"i", "need", "to", 250}, {"i", "have", "to", 220},
        {"i", "am", "here", 180}, {"i", "am", "sure", 170},
        {"i", "am", "happy", 170}, {"i", "am", "your", 230},
        {"i", "was", "in", 280}, {"i", "stayed", "in", 280},
        {"i", "lived", "in", 280}, {"i", "went", "to", 280},
        {"i", "love", "you", 280}, {"i", "love", "watching", 260},
        {"i", "love", "to", 240}, {"i", "like", "watching", 270},
        {"i", "like", "to", 240}, {"i", "like", "watch", -120},
        {"i", "miss", "you", 300}, {"i", "feel", "like", 250},
        {"i", "feel", "you're", -220}, {"feel", "like", "you", 220},
        {"feel", "like", "i", 190}, {"feel", "like", "this", 170},
        {"i", "hope", "you", 210}, {"i", "wish", "you", 200},
        {"i", "can't", "wait", 260}, {"thank", "you", "so", 220},
        {"thank", "you", "for", 200}, {"thank", "you", "very", 220},
        {"thank", "you", "my", -180}, {"how", "are", "you", 320},
        {"how", "it", "works", 340}, {"how", "it", "is", 170},
        {"how", "it", "working", -360}, {"how", "this", "works", 320},
        {"how", "to", "use", 300}, {"how", "to", "do", 260},
        {"how", "to", "fix", 280}, {"how", "to", "make", 260},
        {"how", "working", "its", -520}, {"how", "working", "it's", -520},
        {"where", "are", "you", 300}, {"what", "are", "you", 300},
        {"what", "is", "your", 220}, {"why", "are", "you", 260},
        {"why", "did", "you", 250}, {"can", "you", "help", 210},
        {"could", "you", "please", 320}, {"would", "you", "mind", 300},
        {"may", "i", "ask", 320}, {"can", "you", "please", 250},
        {"i", "would", "like", 300}, {"would", "like", "to", 280},
        {"do", "you", "know", 260}, {"you", "know", "how", 230},
        {"know", "how", "it", 260}, {"are", "you", "okay", 220},
        {"are", "you", "free", 200}, {"are", "you", "there", 190},
        {"please", "help", "me", 260}, {"may", "i", "ask", 320},
        {"my", "i", "ask", -320},
        {"ask", "you", "something", 220}, {"ask", "a", "question", 320},
        {"ask", "a", "questions", -360},
        {"ask", "a", "quesadilla", -420},
        {"see", "you", "soon", 220}, {"see", "you", "later", 220},
        {"miss", "you", "so", 210}, {"miss", "you", "too", 190},
        {"talk", "to", "you", 200}, {"looking", "for", "you", 170},
        {"looking", "at", "you", 170}, {"a", "lot", "of", 260},
        {"a", "lot", "the", -180}, {"one", "of", "the", 240},
        {"there", "is", "a", 220}, {"there", "is", "no", 210},
        {"there", "is", "are", -320}, {"there", "are", "many", 220},
        {"there", "are", "some", 200}, {"there", "are", "is", -320},
        {"in", "manila", "last", 180}, {"in", "tokyo", "last", 180},
        {"in", "london", "last", 180}, {"in", "paris", "last", 180},
        {"because", "i", "love", 180}, {"because", "you", "are", 190},
        {"if", "you", "want", 180}, {"if", "you", "need", 180},
        {"when", "i", "see", 180}, {"when", "you", "come", 180},
        {"going", "to", "be", 240}, {"want", "to", "be", 220},
        {"need", "to", "be", 210}, {"need", "to", "go", 210},
        {"have", "to", "go", 210}, {"have", "to", "be", 200},
        {"have", "to", "do", 180}, {"want", "to", "see", 190},
        {"want", "to", "go", 180}, {"i'm", "your", "husband", 300},
        {"i'm", "your", "wife", 300}, {"you", "are", "my", 210},
        {"you", "are", "right", 180}, {"my", "love", "forever", 210},
        {"my", "life", "forever", 190}, {"love", "you", "more", 170},
        {"love", "you", "so", 220}, {"love", "you", "my", -180},
        {"very", "much", "for", 140}, {"so", "much", "for", 140},
        {"good", "morning", "my", 160}, {"good", "night", "my", 150},
        {"as", "soon", "as", 230}, {"in", "the", "morning", 180},
        {"at", "the", "moment", 180}, {"what's", "going", "on", 340},
        {"going", "on", "with", 170}, {"i", "am", "i'm", -400},
        {"i", "i'm", "am", -400}, {"your", "husband", "are", -220},
        {"i", "love", "us", -220}, {"i", "was", "stay", -420},
        {"was", "stay", "last", -420}
    };
    return rules;
}

int english_filter_penalty(const std::string& prev,
                           const std::string& cand,
                           Language prefix_lang) {
    if (prev == "love" && cand == "us") return -420;
    if (prev == "us" && cand == "my") return -260;
    if (prev == "because" && cand == "us") return -180;
    if (prev == "we" && cand == "us") return -200;
    if (prev == "my" && cand == "wife" && prefix_lang == Language::English) return -420;
    if (prev == "you" && cand == "more") return -140;
    if (prev == "more" && cand == "my") return -170;
    if (prev == "yours" && is_one_of(cand, {"husband", "wife", "love", "life"})) return -320;
    if ((prev == "i" && cand == "i'm") || (prev == "i'm" && cand == "am")) return -500;
    if (prev == "i'm" && cand == "you're") return -420;
    if ((prev == "your" || prev == "you're") && is_one_of(cand, {"husband", "wife"})) {
        return prev == "your" ? 140 : -260;
    }
    if (prev == "feel" && is_one_of(cand, {"you're", "you", "he", "she", "they"})) return -180;
    if (prev == "how" && cand == "working") return -520;
    if (prev == "it" && cand == "working") return -260;
    if (prev == "a" && is_one_of(cand, {"apple", "idea", "honest", "amazing", "important"})) return -220;
    if (prev == "an" && is_one_of(cand, {"good", "beautiful", "new", "little"})) return -220;
    if (prev == "their" && is_one_of(cand, {"is", "are"})) return -260;
    if (prev == "there" && is_one_of(cand, {"love", "husband", "wife"})) return -220;
    return 0;
}

} // namespace

Predictor::Predictor(const PredictorConfig& config) : config_(config) {}
Predictor::~Predictor() { save(); }

std::string Predictor::to_lower_ascii(std::string word) {
    for (auto& c : word) {
        if (c >= 'A' && c <= 'Z') {
            c += 32;
        }
    }
    return word;
}

std::string Predictor::capitalize_for_display(const std::string& word) {
    if (word.empty()) {
        return word;
    }

    Language lang = Dictionary::detect_language(word);

    if (word[0] >= 'a' && word[0] <= 'z') {
        std::string result = word;
        if (lang == Language::Turkish && result[0] == 'i') {
            result.replace(0, 1, "\xC4\xB0");
        } else {
            result[0] = static_cast<char>(result[0] - 32);
        }
        return result;
    }

    if (word.rfind("\xC3\xA7", 0) == 0) return "\xC3\x87" + word.substr(2);
    if (word.rfind("\xC4\x9F", 0) == 0) return "\xC4\x9E" + word.substr(2);
    if (word.rfind("\xC4\xB1", 0) == 0) return "I" + word.substr(2);
    if (word.rfind("\xC3\xB6", 0) == 0) return "\xC3\x96" + word.substr(2);
    if (word.rfind("\xC5\x9F", 0) == 0) return "\xC5\x9E" + word.substr(2);
    if (word.rfind("\xC3\xBC", 0) == 0) return "\xC3\x9C" + word.substr(2);

    return word;
}

std::string Predictor::fold_for_matching(std::string text) {
    text = to_lower_ascii(std::move(text));
    text.erase(std::remove(text.begin(), text.end(), '\''), text.end());
    return text;
}

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

Language Predictor::preferred_language(const std::string& prefix) const {
    if (!prefix.empty()) {
        return Dictionary::detect_language(prefix);
    }

    if (!last_word_.empty()) {
        return Dictionary::detect_language(last_word_);
    }

    if (!previous_word_.empty()) {
        return Dictionary::detect_language(previous_word_);
    }

    return Language::English;
}

std::vector<Candidate> Predictor::sentence_starter_predictions(const std::string& prefix,
                                                               int max_results) const {
    static const std::vector<std::string> english_starters = {
        "i", "the", "this", "we", "you", "it", "they", "today", "hello", "thanks",
        "actually", "honestly", "maybe", "well", "so", "because", "if", "when",
        "sorry", "please", "good", "love"
    };
    static const std::vector<std::string> turkish_starters = {
        "ben", "bu", "bir", "sen", "biz", "bugün", "merhaba", "teşekkürler", "artık", "şimdi"
    };

    const auto& starters =
        preferred_language(prefix) == Language::Turkish ? turkish_starters : english_starters;

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

    // Load Turkish dictionary
    std::string tr_dict = config_.dict_dir + "/tr_TR.txt";
    std::string tr_freq = config_.dict_dir + "/../frequency/tr_freq.txt";
    if (std::filesystem::exists(tr_dict)) {
        success |= dictionary_.load(tr_dict, tr_freq, Language::Turkish);
    }

    // Load user dictionary
    if (!config_.user_dict_path.empty()) {
        dictionary_.load_user_dict(config_.user_dict_path);
    }

    // Load ngram data
    std::string ngram_dir = config_.dict_dir + "/../ngram";
    std::string rules_dir = config_.dict_dir + "/../rules";
    std::string en_bigrams = ngram_dir + "/en_bigrams.txt";
    std::string tr_bigrams = ngram_dir + "/tr_bigrams.txt";

    if (std::filesystem::exists(rules_dir)) {
        set_rules_data_dir(rules_dir);
    }

    if (std::filesystem::exists(en_bigrams)) {
        system_ngram_.load(en_bigrams);
    }
    if (std::filesystem::exists(tr_bigrams)) {
        system_ngram_.load(tr_bigrams);
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

    // Load user word frequencies
    load_user_freq();

    std::cout << "[LinuxComplete] Predictor initialized with "
              << dictionary_.trie().size() << " words, "
              << (system_ngram_.size() + user_ngram_.size()) << " bigrams, "
              << phrase_model_.size() << " phrases, "
              << user_freq_.size() << " user freq entries" << std::endl;

    return success;
}

std::vector<Candidate> Predictor::predict(const std::string& prefix) const {
    if (prefix.length() < static_cast<size_t>(config_.min_prefix_length)) {
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
                         static_cast<size_t>(std::max(1, config_.ai_max_cache_entries))});
    return reranker.rerank(filtered,
                           {prefix, previous_word_, last_word_, sentence_start_, false});
}

std::vector<Candidate> Predictor::filter_candidates(const std::string& prefix,
                                                    std::vector<Candidate> candidates) const {
    if (candidates.empty()) {
        return candidates;
    }

    const Language prefix_lang = preferred_language(prefix);
    const int best_score = candidates.front().score;
    const int min_score = std::max(10, best_score / 8);
    const std::string folded_prefix = fold_for_matching(prefix);

    std::vector<Candidate> filtered;
    filtered.reserve(candidates.size());

    for (auto& candidate : candidates) {
        const std::string folded_word = fold_for_matching(candidate.word);
        if (!folded_prefix.empty() && folded_word.rfind(folded_prefix, 0) != 0) {
            continue;
        }

        const bool is_contraction = candidate.word.find('\'') != std::string::npos;
        const Language candidate_lang = Dictionary::detect_language(candidate.word);

        if (prefix_lang == Language::Turkish && candidate_lang == Language::English && !is_contraction) {
            continue;
        }

        if (prefix_lang == Language::English && candidate_lang == Language::Turkish) {
            continue;
        }

        if (candidate.score < min_score && !is_contraction) {
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

        const std::string prev = to_lower_ascii(last_word_);
        const std::string cand = to_lower_ascii(candidate.word);
        candidate.score += english_filter_penalty(prev, cand, prefix_lang);

        filtered.push_back(candidate);
    }

    if (filtered.empty()) {
        filtered = std::move(candidates);
    }

    std::sort(filtered.begin(), filtered.end());
    if (filtered.size() > static_cast<size_t>(config_.max_candidates)) {
        filtered.resize(config_.max_candidates);
    }
    return filtered;
}

int Predictor::phrase_bonus(const std::string& candidate) const {
    const std::string prev = to_lower_ascii(last_word_);
    const std::string cand = to_lower_ascii(candidate);

    // Check all rule sets
    int score = lookup_pair_rule(english_phrase_rules(), prev, cand);

    // Additional English grammar rules
    for (const auto& rule : english_grammar_rules()) {
        if (prev == rule.prev && cand == rule.cand) {
            score += rule.score;
        }
    }

    // Turkish phrase rules
    for (const auto& rule : turkish_phrase_rules()) {
        if (prev == rule.prev && cand == rule.cand) {
            score += rule.score;
        }
    }

    return score;
}

int Predictor::english_context_bonus(const std::string& candidate) const {
    const std::string prev2 = to_lower_ascii(previous_word_);
    const std::string prev1 = to_lower_ascii(last_word_);
    const std::string cand = to_lower_ascii(candidate);

    int score = 0;

    // English triple rules
    score += lookup_triple_rule(english_context_rules(), prev2, prev1, cand);

    // Additional English grammar triple rules
    for (const auto& rule : english_grammar_triple_rules()) {
        if (prev2 == rule.prev2 && prev1 == rule.prev1 && cand == rule.cand) {
            score += rule.score;
        }
    }

    // Turkish triple rules
    for (const auto& rule : turkish_context_rules()) {
        if (prev2 == rule.prev2 && prev1 == rule.prev1 && cand == rule.cand) {
            score += rule.score;
        }
    }

    return score;
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
    const Language preferred_lang = preferred_language("");
    const std::string last_word_lower = to_lower_ascii(last_word_);

    if (!previous_word_.empty() && !last_word_.empty()) {
        auto phrases = phrase_model_.predict(previous_word_, last_word_, 3);
        for (const auto& phrase : phrases) {
            auto tokens = split_text_tokens(phrase.completion);
            if (tokens.empty()) {
                continue;
            }

            const Language candidate_lang = Dictionary::detect_language(tokens.front());
            if (preferred_lang == Language::Turkish && candidate_lang == Language::English) {
                continue;
            }
            if (preferred_lang == Language::English && candidate_lang == Language::Turkish) {
                continue;
            }

            merge_candidate(merged_results, {phrase.completion, phrase.score + 99000});
        }
    }

    auto context = merge_context_predictions(config_.max_candidates);
    for (const auto& c : context) {
        const Language candidate_lang = Dictionary::detect_language(c.word);
        if (preferred_lang == Language::Turkish && candidate_lang == Language::English) {
            continue;
        }
        if (preferred_lang == Language::English && candidate_lang == Language::Turkish) {
            continue;
        }
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
                         static_cast<size_t>(std::max(1, config_.ai_max_cache_entries))});
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
    return buffer_.length() >= static_cast<size_t>(config_.min_prefix_length);
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
