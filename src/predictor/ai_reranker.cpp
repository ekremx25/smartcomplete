#include "predictor/ai_reranker.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <unordered_map>
#include <unistd.h>

namespace linuxcomplete {

namespace {

std::mutex& cache_mutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<std::string, std::vector<std::string>>& ranking_cache() {
    static std::unordered_map<std::string, std::vector<std::string>> cache;
    return cache;
}

std::deque<std::string>& cache_order() {
    static std::deque<std::string> order;
    return order;
}

} // namespace

AiReranker::AiReranker(AiRerankerConfig config) : config_(std::move(config)) {}

bool AiReranker::is_enabled() const {
    const char* api_key = std::getenv("OPENAI_API_KEY");
    return config_.enabled && api_key && api_key[0] != '\0';
}

std::vector<Candidate> AiReranker::rerank(const std::vector<Candidate>& candidates,
                                          const AiRerankRequest& request) const {
    if (!is_enabled() || candidates.size() < 2) {
        if (config_.debug_logging) {
            log_debug("skip: disabled or insufficient candidates");
        }
        return candidates;
    }

    if (!should_rerank(candidates, request)) {
        log_debug("skip: confidence gating kept local ranking");
        return candidates;
    }

    const std::string cache_key = build_cache_key(candidates, request, config_.model);
    {
        std::lock_guard<std::mutex> lock(cache_mutex());
        auto it = ranking_cache().find(cache_key);
        if (it != ranking_cache().end()) {
            log_debug("cache hit");
            return apply_ranking(candidates, it->second);
        }
    }

    const std::string output_text = request_ranking(candidates, request);
    if (output_text.empty()) {
        log_debug("fallback: empty AI response");
        return candidates;
    }

    const auto ranked_words = parse_ranked_words(output_text);
    if (ranked_words.empty()) {
        log_debug("fallback: AI response could not be parsed");
        return candidates;
    }

    {
        std::lock_guard<std::mutex> lock(cache_mutex());
        ranking_cache()[cache_key] = ranked_words;
        cache_order().push_back(cache_key);
        while (cache_order().size() > config_.max_cache_entries) {
            ranking_cache().erase(cache_order().front());
            cache_order().pop_front();
        }
    }

    log_debug("AI rerank applied");
    return apply_ranking(candidates, ranked_words);
}

bool AiReranker::should_rerank(const std::vector<Candidate>& candidates,
                               const AiRerankRequest& request) const {
    if (!is_enabled() || candidates.size() < 2) {
        return false;
    }

    if (!config_.smart_fallback) {
        return true;
    }

    if (contains_context_sensitive_candidates(candidates)) {
        return true;
    }

    const int gap = candidates[0].score - candidates[1].score;
    if (gap <= config_.uncertainty_gap_threshold) {
        return true;
    }

    if (request.next_word_mode && candidates.size() >= 3) {
        const int spread = candidates[0].score - candidates[2].score;
        if (spread <= config_.uncertainty_gap_threshold * 2) {
            return true;
        }
    }

    if (!request.next_word_mode && request.prefix.size() >= 4 && candidates.size() >= 3) {
        const int spread = candidates[0].score - candidates[2].score;
        if (spread <= config_.uncertainty_gap_threshold * 2) {
            return true;
        }
    }

    return false;
}

std::vector<std::string> AiReranker::parse_ranked_words(const std::string& response_text) {
    nlohmann::json parsed = nlohmann::json::parse(response_text, nullptr, false);
    if (parsed.is_discarded()) {
        return {};
    }

    if (!parsed.contains("ranking") || !parsed["ranking"].is_array()) {
        return {};
    }

    std::vector<std::string> ranked_words;
    for (const auto& entry : parsed["ranking"]) {
        if (entry.is_string()) {
            ranked_words.push_back(entry.get<std::string>());
        }
    }
    return ranked_words;
}

std::vector<Candidate> AiReranker::apply_ranking(const std::vector<Candidate>& candidates,
                                                 const std::vector<std::string>& ranked_words) {
    if (candidates.empty() || ranked_words.empty()) {
        return candidates;
    }

    std::vector<Candidate> ranked;
    ranked.reserve(candidates.size());
    std::vector<bool> used(candidates.size(), false);

    for (const auto& preferred : ranked_words) {
        const std::string folded_preferred = to_lower_ascii(preferred);
        for (size_t i = 0; i < candidates.size(); ++i) {
            if (used[i]) {
                continue;
            }
            if (to_lower_ascii(candidates[i].word) == folded_preferred) {
                ranked.push_back(candidates[i]);
                used[i] = true;
                break;
            }
        }
    }

    for (size_t i = 0; i < candidates.size(); ++i) {
        if (!used[i]) {
            ranked.push_back(candidates[i]);
        }
    }

    return ranked;
}

std::string AiReranker::build_cache_key(const std::vector<Candidate>& candidates,
                                        const AiRerankRequest& request,
                                        const std::string& model) {
    std::ostringstream key;
    key << model << '\n'
        << request.prefix << '\n'
        << request.previous_word << '\n'
        << request.last_word << '\n'
        << (request.sentence_start ? "1" : "0") << '\n'
        << (request.next_word_mode ? "1" : "0");

    for (const auto& candidate : candidates) {
        key << '\n' << candidate.word << '\t' << candidate.score;
    }

    return key.str();
}

std::string AiReranker::to_lower_ascii(std::string text) {
    for (auto& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c += 32;
        }
    }
    return text;
}

