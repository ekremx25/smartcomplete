#include "predictor/emoji.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace linuxcomplete {

EmojiModel::EmojiModel(const std::string& data_path) {
    if (!data_path.empty()) {
        load_from_file(data_path);
        build_index();
    }
}

void EmojiModel::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[SmartComplete] Warning: emoji data not found: " << path << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string shortcode, emoji;
        int score = 400;

        if (!std::getline(iss, shortcode, '\t')) continue;
        if (!std::getline(iss, emoji, '\t')) continue;
        std::string score_str;
        if (std::getline(iss, score_str, '\t')) {
            try { score = std::stoi(score_str); } catch (...) {}
        }

        if (!shortcode.empty() && !emoji.empty()) {
            // Lowercase shortcode for case-insensitive matching.
            for (auto& c : shortcode) {
                if (c >= 'A' && c <= 'Z') c += 32;
            }
            all_emojis_.push_back({shortcode, emoji, score});
        }
    }
}

void EmojiModel::build_index() {
    prefix_index_.clear();
    for (const auto& entry : all_emojis_) {
        // Index by first 1 char and first 2 chars for fast prefix lookup.
        if (entry.shortcode.size() >= 1) {
            prefix_index_[entry.shortcode.substr(0, 1)].push_back(&entry);
        }
        if (entry.shortcode.size() >= 2) {
            prefix_index_[entry.shortcode.substr(0, 2)].push_back(&entry);
        }
    }
}

std::vector<EmojiEntry> EmojiModel::predict(const std::string& prefix, int max_results) const {
    if (prefix.empty() || all_emojis_.empty()) return {};

    // Lowercase prefix for matching.
    std::string lower_prefix = prefix;
    for (auto& c : lower_prefix) {
        if (c >= 'A' && c <= 'Z') c += 32;
    }

    // Use the longest prefix key available in the index (1 or 2 chars).
    const std::string key = lower_prefix.substr(0, std::min<size_t>(2, lower_prefix.size()));
    auto it = prefix_index_.find(key);
    if (it == prefix_index_.end()) {
        // Fall back to 1-char key.
        if (key.size() > 1) {
            it = prefix_index_.find(key.substr(0, 1));
        }
        if (it == prefix_index_.end()) return {};
    }

    std::vector<EmojiEntry> results;
    for (const auto* entry : it->second) {
        if (entry->shortcode.rfind(lower_prefix, 0) == 0) {
            results.push_back(*entry);
        }
    }

    std::sort(results.begin(), results.end(),
              [](const EmojiEntry& a, const EmojiEntry& b) { return a.score > b.score; });

    if (static_cast<int>(results.size()) > max_results) {
        results.resize(max_results);
    }

    return results;
}

} // namespace linuxcomplete
