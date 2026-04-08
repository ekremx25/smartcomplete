#include "predictor/ngram.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace linuxcomplete {

NgramModel::NgramModel() = default;
NgramModel::~NgramModel() = default;

std::string NgramModel::to_lower(const std::string& s) {
    std::string result = s;
    for (auto& c : result) {
        if (c >= 'A' && c <= 'Z') c += 32;
    }
    return result;
}

bool NgramModel::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    int count = 0;
    int merged_duplicates = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string word1, word2;
        int freq;
        if (iss >> word1 >> word2 >> freq) {
            auto& nexts = bigrams_[to_lower(word1)];
            auto [it, inserted] = nexts.emplace(to_lower(word2), freq);
            if (!inserted) {
                it->second = std::max(it->second, freq);
                merged_duplicates++;
            }
            count++;
        }
    }

    std::cout << "[LinuxComplete] Loaded " << count << " bigrams from " << path << std::endl;
    if (merged_duplicates > 0) {
        std::cout << "[LinuxComplete] Merged " << merged_duplicates
                  << " duplicate bigram entries from " << path << std::endl;
    }
    return true;
}

bool NgramModel::save(const std::string& path) const {
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;

    for (const auto& [w1, nexts] : bigrams_) {
        for (const auto& [w2, freq] : nexts) {
            file << w1 << " " << w2 << " " << freq << "\n";
        }
    }
    return true;
}

std::vector<NgramCandidate> NgramModel::predict_next(const std::string& prev_word,
                                                       int max_results) const {
    std::string key = to_lower(prev_word);
    auto it = bigrams_.find(key);
    if (it == bigrams_.end()) return {};

    std::vector<NgramCandidate> results;
    for (const auto& [word, freq] : it->second) {
        results.push_back({word, freq});
    }

    std::sort(results.begin(), results.end());

    if (static_cast<int>(results.size()) > max_results) {
        results.resize(max_results);
    }

    return results;
}

void NgramModel::learn(const std::string& word1, const std::string& word2) {
    std::string w1 = to_lower(word1);
    std::string w2 = to_lower(word2);
    if (w1.empty() || w2.empty()) return;
    bigrams_[w1][w2] += 2;  // Gradual learning — needs multiple uses to become strong
}

} // namespace linuxcomplete
