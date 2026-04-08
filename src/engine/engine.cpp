#include "engine/engine.h"
#include <fcitx-utils/utf8.h>
#include <fcitx/candidatelist.h>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

namespace linuxcomplete {

namespace {

void load_predictor_config_from_file(const std::filesystem::path& path, PredictorConfig& config) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return;
    }

    nlohmann::json parsed = nlohmann::json::parse(file, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return;
    }

    if (parsed.contains("max_candidates") && parsed["max_candidates"].is_number_integer()) {
        config.max_candidates = parsed["max_candidates"].get<int>();
    }
    if (parsed.contains("min_prefix_length") && parsed["min_prefix_length"].is_number_integer()) {
        config.min_prefix_length = parsed["min_prefix_length"].get<int>();
    }
    if (parsed.contains("auto_detect_language") && parsed["auto_detect_language"].is_boolean()) {
        config.auto_detect_language = parsed["auto_detect_language"].get<bool>();
    }
    if (parsed.contains("learn_new_words") && parsed["learn_new_words"].is_boolean()) {
        config.learn_new_words = parsed["learn_new_words"].get<bool>();
    }
    if (parsed.contains("ai_rerank_enabled") && parsed["ai_rerank_enabled"].is_boolean()) {
        config.ai_rerank_enabled = parsed["ai_rerank_enabled"].get<bool>();
    }
    if (parsed.contains("ai_smart_fallback") && parsed["ai_smart_fallback"].is_boolean()) {
        config.ai_smart_fallback = parsed["ai_smart_fallback"].get<bool>();
    }
    if (parsed.contains("ai_debug_logging") && parsed["ai_debug_logging"].is_boolean()) {
        config.ai_debug_logging = parsed["ai_debug_logging"].get<bool>();
    }
    if (parsed.contains("ai_model") && parsed["ai_model"].is_string()) {
        config.ai_model = parsed["ai_model"].get<std::string>();
    }
    if (parsed.contains("ai_timeout_ms") && parsed["ai_timeout_ms"].is_number_integer()) {
        config.ai_timeout_ms = parsed["ai_timeout_ms"].get<int>();
    }
    if (parsed.contains("ai_uncertainty_gap_threshold") &&
        parsed["ai_uncertainty_gap_threshold"].is_number_integer()) {
        config.ai_uncertainty_gap_threshold = parsed["ai_uncertainty_gap_threshold"].get<int>();
    }
    if (parsed.contains("ai_max_cache_entries") &&
        parsed["ai_max_cache_entries"].is_number_integer()) {
        config.ai_max_cache_entries = parsed["ai_max_cache_entries"].get<int>();
    }
}

void apply_predictor_env_overrides(PredictorConfig& config) {
    if (const char* enabled = std::getenv("LINUXCOMPLETE_AI_ENABLED")) {
        std::string value(enabled);
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        config.ai_rerank_enabled = (value == "1" || value == "true" || value == "yes" ||
                                    value == "on");
    }

    if (const char* model = std::getenv("LINUXCOMPLETE_AI_MODEL")) {
        config.ai_model = model;
    }

    if (const char* timeout = std::getenv("LINUXCOMPLETE_AI_TIMEOUT_MS")) {
        try {
            config.ai_timeout_ms = std::stoi(timeout);
        } catch (...) {
        }
    }

    if (const char* smart = std::getenv("LINUXCOMPLETE_AI_SMART_FALLBACK")) {
        std::string value(smart);
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        config.ai_smart_fallback = !(value == "0" || value == "false" || value == "no" ||
                                     value == "off");
    }

    if (const char* debug = std::getenv("LINUXCOMPLETE_AI_DEBUG")) {
        std::string value(debug);
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        config.ai_debug_logging = (value == "1" || value == "true" || value == "yes" ||
                                   value == "on");
    }

    if (const char* gap = std::getenv("LINUXCOMPLETE_AI_GAP_THRESHOLD")) {
        try {
            config.ai_uncertainty_gap_threshold = std::stoi(gap);
        } catch (...) {
        }
    }

    if (const char* cache = std::getenv("LINUXCOMPLETE_AI_MAX_CACHE_ENTRIES")) {
        try {
            config.ai_max_cache_entries = std::stoi(cache);
        } catch (...) {
        }
    }
}

} // namespace

// ═══════════════════════════════════════════
// LinuxCompleteCandidate
// ═══════════════════════════════════════════

