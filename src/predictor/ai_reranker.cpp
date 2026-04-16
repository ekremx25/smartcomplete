#include "predictor/ai_reranker.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <unordered_map>

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace linuxcomplete {

// ---------------------------------------------------------------------------
// RAII guard: removes a temp file on destruction.
// ---------------------------------------------------------------------------
namespace {

struct TempFileGuard {
    std::string path;
    TempFileGuard() = default;
    explicit TempFileGuard(std::string p) : path(std::move(p)) {}
    ~TempFileGuard() {
        if (!path.empty()) {
            std::filesystem::remove(path);
        }
    }
    TempFileGuard(const TempFileGuard&) = delete;
    TempFileGuard& operator=(const TempFileGuard&) = delete;
};

// ---------------------------------------------------------------------------
// Thread-safe LRU cache (module-level statics behind functions).
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Subprocess helper: runs curl via fork/execvp (no shell).
// Returns captured stdout, or empty string on failure/timeout.
// ---------------------------------------------------------------------------
std::string exec_curl(const std::vector<std::string>& args, int timeout_seconds) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return {};
    }

    const pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return {};
    }

    if (pid == 0) {
        // Child: redirect stdout to pipe write-end, exec curl.
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        // Suppress stderr in child.
        const int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        close(pipefd[1]);

        // Build argv for execvp.
        std::vector<const char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& arg : args) {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        execvp(argv[0], const_cast<char* const*>(argv.data()));
        _exit(127);
    }

    // Parent: read stdout from pipe read-end.
    close(pipefd[1]);

    std::string output;
    char buf[4096];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        output.append(buf, static_cast<size_t>(n));
    }
    close(pipefd[0]);

    // Wait for child with timeout.
    int status = 0;
    int waited = 0;
    while (waited < timeout_seconds * 1000) {
        const pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                return output;
            }
            return {};
        }
        if (result < 0) {
            return {};
        }
        usleep(50000); // 50ms
        waited += 50;
    }

    // Timeout: kill child.
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    return {};
}

} // namespace

