#include "predictor/phrase.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace linuxcomplete {

PhraseModel::PhraseModel() = default;
PhraseModel::~PhraseModel() = default;

std::string PhraseModel::to_lower(const std::string& s) {
    std::string result = s;
    for (auto& c : result) {
        if (c >= 'A' && c <= 'Z') c += 32;
    }
    return result;
}

std::string PhraseModel::make_key(const std::string& w1, const std::string& w2) {
    return to_lower(w1) + " " + to_lower(w2);
}

bool PhraseModel::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    int count = 0;
    int merged_duplicates = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        // Parse: word1 word2 rest_of_completion frequency
        std::istringstream iss(line);
        std::vector<std::string> words;
        std::string word;
        while (iss >> word) {
            words.push_back(word);
        }

        // Need at least: trigger1 trigger2 completion1 frequency
        if (words.size() < 4) continue;

        // Last word is frequency
        int freq = 0;
        try {
            freq = std::stoi(words.back());
        } catch (...) {
            continue;
        }
        words.pop_back();

        // First two words are trigger
        std::string trigger1 = words[0];
        std::string trigger2 = words[1];

        // Rest is the completion (from word 3 onwards)
        std::string completion;
        for (size_t i = 2; i < words.size(); i++) {
            if (!completion.empty()) completion += " ";
            completion += words[i];
        }

        if (completion.empty()) continue;

        std::string key = make_key(trigger1, trigger2);
        auto& entries = phrases_[key];
        auto existing = std::find_if(entries.begin(), entries.end(),
                                     [&](const PhraseEntry& entry) {
                                         return to_lower(entry.completion) == to_lower(completion);
                                     });
        if (existing == entries.end()) {
            entries.push_back({completion, freq});
        } else {
            existing->score = std::max(existing->score, freq);
            merged_duplicates++;
        }
        count++;
    }

    std::cout << "[LinuxComplete] Loaded " << count << " phrases from " << path << std::endl;
    if (merged_duplicates > 0) {
        std::cout << "[LinuxComplete] Merged " << merged_duplicates
                  << " duplicate phrase entries from " << path << std::endl;
    }
    return true;
}

std::vector<PhraseEntry> PhraseModel::predict(const std::string& prev2,
                                                const std::string& prev1,
                                                int max_results) const {
    std::string key = make_key(prev2, prev1);
    auto it = phrases_.find(key);
    if (it == phrases_.end()) return {};

    auto results = it->second;
    std::sort(results.begin(), results.end(),
              [](const PhraseEntry& a, const PhraseEntry& b) {
                  return a.score > b.score;
              });

    if (static_cast<int>(results.size()) > max_results) {
        results.resize(max_results);
    }

    return results;
}

} // namespace linuxcomplete
