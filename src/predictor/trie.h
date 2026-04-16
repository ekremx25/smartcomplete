#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace linuxcomplete {

struct TrieNode {
    std::unordered_map<char32_t, std::unique_ptr<TrieNode>> children;
    bool is_word = false;
    int frequency = 0;       // Base frequency from dictionary
    int user_frequency = 0;  // User-learned frequency
    std::string word;        // Full word stored at leaf
};

struct Candidate {
    std::string word;
    int score;  // Combined frequency score

    bool operator<(const Candidate& other) const {
        return score > other.score;  // Higher score first
    }
};

class Trie {
public:
    Trie();
    ~Trie();

    // Insert a word with its frequency
    // Bir kelimeyi frekansıyla birlikte ekle
    void insert(const std::string& word, int frequency = 1);

    // Search for words matching a prefix, returns top N candidates
    // Bir ön eke uyan kelimeleri ara, en iyi N adayı döndür
    std::vector<Candidate> search(const std::string& prefix, int max_results = 5) const;

    // Check if a word exists
    // Bir kelimenin var olup olmadığını kontrol et
    bool contains(const std::string& word) const;

    // Boost a word's user frequency (learning)
    // Bir kelimenin kullanıcı frekansını artır (öğrenme)
    void boost(const std::string& word, int amount = 1);

    // Get total number of words
    // Toplam kelime sayısını al
    size_t size() const;

private:
    std::unique_ptr<TrieNode> root_;
    size_t word_count_ = 0;

    // Convert UTF-8 string to UTF-32 codepoints
    static std::u32string to_utf32(const std::string& utf8);

    // Recursive search helper
    void collect_words(const TrieNode* node, std::vector<Candidate>& results) const;
};

} // namespace linuxcomplete
