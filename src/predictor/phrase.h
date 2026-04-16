#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace linuxcomplete {

struct PhraseEntry {
    std::string completion;  // Full phrase to suggest
    int score;
};

class PhraseModel {
public:
    PhraseModel();
    ~PhraseModel();

    // Load phrases from file
    // Format: word1 word2 completion_word1 completion_word2... frequency
    bool load(const std::string& path);

    // Get phrase completions based on last 2-3 words
    // Son 2-3 kelimeye göre tamamlama öner
    std::vector<PhraseEntry> predict(const std::string& prev2,
                                      const std::string& prev1,
                                      int max_results = 3) const;

    size_t size() const { return phrases_.size(); }

private:
    // Key: "word1 word2" → completions
    std::unordered_map<std::string, std::vector<PhraseEntry>> phrases_;

    static std::string make_key(const std::string& w1, const std::string& w2);
    static std::string to_lower(const std::string& s);
};

} // namespace linuxcomplete