LinuxCompleteCandidate::LinuxCompleteCandidate(LinuxCompleteEngine* engine,
                                                 const Candidate& candidate)
    : fcitx::CandidateWord(fcitx::Text(candidate.word)),
      engine_(engine),
      word_(candidate.word) {}

void LinuxCompleteCandidate::select(fcitx::InputContext* ic) const {
    engine_->commit_candidate(ic, word_);
}

std::string LinuxCompleteEngine::to_lower_ascii(std::string text) {
    for (auto& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c += 32;
        }
    }
    return text;
}

std::string LinuxCompleteEngine::normalize_contraction_typo(std::string text) {
    auto lower = to_lower_ascii(text);

    static const std::unordered_map<std::string, std::string> typo_rewrites = {
        {"dont't", "don't"},
        {"cant't", "can't"},
        {"wont't", "won't"},
        {"isnt't", "isn't"},
        {"arent't", "aren't"},
        {"didnt't", "didn't"},
        {"hasnt't", "hasn't"},
        {"havent't", "haven't"},
        {"wouldnt't", "wouldn't"},
        {"shouldnt't", "shouldn't"},
        {"couldnt't", "couldn't"},
        {"wasnt't", "wasn't"},
        {"werent't", "weren't"},
        {"its's", "it's"},
        {"that's", "that's"},
        {"thats's", "that's"},
        {"there's", "there's"},
        {"theres's", "there's"},
        {"youre're", "you're"},
        {"were're", "we're"},
        {"theyre're", "they're"},
        {"i'am", "I'm"},
        {"i'v", "I've"},
        {"i'will", "I'll"}
    };

    auto it = typo_rewrites.find(lower);
    if (it != typo_rewrites.end()) {
        return it->second;
    }

    return text;
}

std::string LinuxCompleteEngine::fold_for_matching(std::string text) {
    text = to_lower_ascii(std::move(text));
    text.erase(std::remove(text.begin(), text.end(), '\''), text.end());
    return text;
}

bool LinuxCompleteEngine::is_prefix_match(const std::string& buffer, const std::string& word) {
    if (buffer.empty()) {
        return true;
    }

    const std::string folded_buffer = fold_for_matching(buffer);
    const std::string folded_word = fold_for_matching(word);
    return folded_word.rfind(folded_buffer, 0) == 0;
}

std::string LinuxCompleteEngine::match_case_to_buffer(const std::string& buffer, std::string word) {
    if (buffer.empty() || word.empty()) {
        return word;
    }

    const unsigned char first = static_cast<unsigned char>(buffer[0]);
    if (first >= 'a' && first <= 'z' && word[0] >= 'A' && word[0] <= 'Z') {
        word[0] = static_cast<char>(word[0] + 32);
    }

    return word;
}

// ═══════════════════════════════════════════
// LinuxCompleteEngine
// ═══════════════════════════════════════════

LinuxCompleteEngine::LinuxCompleteEngine(fcitx::Instance* instance)
    : instance_(instance) {

    PredictorConfig config;

    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "";
    std::string config_dir = home + "/.local/share/linuxcomplete";
    std::filesystem::path user_config_path = std::filesystem::path(home) / ".config" /
                                             "linuxcomplete" / "linuxcomplete.conf";
    std::filesystem::path system_config_path = std::filesystem::path(CONFIG_INSTALL_DIR) /
                                               "linuxcomplete.conf";
    std::string system_dict_dir = DICT_INSTALL_DIR;

    load_predictor_config_from_file(system_config_path, config);
    if (std::filesystem::exists(user_config_path)) {
        load_predictor_config_from_file(user_config_path, config);
    }

    if (std::filesystem::exists(config_dir + "/dict")) {
        config.dict_dir = config_dir + "/dict";
    } else if (std::filesystem::exists(system_dict_dir)) {
        config.dict_dir = system_dict_dir;
    }

    config.user_dict_path = config_dir + "/user/learned.txt";
    apply_predictor_env_overrides(config);

    predictor_ = std::make_unique<Predictor>(config);
    predictor_->init();

    std::cout << "[LinuxComplete] Engine initialized" << std::endl;
}

LinuxCompleteEngine::~LinuxCompleteEngine() {
    if (predictor_) {
        predictor_->save();
    }
}

std::vector<fcitx::InputMethodEntry> LinuxCompleteEngine::listInputMethods() {
    std::vector<fcitx::InputMethodEntry> result;
    result.emplace_back(std::move(
        fcitx::InputMethodEntry("linuxcomplete", "LinuxComplete", "*", "linuxcomplete")
            .setIcon("input-keyboard")
            .setLabel("LC")
    ));
    return result;
}

