#include "engine/engine_config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

namespace linuxcomplete {

void load_predictor_config_from_file(const std::filesystem::path& path, PredictorConfig& config) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    nlohmann::json parsed = nlohmann::json::parse(file, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) return;

    auto read_int = [&](const char* key, int& out) {
        if (parsed.contains(key) && parsed[key].is_number_integer()) out = parsed[key].get<int>();
    };
    auto read_bool = [&](const char* key, bool& out) {
        if (parsed.contains(key) && parsed[key].is_boolean()) out = parsed[key].get<bool>();
    };
    auto read_str = [&](const char* key, std::string& out) {
        if (parsed.contains(key) && parsed[key].is_string()) out = parsed[key].get<std::string>();
    };
    read_int("max_candidates", config.max_candidates);
    read_int("min_prefix_length", config.min_prefix_length);
    read_bool("learn_new_words", config.learn_new_words);
    read_bool("ai_rerank_enabled", config.ai_rerank_enabled);
    read_bool("ai_smart_fallback", config.ai_smart_fallback);
    read_bool("ai_debug_logging", config.ai_debug_logging);
    read_str("ai_model", config.ai_model);
    read_int("ai_timeout_ms", config.ai_timeout_ms);
    read_int("ai_uncertainty_gap_threshold", config.ai_uncertainty_gap_threshold);
    if (parsed.contains("ai_max_cache_entries") && parsed["ai_max_cache_entries"].is_number_integer()) {
        config.ai_max_cache_entries = parsed["ai_max_cache_entries"].get<std::size_t>();
    }

    // disabled_programs: "replace" replaces the default list, "extend" appends to it.
    if (parsed.contains("disabled_programs") && parsed["disabled_programs"].is_array()) {
        config.disabled_programs.clear();
        for (const auto& item : parsed["disabled_programs"]) {
            if (item.is_string()) config.disabled_programs.push_back(item.get<std::string>());
        }
    }
    if (parsed.contains("disabled_programs_extend") && parsed["disabled_programs_extend"].is_array()) {
        for (const auto& item : parsed["disabled_programs_extend"]) {
            if (item.is_string()) config.disabled_programs.push_back(item.get<std::string>());
        }
    }
}

void apply_predictor_env_overrides(PredictorConfig& config) {
    auto env_bool = [](const char* name, bool& out) {
        const char* val = std::getenv(name);
        if (!val) return;
        std::string s(val);
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        out = (s == "1" || s == "true" || s == "yes" || s == "on");
    };

    auto env_int = [](const char* name, int& out) {
        const char* val = std::getenv(name);
        if (!val) return;
        try { out = std::stoi(val); } catch (...) {}
    };

    env_bool("LINUXCOMPLETE_AI_ENABLED", config.ai_rerank_enabled);
    env_bool("LINUXCOMPLETE_AI_SMART_FALLBACK", config.ai_smart_fallback);
    env_bool("LINUXCOMPLETE_AI_DEBUG", config.ai_debug_logging);
    env_int("LINUXCOMPLETE_AI_TIMEOUT_MS", config.ai_timeout_ms);
    env_int("LINUXCOMPLETE_AI_GAP_THRESHOLD", config.ai_uncertainty_gap_threshold);

    if (const char* model = std::getenv("LINUXCOMPLETE_AI_MODEL")) {
        config.ai_model = model;
    }
    if (const char* cache = std::getenv("LINUXCOMPLETE_AI_MAX_CACHE_ENTRIES")) {
        try { config.ai_max_cache_entries = std::stoi(cache); } catch (...) {}
    }
}

} // namespace linuxcomplete
