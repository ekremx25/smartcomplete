#include "engine/engine.h"
#include "engine/engine_config.h"
#include "predictor/text_utils.h"

#include <fcitx-utils/utf8.h>
#include <fcitx/candidatelist.h>
#include <iostream>
#include <filesystem>
#include <cstdlib>

namespace linuxcomplete {

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

// ═══════════════════════════════════════════
// LinuxCompleteEngine — construction
// ═══════════════════════════════════════════

LinuxCompleteEngine::LinuxCompleteEngine(fcitx::Instance* instance)
    : instance_(instance) {
    PredictorConfig config;

    const char* home_raw = std::getenv("HOME");
    std::string home = home_raw ? home_raw : "";
    std::string config_dir = home + "/.local/share/linuxcomplete";
    auto user_config_path = std::filesystem::path(home) / ".config" / "linuxcomplete" / "linuxcomplete.conf";
    auto system_config_path = std::filesystem::path(CONFIG_INSTALL_DIR) / "linuxcomplete.conf";

    load_predictor_config_from_file(system_config_path, config);
    if (std::filesystem::exists(user_config_path)) {
        load_predictor_config_from_file(user_config_path, config);
    }

    if (std::filesystem::exists(config_dir + "/dict")) {
        config.dict_dir = config_dir + "/dict";
    } else if (std::filesystem::exists(std::string(DICT_INSTALL_DIR))) {
        config.dict_dir = DICT_INSTALL_DIR;
    }

    config.user_dict_path = config_dir + "/user/learned.txt";
    apply_predictor_env_overrides(config);

    predictor_ = std::make_unique<Predictor>(config);
    predictor_->init();

    std::cout << "[SmartComplete] Engine initialized" << std::endl;
}

LinuxCompleteEngine::~LinuxCompleteEngine() {
    if (predictor_) predictor_->save();
}

// ═══════════════════════════════════════════
// Fcitx5 lifecycle
// ═══════════════════════════════════════════

std::vector<fcitx::InputMethodEntry> LinuxCompleteEngine::listInputMethods() {
    std::vector<fcitx::InputMethodEntry> result;
    result.emplace_back(std::move(
        fcitx::InputMethodEntry("linuxcomplete", "SmartComplete", "*", "linuxcomplete")
            .setIcon("input-keyboard")
            .setLabel("SC")
    ));
    return result;
}

void LinuxCompleteEngine::activate(const fcitx::InputMethodEntry&, fcitx::InputContextEvent&) {
    if (predictor_) predictor_->clear_buffer();
    current_candidates_.clear();
    selected_index_ = -1;
}

void LinuxCompleteEngine::deactivate(const fcitx::InputMethodEntry&, fcitx::InputContextEvent& event) {
    if (predictor_) predictor_->clear_buffer();
    current_candidates_.clear();
    clear_state(event.inputContext());
}

void LinuxCompleteEngine::reset(const fcitx::InputMethodEntry&, fcitx::InputContextEvent& event) {
    if (predictor_) predictor_->clear_buffer();
    current_candidates_.clear();
    clear_state(event.inputContext());
}

// ═══════════════════════════════════════════
// Key event handling
// ═══════════════════════════════════════════

void LinuxCompleteEngine::keyEvent(const fcitx::InputMethodEntry&, fcitx::KeyEvent& event) {
    auto* ic = event.inputContext();
    auto key = event.key();
    if (event.isRelease()) return;

    // ── Pass-through for blocklisted programs (terminals, shells) ──
    // Shell tab-completion + autocorrection would fight each other,
    // and emoji shortcodes in a terminal are just noise.
    // Matches by prefix (case-insensitive), so "kitty" also catches
    // "kittyfloat", "kittyterm" and other custom window class variants.
    if (predictor_ && predictor_->is_program_disabled(ic->program())) {
        // Flush any pending buffer and stay out of the way.
        if (!predictor_->buffer().empty()) {
            ic->commitString(predictor_->buffer());
            predictor_->clear_buffer();
            current_candidates_.clear();
            clear_state(ic);
        }
        return; // Let Fcitx5 forward the raw key unchanged.
    }

    // Helper: get the best candidate index (selected or first).
    auto best_idx = [this]() -> int {
        if (current_candidates_.empty()) return -1;
        if (selected_index_ >= 0 && selected_index_ < static_cast<int>(current_candidates_.size()))
            return selected_index_;
        return 0;
    };

    // Helper: commit candidate + punctuation in one step.
    auto commit_with_punct = [&](const std::string& punct, bool sentence_end) -> bool {
        int idx = best_idx();
        if (idx < 0) return false;
        commit_candidate(ic, current_candidates_[idx].word);
        ic->commitString(punct);
        if (sentence_end) predictor_->on_sentence_boundary();
        current_candidates_.clear();
        clear_state(ic);
        event.filterAndAccept();
        return true;
    };

    // Helper: flush buffer as raw text + commit punctuation.
    auto flush_buffer_with_punct = [&](const std::string& punct, bool sentence_end) {
        if (!predictor_->buffer().empty()) {
            if (predictor_->buffer().length() >= 3) predictor_->learn_word(predictor_->buffer());
            ic->commitString(predictor_->buffer());
            predictor_->on_word_boundary();
        }
        ic->commitString(punct);
        if (sentence_end) predictor_->on_sentence_boundary();
        current_candidates_.clear();
        clear_state(ic);
        event.filterAndAccept();
    };

    // ── Arrow navigation ──
    if (key.check(FcitxKey_Down) || key.check(FcitxKey_Up)) {
        if (current_candidates_.empty()) return;
        const int n = static_cast<int>(current_candidates_.size());
        selected_index_ = key.check(FcitxKey_Down)
            ? (selected_index_ + 1) % n
            : (selected_index_ - 1 + n) % n;
        auto& panel = ic->inputPanel();
        auto list = panel.candidateList();
        if (list) {
            auto* cursor = list->toCursorMovable();
            if (cursor) key.check(FcitxKey_Down) ? cursor->nextCandidate() : cursor->prevCandidate();
            ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
        }
        event.filterAndAccept();
        return;
    }

    // ── Tab / Enter: accept candidate ──
    if (key.check(FcitxKey_Tab) || key.check(FcitxKey_Return) || key.check(FcitxKey_KP_Enter)) {
        if (!current_candidates_.empty()) {
            int idx = (selected_index_ >= 0) ? selected_index_ : 0;
            if (idx >= static_cast<int>(current_candidates_.size())) idx = 0;
            commit_candidate(ic, current_candidates_[idx].word);
            event.filterAndAccept();
            return;
        }
        if (key.check(FcitxKey_Return) || key.check(FcitxKey_KP_Enter)) {
            if (!predictor_->buffer().empty()) {
                if (predictor_->buffer().length() >= 3) predictor_->learn_word(predictor_->buffer());
                ic->commitString(predictor_->buffer());
                predictor_->on_word_boundary();
            }
            predictor_->on_sentence_boundary();
            current_candidates_.clear();
            clear_state(ic);
        }
        return;
    }

    // ── Escape: dismiss ──
    if (key.check(FcitxKey_Escape)) {
        if (!predictor_->buffer().empty() || !current_candidates_.empty()) {
            predictor_->clear_buffer();
            current_candidates_.clear();
            clear_state(ic);
            event.filterAndAccept();
        }
        return;
    }

    // ── Space: autocorrect + next-word ──
    if (key.check(FcitxKey_space)) {
        bool swallow = false;
        if (apply_space_autocorrect(ic, swallow)) {
            current_candidates_.clear();
            selected_index_ = -1;
            update_next_word_candidates(ic);
            if (swallow) event.filterAndAccept();
            return;
        }
        if (!predictor_->buffer().empty()) {
            if (predictor_->buffer().length() >= 3) predictor_->learn_word(predictor_->buffer());
            ic->commitString(predictor_->buffer() + " ");
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

    // ── Apostrophe ──
    if (key.check(FcitxKey_apostrophe)) {
        if (commit_with_punct("'", false)) return;
        predictor_->push_char("'");
        update_candidates(ic);
        event.filterAndAccept();
        return;
    }

    // ── Sentence-ending punctuation (.!?) ──
    if (key.check(FcitxKey_period) || key.check(FcitxKey_exclam) || key.check(FcitxKey_question)) {
        std::string p = key.check(FcitxKey_exclam) ? "!" : (key.check(FcitxKey_question) ? "?" : ".");
        if (!commit_with_punct(p, true)) flush_buffer_with_punct(p, true);
        return;
    }

    // ── Colon: emoji mode ──
    if (key.check(FcitxKey_colon)) {
        if (predictor_->buffer().empty() || predictor_->buffer()[0] == ':') {
            predictor_->push_char(":");
            update_candidates(ic);
            event.filterAndAccept();
            return;
        }
        if (!commit_with_punct(":", false)) flush_buffer_with_punct(":", false);
        return;
    }

    // ── Comma / semicolon ──
    if (key.check(FcitxKey_comma) || key.check(FcitxKey_semicolon)) {
        std::string p = key.check(FcitxKey_semicolon) ? ";" : ",";
        if (!commit_with_punct(p, false)) flush_buffer_with_punct(p, false);
        return;
    }

    // ── Backspace ──
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

    // ── Any other key: flush buffer, pass through ──
    if (!predictor_->buffer().empty()) {
        ic->commitString(predictor_->buffer());
        predictor_->clear_buffer();
        current_candidates_.clear();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }
}

// ═══════════════════════════════════════════
// Candidate commit & UI rendering
// ═══════════════════════════════════════════

void LinuxCompleteEngine::commit_candidate(fcitx::InputContext* ic, const std::string& word) {
    const std::string buffer = predictor_->buffer();

    // Emoji candidate: extract emoji character before the space.
    if (!buffer.empty() && buffer[0] == ':' && word.length() >= 2) {
        auto sp = word.find(' ');
        ic->commitString(sp != std::string::npos ? word.substr(0, sp) : word);
        predictor_->clear_buffer();
        current_candidates_.clear();
        selected_index_ = -1;
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
        return;
    }

    const std::string final_word = text_utils::match_case_to_buffer(buffer, word);
    if (final_word.find(' ') != std::string::npos)
        predictor_->on_text_accepted(final_word);
    else
        predictor_->on_word_accepted(final_word);

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

    // Emoji shortcode mode (:smile, :heart, etc.)
    if (buffer.length() >= 2 && buffer[0] == ':') {
        auto emoji_results = predictor_->predict_emoji(buffer);
        if (!emoji_results.empty()) {
            for (const auto& c : emoji_results) current_candidates_.push_back(c);
            show_candidate_list(ic, preedit);
            return;
        }
    }

    // Normal prefix prediction.
    if (predictor_->should_predict()) {
        auto candidates = predictor_->predict(buffer);
        for (const auto& c : candidates) {
            if (c.word == buffer || c.word.length() <= buffer.length()) continue;
            current_candidates_.push_back(c);
        }
        if (!current_candidates_.empty()) {
            // Ghost text: show the best completion inline.
            const auto& best = current_candidates_[0].word;
            if (best.length() > buffer.length()) {
                preedit.append(best.substr(buffer.length()), fcitx::TextFormatFlag::Underline);
            }
        }
    }

    show_candidate_list(ic, preedit);
}

void LinuxCompleteEngine::update_next_word_candidates(fcitx::InputContext* ic) {
    auto& panel = ic->inputPanel();
    panel.reset();
    current_candidates_.clear();
    selected_index_ = -1;

    if (predictor_->can_predict_next()) {
        for (const auto& ng : predictor_->predict_next_word()) {
            current_candidates_.push_back({ng.word, ng.score});
        }
        if (!current_candidates_.empty()) {
            auto list = std::make_unique<fcitx::CommonCandidateList>();
            list->setPageSize(5);
            list->setLayoutHint(fcitx::CandidateLayoutHint::Vertical);
            for (const auto& c : current_candidates_)
                list->append<LinuxCompleteCandidate>(this, c);
            panel.setCandidateList(std::move(list));
        }
    }
    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

void LinuxCompleteEngine::show_candidate_list(fcitx::InputContext* ic, const fcitx::Text& preedit) {
    auto& panel = ic->inputPanel();
    if (!current_candidates_.empty()) {
        auto list = std::make_unique<fcitx::CommonCandidateList>();
        list->setPageSize(5);
        list->setLayoutHint(fcitx::CandidateLayoutHint::Vertical);
        for (const auto& c : current_candidates_)
            list->append<LinuxCompleteCandidate>(this, c);
        panel.setCandidateList(std::move(list));
    }
    panel.setPreedit(preedit);
    panel.setClientPreedit(preedit);
    ic->updatePreedit();
    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

void LinuxCompleteEngine::clear_state(fcitx::InputContext* ic) {
    ic->inputPanel().reset();
    selected_index_ = -1;
    ic->updatePreedit();
    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

bool LinuxCompleteEngine::is_letter_key(const fcitx::Key& key, std::string& ch) const {
    auto sym = key.sym();
    if ((sym >= FcitxKey_a && sym <= FcitxKey_z) || (sym >= FcitxKey_A && sym <= FcitxKey_Z)) {
        ch = std::string(1, static_cast<char>(sym));
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════
// Auto-correction (contraction merging)
// ═══════════════════════════════════════════

LinuxCompleteEngine::AutoCorrectResult LinuxCompleteEngine::detect_space_autocorrect() const {
    AutoCorrectResult result;
    if (!predictor_) return result;

    const std::string cur = text_utils::to_lower_ascii(predictor_->buffer());
    const std::string prev = text_utils::to_lower_ascii(predictor_->last_word());

    struct ContractionRule { const char* prev; const char* cur; bool merge; const char* replacement; };
    static const ContractionRule rules[] = {
        {nullptr, "dont",  false, "don't"},
        {"i",     "am",    true,  "I'm"},
        {"you",   "are",   true,  "you're"},
        {"we",    "are",   true,  "we're"},
        {"they",  "are",   true,  "they're"},
        {"it",    "is",    true,  "it's"},
    };

    for (const auto& r : rules) {
        if (cur == r.cur && (!r.prev || prev == r.prev)) {
            result.applied = true;
            result.merge_previous_word = r.merge;
            result.replacement = r.replacement;
            return result;
        }
    }
    return result;
}

bool LinuxCompleteEngine::apply_space_autocorrect(fcitx::InputContext* ic, bool& swallow_space) {
    swallow_space = false;
    if (!predictor_ || predictor_->buffer().empty()) return false;

    auto correction = detect_space_autocorrect();
    if (!correction.applied) return false;

    if (correction.merge_previous_word) {
        const bool can_rewrite =
            ic->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText) &&
            ic->surroundingText().isValid() &&
            !predictor_->last_word().empty();
        if (!can_rewrite) return false;

        const auto del = static_cast<unsigned int>(fcitx::utf8::length(predictor_->last_word())) + 1u;
        ic->deleteSurroundingText(-static_cast<int>(del), del);
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