void LinuxCompleteEngine::activate(const fcitx::InputMethodEntry& /*entry*/,
                                    fcitx::InputContextEvent& /*event*/) {
    if (predictor_) {
        predictor_->clear_buffer();
    }
    current_candidates_.clear();
    selected_index_ = -1;
}

void LinuxCompleteEngine::deactivate(const fcitx::InputMethodEntry& /*entry*/,
                                      fcitx::InputContextEvent& event) {
    auto* ic = event.inputContext();
    if (predictor_) {
        predictor_->clear_buffer();
    }
    current_candidates_.clear();
    clear_state(ic);
}

void LinuxCompleteEngine::reset(const fcitx::InputMethodEntry& /*entry*/,
                                 fcitx::InputContextEvent& event) {
    auto* ic = event.inputContext();
    if (predictor_) {
        predictor_->clear_buffer();
    }
    current_candidates_.clear();
    clear_state(ic);
}

void LinuxCompleteEngine::keyEvent(const fcitx::InputMethodEntry& /*entry*/,
                                    fcitx::KeyEvent& event) {
    auto* ic = event.inputContext();
    auto key = event.key();

    if (event.isRelease()) return;

    auto selected_candidate_index = [this]() {
        if (current_candidates_.empty()) {
            return -1;
        }
        if (selected_index_ >= 0 &&
            selected_index_ < static_cast<int>(current_candidates_.size())) {
            return selected_index_;
        }
        return 0;
    };

    auto commit_candidate_with_punctuation =
        [&](const std::string& punctuation, bool sentence_boundary) {
            int idx = selected_candidate_index();
            if (idx < 0) {
                return false;
            }

            commit_candidate(ic, current_candidates_[idx].word);
            ic->commitString(punctuation);

            if (sentence_boundary) {
                predictor_->on_sentence_boundary();
            }

            current_candidates_.clear();
            clear_state(ic);
            event.filterAndAccept();
            return true;
        };

    // ── Arrow Down: select next ──
    if (key.check(FcitxKey_Down)) {
        if (!current_candidates_.empty()) {
            selected_index_++;
            if (selected_index_ >= static_cast<int>(current_candidates_.size())) selected_index_ = 0;
            // Forward key to Fcitx5 candidate list for visual highlight
            auto& panel = ic->inputPanel();
            auto list = panel.candidateList();
            if (list) {
                auto* cursor = list->toCursorMovable();
                if (cursor) cursor->nextCandidate();
                ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
            }
            event.filterAndAccept();
        }
        return;
    }

    // ── Arrow Up: select previous ──
    if (key.check(FcitxKey_Up)) {
        if (!current_candidates_.empty()) {
            selected_index_--;
            if (selected_index_ < 0) selected_index_ = static_cast<int>(current_candidates_.size()) - 1;
            auto& panel = ic->inputPanel();
            auto list = panel.candidateList();
            if (list) {
                auto* cursor = list->toCursorMovable();
                if (cursor) cursor->prevCandidate();
                ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
            }
            event.filterAndAccept();
        }
        return;
    }

    // ── Tab: accept selected or first candidate ──
    if (key.check(FcitxKey_Tab)) {
        if (!current_candidates_.empty()) {
            int idx = (selected_index_ >= 0) ? selected_index_ : 0;
            if (idx >= static_cast<int>(current_candidates_.size())) {
                idx = 0;
            }
            commit_candidate(ic, current_candidates_[idx].word);
            event.filterAndAccept();
            return;
        }
        return;
    }

    // ── Enter: accept selected candidate if any ──
    if (key.check(FcitxKey_Return) || key.check(FcitxKey_KP_Enter)) {
        if (!current_candidates_.empty() && selected_index_ >= 0 &&
            selected_index_ < static_cast<int>(current_candidates_.size())) {
            commit_candidate(ic, current_candidates_[selected_index_].word);
            event.filterAndAccept();
            return;
        }
        if (!predictor_->buffer().empty()) {
            std::string word = predictor_->buffer();
            if (word.length() >= 3) {
                predictor_->learn_word(word);
            }
            ic->commitString(word);
            predictor_->on_word_boundary();
        }
        predictor_->on_sentence_boundary();
        current_candidates_.clear();
        clear_state(ic);
        return;
    }

    // ── Escape: dismiss suggestions ──
    if (key.check(FcitxKey_Escape)) {
        if (!predictor_->buffer().empty() || !current_candidates_.empty()) {
            predictor_->clear_buffer();
            current_candidates_.clear();
            clear_state(ic);
            event.filterAndAccept();
            return;
        }
        return;
    }

    // ── Space: learn word, show next-word predictions ──
    if (key.check(FcitxKey_space)) {
        bool swallow_space = false;
        if (apply_space_autocorrect(ic, swallow_space)) {
            current_candidates_.clear();
            selected_index_ = -1;
            update_next_word_candidates(ic);
            if (swallow_space) {
                event.filterAndAccept();
            }
            return;
        }

        if (!predictor_->buffer().empty()) {
            std::string word = predictor_->buffer();
            if (word.length() >= 3) {
                predictor_->learn_word(word);
            }
            ic->commitString(word + " ");
            predictor_->on_word_boundary();
        } else {
            ic->commitString(" ");
        }
        current_candidates_.clear();
        selected_index_ = -1;
        update_next_word_candidates(ic);
        event.filterAndAccept();
        return;
    }

    if (key.check(FcitxKey_apostrophe)) {
        if (commit_candidate_with_punctuation("'", false)) {
            return;
        }

        predictor_->push_char("'");
        update_candidates(ic);
        event.filterAndAccept();
        return;
    }

    if (key.check(FcitxKey_period) || key.check(FcitxKey_exclam) || key.check(FcitxKey_question)) {
        std::string punctuation = ".";
        if (key.check(FcitxKey_exclam)) {
            punctuation = "!";
        } else if (key.check(FcitxKey_question)) {
            punctuation = "?";
        }

        if (commit_candidate_with_punctuation(punctuation, true)) {
            return;
        }

        if (!predictor_->buffer().empty()) {
            if (predictor_->buffer().length() >= 3) {
                predictor_->learn_word(predictor_->buffer());
            }
            ic->commitString(predictor_->buffer());
            predictor_->on_word_boundary();
        }
        ic->commitString(punctuation);
        predictor_->on_sentence_boundary();
        current_candidates_.clear();
        clear_state(ic);
        event.filterAndAccept();
        return;
    }

    // ── Colon: start emoji mode if buffer is empty ──
    if (key.check(FcitxKey_colon)) {
        if (predictor_->buffer().empty()) {
            // Start emoji mode — add : to buffer
            predictor_->push_char(":");
            update_candidates(ic);
            event.filterAndAccept();
            return;
        } else if (predictor_->buffer()[0] == ':') {
            // Already in emoji mode — add another : (for ::)
            predictor_->push_char(":");
            update_candidates(ic);
            event.filterAndAccept();
            return;
        }
        // Buffer has text, treat as punctuation
        if (commit_candidate_with_punctuation(":", false)) {
            return;
        }
        if (!predictor_->buffer().empty()) {
            if (predictor_->buffer().length() >= 3) {
                predictor_->learn_word(predictor_->buffer());
            }
            ic->commitString(predictor_->buffer());
            predictor_->on_word_boundary();
        }
        ic->commitString(":");
        current_candidates_.clear();
        clear_state(ic);
        event.filterAndAccept();
        return;
    }

    if (key.check(FcitxKey_comma) || key.check(FcitxKey_semicolon)) {
        std::string punctuation = ",";
        if (key.check(FcitxKey_semicolon)) {
            punctuation = ";";
        }

        if (commit_candidate_with_punctuation(punctuation, false)) {
            return;
        }

        if (!predictor_->buffer().empty()) {
            if (predictor_->buffer().length() >= 3) {
                predictor_->learn_word(predictor_->buffer());
            }
            ic->commitString(predictor_->buffer());
            predictor_->on_word_boundary();
        }
        ic->commitString(punctuation);
        current_candidates_.clear();
        clear_state(ic);
        event.filterAndAccept();
        return;
    }

    // ── Backspace: update buffer ──
    if (key.check(FcitxKey_BackSpace)) {
        if (!predictor_->buffer().empty()) {
            predictor_->pop_char();
            update_candidates(ic);
            event.filterAndAccept();
        }
        return;
    }

    // ── Letter input ──
    std::string ch;
    if (is_letter_key(key, ch)) {
        predictor_->push_char(ch);
        update_candidates(ic);
        event.filterAndAccept();
        return;
    }

    // ── Any other key: reset buffer, pass through ──
    if (!predictor_->buffer().empty()) {
        ic->commitString(predictor_->buffer());
        predictor_->clear_buffer();
        current_candidates_.clear();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }
}