std::string AiReranker::shell_quote(const std::string& text) {
    std::string quoted = "'";
    for (char ch : text) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

std::string AiReranker::extract_output_text(const std::string& response_body) {
    nlohmann::json parsed = nlohmann::json::parse(response_body, nullptr, false);
    if (parsed.is_discarded()) {
        return {};
    }

    if (parsed.contains("output_text") && parsed["output_text"].is_string()) {
        return parsed["output_text"].get<std::string>();
    }

    if (!parsed.contains("output") || !parsed["output"].is_array()) {
        return {};
    }

    for (const auto& item : parsed["output"]) {
        if (!item.contains("content") || !item["content"].is_array()) {
            continue;
        }
        for (const auto& content : item["content"]) {
            if (content.contains("text") && content["text"].is_string()) {
                return content["text"].get<std::string>();
            }
        }
    }

    return {};
}

bool AiReranker::contains_context_sensitive_candidates(const std::vector<Candidate>& candidates) const {
    for (const auto& candidate : candidates) {
        if (candidate.word.find(' ') != std::string::npos ||
            candidate.word.find('\'') != std::string::npos) {
            return true;
        }
    }
    return false;
}

void AiReranker::log_debug(const std::string& message) const {
    if (!config_.debug_logging) {
        return;
    }
    std::cerr << "[LinuxComplete][AI] " << message << std::endl;
}

std::string AiReranker::request_ranking(const std::vector<Candidate>& candidates,
                                        const AiRerankRequest& request) const {
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key || api_key[0] == '\0') {
        return {};
    }

    if (const char* test_response = std::getenv("LINUXCOMPLETE_AI_TEST_RESPONSE")) {
        return test_response;
    }

    nlohmann::json candidate_rows = nlohmann::json::array();
    for (const auto& candidate : candidates) {
        candidate_rows.push_back({
            {"word", candidate.word},
            {"score", candidate.score}
        });
    }

    const std::string mode = request.next_word_mode ? "next_word" : "prefix";
    const std::string context_line =
        "mode=" + mode +
        ", prefix=" + request.prefix +
        ", previous_word=" + request.previous_word +
        ", last_word=" + request.last_word +
        ", sentence_start=" + std::string(request.sentence_start ? "true" : "false");

    nlohmann::json payload = {
        {"model", config_.model},
        {"input", nlohmann::json::array({
            {
                {"role", "system"},
                {"content", nlohmann::json::array({
                    {
                        {"type", "input_text"},
                        {"text",
                         "You rerank autocomplete candidates. Prefer grammatical, natural, and contextually likely "
                         "choices. Preserve only existing candidate words. Return strict JSON with shape "
                         "{\"ranking\":[...]} and no extra text."}
                    }
                })}
            },
            {
                {"role", "user"},
                {"content", nlohmann::json::array({
                    {
                        {"type", "input_text"},
                        {"text",
                         "Context: " + context_line +
                         "\nCandidates: " + candidate_rows.dump() +
                         "\nRank the candidates from best to worst for natural autocomplete in this exact context. "
                         "Consider grammar, collocations, contractions, typo recovery, and conversational fluency. "
                         "Do not invent new candidates. Return JSON only."}
                    }
                })}
            }
        })},
        {"max_output_tokens", 120}
    };

    std::array<char, 256> temp_name{};
    std::snprintf(temp_name.data(), temp_name.size(), "/tmp/linuxcomplete_ai_%d_XXXXXX", getpid());
    const int fd = mkstemp(temp_name.data());
    if (fd == -1) {
        return {};
    }
    close(fd);

    const std::filesystem::path payload_path(temp_name.data());
    {
        std::ofstream out(payload_path);
        if (!out.is_open()) {
            std::filesystem::remove(payload_path);
            return {};
        }
        out << payload.dump();
    }

    const int timeout_seconds = std::max(1, config_.timeout_ms / 1000);
    log_debug("sending OpenAI rerank request");
    const std::string command =
        "curl -sS --max-time " + std::to_string(timeout_seconds) +
        " https://api.openai.com/v1/responses" +
        " -H " + shell_quote("Authorization: Bearer " + std::string(api_key)) +
        " -H 'Content-Type: application/json'" +
        " --data-binary @" + shell_quote(payload_path.string());

    std::string response_body;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[4096];
        while (std::fgets(buffer, sizeof(buffer), pipe)) {
            response_body += buffer;
        }
        const int status = pclose(pipe);
        if (status != 0) {
            log_debug("curl request failed, using local ranking");
            response_body.clear();
        }
    }

    std::filesystem::remove(payload_path);
    return extract_output_text(response_body);
}

} // namespace linuxcomplete
