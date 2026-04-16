#include "predictor/trie.h"
#include <algorithm>
#include <queue>
#include <codecvt>
#include <locale>

namespace linuxcomplete {

Trie::Trie() : root_(std::make_unique<TrieNode>()) {}
Trie::~Trie() = default;

std::u32string Trie::to_utf32(const std::string& utf8) {
    std::u32string result;
    size_t i = 0;
    while (i < utf8.size()) {
        char32_t cp;
        unsigned char c = utf8[i];
        if (c < 0x80) {
            cp = c;
            i += 1;
        } else if (c < 0xC0) {
            cp = 0xFFFD;  // Invalid
            i += 1;
        } else if (c < 0xE0) {
            cp = c & 0x1F;
            if (i + 1 < utf8.size()) cp = (cp << 6) | (utf8[i+1] & 0x3F);
            i += 2;
        } else if (c < 0xF0) {
            cp = c & 0x0F;
            if (i + 2 < utf8.size()) {
                cp = (cp << 6) | (utf8[i+1] & 0x3F);
                cp = (cp << 6) | (utf8[i+2] & 0x3F);
            }
            i += 3;
        } else {
            cp = c & 0x07;
            if (i + 3 < utf8.size()) {
                cp = (cp << 6) | (utf8[i+1] & 0x3F);
                cp = (cp << 6) | (utf8[i+2] & 0x3F);
                cp = (cp << 6) | (utf8[i+3] & 0x3F);
            }
            i += 4;
        }
        result.push_back(cp);
    }
    return result;
}

void Trie::insert(const std::string& word, int frequency) {
    auto codepoints = to_utf32(word);
    TrieNode* node = root_.get();

    for (char32_t cp : codepoints) {
        // Convert to lowercase for case-insensitive matching
        if (cp >= U'A' && cp <= U'Z') cp += 32;



        if (node->children.find(cp) == node->children.end()) {
            node->children[cp] = std::make_unique<TrieNode>();
        }
        node = node->children[cp].get();
    }

    if (!node->is_word) {
        word_count_++;
    }
    node->is_word = true;
    node->frequency = frequency;
    node->word = word;
}

std::vector<Candidate> Trie::search(const std::string& prefix, int max_results) const {
    auto codepoints = to_utf32(prefix);
    const TrieNode* node = root_.get();

    // Navigate to the prefix node
    for (char32_t cp : codepoints) {
        if (cp >= U'A' && cp <= U'Z') cp += 32;


        auto it = node->children.find(cp);
        if (it == node->children.end()) {
            return {};  // Prefix not found
        }
        node = it->second.get();
    }

    // Collect all words under this prefix
    std::vector<Candidate> results;
    collect_words(node, results);

    // Sort by score (frequency + user_frequency)
    std::sort(results.begin(), results.end());

    // Return top N
    if (results.size() > static_cast<size_t>(max_results)) {
        results.resize(max_results);
    }

    return results;
}

void Trie::collect_words(const TrieNode* node, std::vector<Candidate>& results) const {
    if (!node) return;

    if (node->is_word) {
        results.push_back({
            node->word,
            node->frequency + (node->user_frequency * 10)  // User words get 10x boost
        });
    }

    for (const auto& [cp, child] : node->children) {
        (void)cp;
        collect_words(child.get(), results);
    }
}

bool Trie::contains(const std::string& word) const {
    auto codepoints = to_utf32(word);
    const TrieNode* node = root_.get();

    for (char32_t cp : codepoints) {
        if (cp >= U'A' && cp <= U'Z') cp += 32;


        auto it = node->children.find(cp);
        if (it == node->children.end()) return false;
        node = it->second.get();
    }

    return node->is_word;
}

void Trie::boost(const std::string& word, int amount) {
    auto codepoints = to_utf32(word);
    TrieNode* node = root_.get();

    for (char32_t cp : codepoints) {
        if (cp >= U'A' && cp <= U'Z') cp += 32;


        auto it = node->children.find(cp);
        if (it == node->children.end()) return;
        node = it->second.get();
    }

    if (node->is_word) {
        node->user_frequency += amount;
    }
}

size_t Trie::size() const {
    return word_count_;
}

} // namespace linuxcomplete
