#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace linuxcomplete {

struct NgramCandidate {
    std::string word;
    int score;
    bool operator<(const NgramCandidate& other) const {
        return score > other.score;
    }
};

class NgramModel {
public:
    NgramModel();
    ~NgramModel();

    // Load bigram data from file
    bool load(const std::string& path);

    // Save learned bigrams
    bool save(const std::string& path) const;

    // Get next word predictions based on previous word(s)
    // Önceki kelime(ler)e göre sonraki kelime tahminleri
    std::vector<NgramCandidate> predict_next(const std::string& prev_word, int max_results = 5) const;

    // Learn a bigram from user input
    // Kullanıcı girdisinden bigram öğren
    void learn(const std::string& word1, const std::string& word2);

    size_t size() const { return bigrams_.size(); }

    bool empty() const { return bigrams_.empty(); }

private:
    // bigrams_["love"]["you"] = 50 (frequency)
    std::unordered_map<std::string, std::unordered_map<std::string, int>> bigrams_;

    static std::string to_lower(const std::string& s);
};

} // namespace linuxcomplete
