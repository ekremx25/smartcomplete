#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace linuxcomplete {

struct EmojiEntry {
    std::string shortcode;  // :smile
    std::string emoji;      // 😊
    int score;
};

class EmojiModel {
public:
    EmojiModel();

    // Get emoji suggestions for a shortcode prefix
    // :smi → 😊 smile, 😄 grin
    std::vector<EmojiEntry> predict(const std::string& prefix, int max_results = 5) const;

    size_t size() const { return emojis_.size(); }

private:
    std::vector<EmojiEntry> emojis_;
};

} // namespace linuxcomplete
