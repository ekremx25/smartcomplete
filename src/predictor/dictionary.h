#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "predictor/trie.h"

namespace linuxcomplete {

enum class Language {
    English,
    Turkish,
    Unknown
};

class Dictionary {
public:
    Dictionary();
    ~Dictionary();

    // Load a dictionary file into the trie
    // Bir sözlük dosyasını trie'ye yükle
    bool load(const std::string& dict_path, const std::string& freq_path, Language lang);

    // Load user dictionary
    // Kullanıcı sözlüğünü yükle
    bool load_user_dict(const std::string& path);

    // Save user dictionary
    // Kullanıcı sözlüğünü kaydet
    bool save_user_dict(const std::string& path) const;

    // Record or update a learned user word
    void record_user_word(const std::string& word, int freq);

    // Detect language from a prefix
    // Bir ön ekten dili algıla
    static Language detect_language(const std::string& text);

    // Check if character is Turkish-specific
    // Karakterin Türkçeye özgü olup olmadığını kontrol et
    static bool is_turkish_char(char32_t cp);

    Trie& trie() { return trie_; }
    const Trie& trie() const { return trie_; }

private:
    Trie trie_;
    std::unordered_map<std::string, int> user_words_;
};

} // namespace linuxcomplete
