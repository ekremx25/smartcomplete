#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace linuxcomplete {

struct EmojiEntry {
    std::string shortcode;  // :smile
    std::string emoji;      // 😊
    int score;
};

class EmojiModel {
public:
    // Loads emoji data from a TSV file (shortcode\temoji\tscore).
    explicit EmojiModel(const std::string& data_path = "");

    std::vector<EmojiEntry> predict(const std::string& prefix, int max_results = 5) const;
    size_t size() const { return all_emojis_.size(); }

private:
    std::vector<EmojiEntry> all_emojis_;
    // Prefix index: first 2 chars of shortcode → list of entries.
    // Reduces search space from O(N) to O(bucket_size).
    std::unordered_map<std::string, std::vector<const EmojiEntry*>> prefix_index_;

    void load_from_file(const std::string& path);
    void build_index();
};

} // namespace linuxcomplete
