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
    std::string model = "gpt-5-mini";
    int timeout_ms = 1200;
    int uncertainty_gap_threshold = 160;
    std::size_t max_cache_entries = 128;
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
    static std::string shell_quote(const std::string& text);
    static std::string extract_output_text(const std::string& response_body);
    bool contains_context_sensitive_candidates(const std::vector<Candidate>& candidates) const;
    void log_debug(const std::string& message) const;
    std::string request_ranking(const std::vector<Candidate>& candidates,
                                const AiRerankRequest& request) const;
};

} // namespace linuxcomplete
