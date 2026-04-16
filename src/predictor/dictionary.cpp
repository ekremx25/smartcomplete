#include "predictor/dictionary.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace linuxcomplete {

Dictionary::Dictionary() = default;
Dictionary::~Dictionary() = default;

bool Dictionary::load(const std::string& dict_path, const std::string& freq_path,
                      Language /*lang*/) {
    // Load frequency data first
    std::unordered_map<std::string, int> frequencies;
    if (!freq_path.empty()) {
        std::ifstream freq_file(freq_path);
        if (freq_file.is_open()) {
            std::string line;
            while (std::getline(freq_file, line)) {
                std::istringstream iss(line);
                std::string word;
                int freq;
                if (iss >> word >> freq) {
                    frequencies[word] = freq;
                }
            }
        }
    }

    // Load dictionary words
    std::ifstream dict_file(dict_path);
    if (!dict_file.is_open()) {
        std::cerr << "[LinuxComplete] Failed to open dictionary: " << dict_path << std::endl;
        return false;
    }

    std::string word;
    int loaded = 0;
    while (std::getline(dict_file, word)) {
        // Trim whitespace
        while (!word.empty() && (word.back() == '\r' || word.back() == '\n' ||
               word.back() == ' ')) {
            word.pop_back();
        }
        while (!word.empty() && (word.front() == ' ')) {
            word.erase(word.begin());
        }

        if (word.empty() || word[0] == '#') continue;

        int freq = 1;
        auto it = frequencies.find(word);
        if (it != frequencies.end()) {
            freq = it->second;
        }

        trie_.insert(word, freq);
        loaded++;
    }

    std::cout << "[LinuxComplete] Loaded " << loaded << " words from " << dict_path << std::endl;
    return true;
}

bool Dictionary::load_user_dict(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string word;
        int freq;
        if (iss >> word >> freq) {
            trie_.insert(word, 1);
            trie_.boost(word, freq);
            user_words_[word] = freq;
        }
    }

    std::cout << "[LinuxComplete] Loaded " << user_words_.size() << " user words" << std::endl;
    return true;
}

bool Dictionary::save_user_dict(const std::string& path) const {
    // Ensure directory exists
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;

    for (const auto& [word, freq] : user_words_) {
        file << word << " " << freq << "\n";
    }

    return true;
}

void Dictionary::record_user_word(const std::string& word, int freq) {
    if (word.empty()) {
        return;
    }

    auto it = user_words_.find(word);
    if (it == user_words_.end()) {
        user_words_[word] = std::max(1, freq);
        return;
    }

    it->second = std::max(it->second, freq);
}

Language Dictionary::detect_language(const std::string& text) {
    for (size_t i = 0; i < text.size(); ) {
        unsigned char c = text[i];
        char32_t cp;

        if (c < 0x80) {
            cp = c;
            i += 1;
        } else if (c < 0xE0) {
            cp = (c & 0x1F) << 6;
            if (i + 1 < text.size()) cp |= (text[i+1] & 0x3F);
            i += 2;
        } else if (c < 0xF0) {
            cp = (c & 0x0F) << 12;
            if (i + 2 < text.size()) {
                cp |= (text[i+1] & 0x3F) << 6;
                cp |= (text[i+2] & 0x3F);
            }
            i += 3;
        } else {
            i += 4;
            continue;
        }

        if (cp == U'\'') {
            return Language::English;
        }
    }

    return Language::English;
}

} // namespace linuxcomplete