void LinuxCompleteEngine::commit_candidate(fcitx::InputContext* ic,
                                            const std::string& word) {
    const std::string buffer = predictor_->buffer();

    // Check if this is an emoji candidate (starts with emoji character)
    // Emoji format: "😊 :smile" — extract just the emoji
    if (!buffer.empty() && buffer[0] == ':' && word.length() >= 2) {
        // Find the space separator between emoji and shortcode
        auto space_pos = word.find(' ');
        std::string emoji_char = (space_pos != std::string::npos) ? word.substr(0, space_pos) : word;
        ic->commitString(emoji_char);
        predictor_->clear_buffer();
        current_candidates_.clear();
        selected_index_ = -1;
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
        return;
    }

    const std::string final_word = match_case_to_buffer(buffer, word);
    if (final_word.find(' ') != std::string::npos) {
        predictor_->on_text_accepted(final_word);
    } else {
        predictor_->on_word_accepted(final_word);
    }

    // Commit the full word — Fcitx5 auto-clears preedit on commit
    ic->commitString(final_word);

    predictor_->clear_buffer();
    current_candidates_.clear();
    selected_index_ = -1;
    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

void LinuxCompleteEngine::update_candidates(fcitx::InputContext* ic) {
    auto& panel = ic->inputPanel();
    panel.reset();
    current_candidates_.clear();
    selected_index_ = -1;

    const auto& buffer = predictor_->buffer();
    if (buffer.empty()) {
        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
        return;
    }

    fcitx::Text preedit;
    preedit.append(buffer);
    preedit.setCursor(fcitx::utf8::length(buffer));

    // Check for emoji shortcodes (:smile, :heart, etc.)
    if (buffer.length() >= 2 && buffer[0] == ':') {
        auto emoji_results = predictor_->predict_emoji(buffer);
        if (!emoji_results.empty()) {
            for (const auto& c : emoji_results) {
                current_candidates_.push_back(c);
            }

            auto candidateList = std::make_unique<fcitx::CommonCandidateList>();
            candidateList->setPageSize(5);
            candidateList->setLayoutHint(fcitx::CandidateLayoutHint::Vertical);

            for (const auto& c : current_candidates_) {
                candidateList->append<LinuxCompleteCandidate>(this, c);
            }

            panel.setCandidateList(std::move(candidateList));
            panel.setPreedit(preedit);
            panel.setClientPreedit(preedit);
            ic->updatePreedit();
            ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
            return;
        }
    }

    if (predictor_->should_predict()) {
        auto candidates = predictor_->predict(buffer);

        for (const auto& candidate : candidates) {
            if (candidate.word == buffer) continue;
            if (candidate.word.length() <= buffer.length()) continue;
            current_candidates_.push_back(candidate);
        }

        if (!current_candidates_.empty()) {
            std::string best = current_candidates_[0].word;
            if (best.length() > buffer.length()) {
                preedit.append(best.substr(buffer.length()), fcitx::TextFormatFlag::Underline);
            }

            auto candidateList = std::make_unique<fcitx::CommonCandidateList>();
            candidateList->setPageSize(5);
            candidateList->setLayoutHint(fcitx::CandidateLayoutHint::Vertical);

            for (const auto& c : current_candidates_) {
                candidateList->append<LinuxCompleteCandidate>(this, c);
            }

            panel.setCandidateList(std::move(candidateList));
        }
    }
    panel.setPreedit(preedit);
    panel.setClientPreedit(preedit);
    ic->updatePreedit();
    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

void LinuxCompleteEngine::update_next_word_candidates(fcitx::InputContext* ic) {
    auto& panel = ic->inputPanel();
    panel.reset();
    current_candidates_.clear();
    selected_index_ = -1;

    if (predictor_->can_predict_next()) {
        auto ngram_results = predictor_->predict_next_word();

        for (const auto& ng : ngram_results) {
            current_candidates_.push_back({ng.word, ng.score});
        }

        if (!current_candidates_.empty()) {
            auto candidateList = std::make_unique<fcitx::CommonCandidateList>();
            candidateList->setPageSize(5);
            candidateList->setLayoutHint(fcitx::CandidateLayoutHint::Vertical);

            for (const auto& c : current_candidates_) {
                candidateList->append<LinuxCompleteCandidate>(this, c);
            }

            panel.setCandidateList(std::move(candidateList));
        }
    }

    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

void LinuxCompleteEngine::update_candidates_with_selection(fcitx::InputContext* /*ic*/) {
    // Not used in simplified version
}

void LinuxCompleteEngine::show_candidates(fcitx::InputContext* /*ic*/, int /*highlight_idx*/) {
    // Not used in simplified version
}

void LinuxCompleteEngine::clear_state(fcitx::InputContext* ic) {
    auto& panel = ic->inputPanel();
    panel.reset();
    selected_index_ = -1;
    ic->updatePreedit();
    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

bool LinuxCompleteEngine::is_letter_key(const fcitx::Key& key, std::string& ch) const {
    auto sym = key.sym();

    if (sym >= FcitxKey_a && sym <= FcitxKey_z) {
        ch = std::string(1, static_cast<char>(sym));
        return true;
    }
    if (sym >= FcitxKey_A && sym <= FcitxKey_Z) {
        ch = std::string(1, static_cast<char>(sym));
        return true;
    }

    auto s = static_cast<uint32_t>(sym);
    if (s == 0x00E7) { ch = "\xC3\xA7"; return true; }
    if (s == 0x011F) { ch = "\xC4\x9F"; return true; }
    if (s == 0x0131) { ch = "\xC4\xB1"; return true; }
    if (s == 0x00F6) { ch = "\xC3\xB6"; return true; }
    if (s == 0x015F) { ch = "\xC5\x9F"; return true; }
    if (s == 0x00FC) { ch = "\xC3\xBC"; return true; }
    if (s == 0x00C7) { ch = "\xC3\x87"; return true; }
    if (s == 0x011E) { ch = "\xC4\x9E"; return true; }
    if (s == 0x0130) { ch = "\xC4\xB0"; return true; }
    if (s == 0x00D6) { ch = "\xC3\x96"; return true; }
    if (s == 0x015E) { ch = "\xC5\x9E"; return true; }
    if (s == 0x00DC) { ch = "\xC3\x9C"; return true; }

    return false;
}

LinuxCompleteEngine::AutoCorrectResult LinuxCompleteEngine::detect_space_autocorrect() const {
    AutoCorrectResult result;
    if (!predictor_) {
        return result;
    }

    const std::string current = predictor_->buffer();
    const std::string previous = predictor_->last_word();
    const std::string current_lower = to_lower_ascii(current);
    const std::string previous_lower = to_lower_ascii(previous);

    if (current_lower == "dont") {
        result.applied = true;
        result.replacement = "don't";
        return result;
    }

    if (previous_lower == "i" && current_lower == "am") {
        result.applied = true;
        result.merge_previous_word = true;
        result.replacement = "I'm";
        return result;
    }

    if (previous_lower == "you" && current_lower == "are") {
        result.applied = true;
        result.merge_previous_word = true;
        result.replacement = "you're";
        return result;
    }

    if (previous_lower == "we" && current_lower == "are") {
        result.applied = true;
        result.merge_previous_word = true;
        result.replacement = "we're";
        return result;
    }

    if (previous_lower == "they" && current_lower == "are") {
        result.applied = true;
        result.merge_previous_word = true;
        result.replacement = "they're";
        return result;
    }

    if (previous_lower == "it" && current_lower == "is") {
        result.applied = true;
        result.merge_previous_word = true;
        result.replacement = "it's";
    }
    return result;
}

bool LinuxCompleteEngine::apply_space_autocorrect(fcitx::InputContext* ic, bool& swallow_space) {
    swallow_space = false;
    if (!predictor_ || predictor_->buffer().empty()) {
        return false;
    }

    auto correction = detect_space_autocorrect();
    if (!correction.applied) {
        return false;
    }

    if (correction.merge_previous_word) {
        const bool can_rewrite_in_place =
            ic->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText) &&
            ic->surroundingText().isValid() &&
            !predictor_->last_word().empty();
        if (!can_rewrite_in_place) {
            return false;
        }

        const auto delete_chars =
            static_cast<unsigned int>(fcitx::utf8::length(predictor_->last_word())) +
            1u;
        ic->deleteSurroundingText(-static_cast<int>(delete_chars), delete_chars);
        ic->commitString(correction.replacement + " ");
        predictor_->replace_last_committed_and_current(correction.replacement);
        swallow_space = true;
        return true;
    }

    ic->commitString(correction.replacement + " ");
    predictor_->replace_current_word(correction.replacement);
    swallow_space = true;
    return true;
}

} // namespace linuxcomplete

FCITX_ADDON_FACTORY(linuxcomplete::LinuxCompleteFactory);
