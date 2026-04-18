#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "predictor/trie.h"
#include "predictor/dictionary.h"
#include "predictor/ngram.h"
#include "predictor/phrase.h"
#include "predictor/emoji.h"

namespace linuxcomplete {

struct PredictorConfig {
    int max_candidates = 5;
    int min_prefix_length = 2;
    // Absolute floor for candidate score — filters out rare/weird words
    // with no frequency data (e.g. "wifehood" when you type "wife").
    int min_candidate_score = 50;
    // Higher bar for suggestions when the buffer IS already a valid
    // dictionary word: only show extensions that are clearly meaningful.
    int complete_word_extension_min_score = 150;
    bool learn_new_words = true;
    bool ai_rerank_enabled = false;
    bool ai_smart_fallback = true;
    bool ai_debug_logging = false;
    std::string ai_model = "gpt-4o-mini";
    int ai_timeout_ms = 1200;
    int ai_uncertainty_gap_threshold = 160;
    int ai_max_cache_entries = 128;
    // OpenAI-compatible provider endpoint (Groq, Ollama, OpenRouter, etc.).
    std::string ai_api_base = "https://api.openai.com/v1";
    std::string ai_api_key_env = "OPENAI_API_KEY";
    // UI-managed file path with {provider, model, api_base, api_key}.
    // Overrides all three above when present. Default: ~/.config/linuxcomplete/api_keys.json
    std::string ai_api_key_file = "";
    std::string dict_dir;
    std::string user_dict_path;

    // Programs where SmartComplete should stay out of the way (no predictions,
    // pure pass-through). Default list covers common terminal emulators and shells.
    std::vector<std::string> disabled_programs = {
        "kitty", "alacritty", "foot", "wezterm", "wezterm-gui",
        "xterm", "urxvt", "rxvt", "st",
        "gnome-terminal", "gnome-terminal-server",
        "konsole", "xfce4-terminal", "lxterminal",
        "mate-terminal", "deepin-terminal", "terminator",
        "tilix", "hyper", "terminology", "blackbox",
        "ptyxis", "cool-retro-term", "termite",
        "zsh", "bash", "fish", "tmux", "screen"
    };
};

class Predictor {
public:
    explicit Predictor(const PredictorConfig& config);
    ~Predictor();

    bool init();

    // Get predictions — combines trie prefix search + ngram context
    std::vector<Candidate> predict(const std::string& prefix) const;

    // Get next-word predictions (after space, no prefix yet)
    std::vector<Candidate> predict_next_word() const;

    void on_word_accepted(const std::string& word);
    void on_text_accepted(const std::string& text);

    // Get emoji suggestions for :shortcode prefix
    std::vector<Candidate> predict_emoji(const std::string& prefix) const;
    void learn_word(const std::string& word);
    void replace_current_word(const std::string& word);
    void replace_last_committed_and_current(const std::string& word);
    void save();

    const std::string& buffer() const { return buffer_; }
    const std::string& last_word() const { return last_word_; }

    void push_char(const std::string& ch);
    void pop_char();
    void clear_buffer();

    // Called when space/enter is pressed — moves buffer to last_word
    void on_word_boundary();
    void on_sentence_boundary();

    bool should_predict() const;

    // Returns true if SmartComplete should stay out of the way for this program
    // (e.g. terminal emulators where shell completion handles tab/prediction).
    bool is_program_disabled(const std::string& program) const;

    // Check if we can predict next word (buffer empty, last_word exists)
    bool can_predict_next() const;

private:
    PredictorConfig config_;
    Dictionary dictionary_;
    NgramModel system_ngram_;
    NgramModel user_ngram_;
    PhraseModel phrase_model_;
    EmojiModel emoji_model_;
    std::string buffer_;
    std::string previous_word_;
    std::string last_word_;
    std::string user_ngram_path_;
    std::string user_freq_path_;
    bool sentence_start_ = true;

    // User word frequency tracker — most used words get priority
    std::unordered_map<std::string, int> user_freq_;

    void load_user_freq();
    void save_user_freq() const;
    void register_completed_word(const std::string& word, bool boost_trie);
    std::vector<Candidate> merge_context_predictions(int max_results) const;
    std::vector<Candidate> filter_candidates(const std::string& prefix,
                                             std::vector<Candidate> candidates) const;
    std::vector<Candidate> typo_correction_predictions(const std::string& prefix,
                                                       int max_results) const;
    int phrase_bonus(const std::string& candidate) const;
    int english_context_bonus(const std::string& candidate) const;
    std::vector<Candidate> sentence_starter_predictions(const std::string& prefix,
                                                        int max_results) const;
    std::vector<Candidate> contraction_predictions(const std::string& prefix,
                                                   int max_results) const;
    Language preferred_language(const std::string& prefix) const;
    static std::string to_lower_ascii(std::string word);
    static std::string capitalize_for_display(const std::string& word);
    static std::string fold_for_matching(std::string text);
    static bool starts_with_folded(const std::string& text, const std::string& prefix);
    static std::vector<std::string> split_text_tokens(const std::string& text);
    static void merge_candidate(std::unordered_map<std::string, Candidate>& merged,
                                const Candidate& candidate);
};

} // namespace linuxcomplete