// ---------------------------------------------------------------------------
// AiReranker implementation
// ---------------------------------------------------------------------------

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
                               const AiRerankRequest& /* request */) const {
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

    if (candidates.size() >= 3) {
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
        const std::string folded = to_lower_ascii(preferred);
        for (size_t i = 0; i < candidates.size(); ++i) {
            if (used[i]) continue;
            if (to_lower_ascii(candidates[i].word) == folded) {
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
        << (request.sentence_start ? '1' : '0') << '\n'
        << (request.next_word_mode ? '1' : '0');

    for (const auto& c : candidates) {
        key << '\n' << c.word << '\t' << c.score;
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

// ---------------------------------------------------------------------------
// Extracts the assistant's text content from a Chat Completions response.
// Expected path: choices[0].message.content
// ---------------------------------------------------------------------------
std::string AiReranker::extract_chat_content(const std::string& response_body) {
    nlohmann::json parsed = nlohmann::json::parse(response_body, nullptr, false);
    if (parsed.is_discarded()) {
        return {};
    }

    // Chat Completions format: choices[0].message.content
    if (parsed.contains("choices") && parsed["choices"].is_array() &&
        !parsed["choices"].empty()) {
        const auto& choice = parsed["choices"][0];
        if (choice.contains("message") && choice["message"].contains("content") &&
            choice["message"]["content"].is_string()) {
            return choice["message"]["content"].get<std::string>();
        }
    }

    // Fallback: Responses API format (output_text or output[].content[].text)
    if (parsed.contains("output_text") && parsed["output_text"].is_string()) {
        return parsed["output_text"].get<std::string>();
    }

    if (parsed.contains("output") && parsed["output"].is_array()) {
        for (const auto& item : parsed["output"]) {
            if (!item.contains("content") || !item["content"].is_array()) continue;
            for (const auto& content : item["content"]) {
                if (content.contains("text") && content["text"].is_string()) {
                    return content["text"].get<std::string>();
                }
            }
        }
    }

    return {};
}

bool AiReranker::contains_context_sensitive_candidates(const std::vector<Candidate>& candidates) const {
    for (const auto& c : candidates) {
        if (c.word.find(' ') != std::string::npos ||
            c.word.find('\'') != std::string::npos) {
            return true;
        }
    }
    return false;
}

void AiReranker::log_debug(const std::string& message) const {
    if (config_.debug_logging) {
        std::cerr << "[SmartComplete][AI] " << message << std::endl;
    }
}

// ---------------------------------------------------------------------------
// Sends a reranking request to the OpenAI Chat Completions API.
// Uses fork/execvp to run curl directly — no shell interpretation.
// Payload is written to a temp file via the fd from mkstemp (no TOCTOU).
// ---------------------------------------------------------------------------
std::string AiReranker::request_ranking(const std::vector<Candidate>& candidates,
                                        const AiRerankRequest& request) const {
    const char* api_key = std::getenv("OPENAI_API_KEY");
    if (!api_key || api_key[0] == '\0') {
        return {};
    }

    // Allow test injection for regression tests (no network call).
    if (const char* test_response = std::getenv("LINUXCOMPLETE_AI_TEST_RESPONSE")) {
        return test_response;
    }

    // Build candidate list for the prompt.
    nlohmann::json candidate_rows = nlohmann::json::array();
    for (const auto& c : candidates) {
        candidate_rows.push_back({{"word", c.word}, {"score", c.score}});
    }

    const std::string mode = request.next_word_mode ? "next_word" : "prefix";
    const std::string context_line =
        "mode=" + mode +
        ", prefix=" + request.prefix +
        ", previous_word=" + request.previous_word +
        ", last_word=" + request.last_word +
        ", sentence_start=" + std::string(request.sentence_start ? "true" : "false");

    // Chat Completions API payload.
    nlohmann::json payload = {
        {"model", config_.model},
        {"messages", nlohmann::json::array({
            {
                {"role", "system"},
                {"content",
                 "You rerank autocomplete candidates. Prefer grammatical, natural, and contextually "
                 "likely choices. Preserve only existing candidate words. Return strict JSON with "
                 "shape {\"ranking\":[...]} and no extra text."}
            },
            {
                {"role", "user"},
                {"content",
                 "Context: " + context_line +
                 "\nCandidates: " + candidate_rows.dump() +
                 "\nRank the candidates from best to worst for natural autocomplete in this exact "
                 "context. Consider grammar, collocations, contractions, typo recovery, and "
                 "conversational fluency. Do not invent new candidates. Return JSON only."}
            }
        })},
        {"max_tokens", 120},
        {"temperature", 0.3}
    };

    // Write payload to temp file via the fd from mkstemp (no TOCTOU race).
    std::array<char, 256> temp_name{};
    std::snprintf(temp_name.data(), temp_name.size(), "/tmp/smartcomplete_ai_XXXXXX");
    const int fd = mkstemp(temp_name.data());
    if (fd == -1) {
        return {};
    }

    TempFileGuard temp_guard(std::string(temp_name.data()));
    const std::string payload_str = payload.dump();
    const ssize_t written = write(fd, payload_str.data(), payload_str.size());
    close(fd);

    if (written < 0 || static_cast<size_t>(written) != payload_str.size()) {
        return {};
    }

    // Build curl argv — no shell, direct exec.
    const int timeout_seconds = std::max(1, config_.timeout_ms / 1000);
    const std::string auth_header = "Authorization: Bearer " + std::string(api_key);
    const std::string data_arg = "@" + temp_guard.path;

    std::vector<std::string> curl_args = {
        "curl", "-sS",
        "--max-time", std::to_string(timeout_seconds),
        "https://api.openai.com/v1/chat/completions",
        "-H", auth_header,
        "-H", "Content-Type: application/json",
        "--data-binary", data_arg
    };

    log_debug("sending rerank request (" + std::to_string(candidates.size()) + " candidates)");
    const std::string response_body = exec_curl(curl_args, timeout_seconds + 2);

    if (response_body.empty()) {
        log_debug("curl returned empty response");
        return {};
    }

    return extract_chat_content(response_body);
}

} // namespace linuxcomplete
