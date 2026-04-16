#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "predictor/trie.h"

namespace linuxcomplete {

enum class Language {
    English,
    Unknown
};

class Dictionary {
public:
    Dictionary();
    ~Dictionary();

    // Load a dictionary file into the trie
    bool load(const std::string& dict_path, const std::string& freq_path, Language lang);

    // Load user dictionary
    bool load_user_dict(const std::string& path);

    // Save user dictionary
    bool save_user_dict(const std::string& path) const;

    // Record or update a learned user word
    void record_user_word(const std::string& word, int freq);

    // Detect language from a prefix
    static Language detect_language(const std::string& text);

    Trie& trie() { return trie_; }
    const Trie& trie() const { return trie_; }

private:
    Trie trie_;
    std::unordered_map<std::string, int> user_words_;
};

} // namespace linuxcomplete
