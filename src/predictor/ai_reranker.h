#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "predictor/trie.h"

namespace linuxcomplete {

struct AiRerankerConfig {
    bool enabled = false;
    bool smart_fallback = true;
    bool debug_logging = false;
    std::string model = "gpt-4o-mini";
    int timeout_ms = 1200;
    int uncertainty_gap_threshold = 160;
    std::size_t max_cache_entries = 128;
    // Base URL for the OpenAI-compatible API (everything before /chat/completions).
    // Defaults to OpenAI; switch to Groq / Ollama / OpenRouter / etc. without code changes.
    std::string api_base = "https://api.openai.com/v1";
    // Name of the environment variable that holds the API key.
    // Defaults to OPENAI_API_KEY; use GROQ_API_KEY for Groq, etc.
    std::string api_key_env = "OPENAI_API_KEY";
    // Optional: JSON file managed by the UI (e.g. Quickshell Settings → API Keys).
    // When present and valid, overrides model / api_base / api_key from env.
    // Expected shape:
    //   { "provider": "groq", "model": "llama-3.3-70b-versatile",
    //     "api_base": "https://api.groq.com/openai/v1", "api_key": "gsk_..." }
    // File is read on every request (cheap — small file, kernel caches).
    std::string api_key_file = "";
};

struct AiRerankRequest {
    std::string prefix;
    std::string previous_word;
    std::string last_word;
    bool sentence_start = false;
    bool next_word_mode = false;
};

class AiReranker {
public:
    explicit AiReranker(AiRerankerConfig config);

    bool is_enabled() const;
    std::vector<Candidate> rerank(const std::vector<Candidate>& candidates,
                                  const AiRerankRequest& request) const;
    bool should_rerank(const std::vector<Candidate>& candidates,
                       const AiRerankRequest& request) const;

    static std::vector<std::string> parse_ranked_words(const std::string& response_text);
    static std::vector<Candidate> apply_ranking(const std::vector<Candidate>& candidates,
                                                const std::vector<std::string>& ranked_words);
    static std::string build_cache_key(const std::vector<Candidate>& candidates,
                                       const AiRerankRequest& request,
                                       const std::string& model);

private:
    AiRerankerConfig config_;

    static std::string to_lower_ascii(std::string text);
    static std::string extract_chat_content(const std::string& response_body);
    bool contains_context_sensitive_candidates(const std::vector<Candidate>& candidates) const;
    void log_debug(const std::string& message) const;
    std::string request_ranking(const std::vector<Candidate>& candidates,
                                const AiRerankRequest& request) const;
};

} // namespace linuxcomplete
