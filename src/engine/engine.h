#pragma once

#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/candidatelist.h>
#include <vector>

#include "predictor/predictor.h"

namespace linuxcomplete {

class LinuxCompleteEngine;

// Candidate word in the popup list
class LinuxCompleteCandidate : public fcitx::CandidateWord {
public:
    LinuxCompleteCandidate(LinuxCompleteEngine* engine, const Candidate& candidate);
    void select(fcitx::InputContext* ic) const override;

private:
    LinuxCompleteEngine* engine_;
    std::string word_;
};

// Main engine class
class LinuxCompleteEngine : public fcitx::InputMethodEngineV2 {
public:
    LinuxCompleteEngine(fcitx::Instance* instance);
    ~LinuxCompleteEngine() override;

    // Called when input context is created
    void activate(const fcitx::InputMethodEntry& entry,
                  fcitx::InputContextEvent& event) override;

    // Called when input context is destroyed
    void deactivate(const fcitx::InputMethodEntry& entry,
                    fcitx::InputContextEvent& event) override;

    // Handle key events
    void keyEvent(const fcitx::InputMethodEntry& entry,
                  fcitx::KeyEvent& event) override;

    // Reset state
    void reset(const fcitx::InputMethodEntry& entry,
               fcitx::InputContextEvent& event) override;

    // List input methods provided by this engine
    std::vector<fcitx::InputMethodEntry> listInputMethods() override;

    // Commit a candidate word
    void commit_candidate(fcitx::InputContext* ic, const std::string& word);

    fcitx::Instance* instance() { return instance_; }

private:
    struct AutoCorrectResult {
        bool applied = false;
        bool merge_previous_word = false;
        std::string replacement;
    };

    fcitx::Instance* instance_;
    std::unique_ptr<Predictor> predictor_;
    int selected_index_ = -1;
    std::vector<Candidate> current_candidates_;  // Cached filtered candidates

    // Update candidate list in input panel
    void update_candidates(fcitx::InputContext* ic);

    // Show next-word predictions (after space)
    void update_next_word_candidates(fcitx::InputContext* ic);

    // Rebuild candidates with current selection highlighted
    void update_candidates_with_selection(fcitx::InputContext* ic);

    // Render candidate list with ghost text
    void show_candidates(fcitx::InputContext* ic, int highlight_idx);

    // Clear preedit and candidates
    void clear_state(fcitx::InputContext* ic);

    // Check if a key is a letter (including Turkish chars)
    bool is_letter_key(const fcitx::Key& key, std::string& ch) const;

    AutoCorrectResult detect_space_autocorrect() const;
    bool apply_space_autocorrect(fcitx::InputContext* ic, bool& swallow_space);
    static std::string to_lower_ascii(std::string text);
    static std::string normalize_contraction_typo(std::string text);
    static std::string fold_for_matching(std::string text);
    static bool is_prefix_match(const std::string& buffer, const std::string& word);
    static std::string match_case_to_buffer(const std::string& buffer, std::string word);
};

// Factory for creating engine instances
class LinuxCompleteFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
        return new LinuxCompleteEngine(manager->instance());
    }
};

} // namespace linuxcomplete
