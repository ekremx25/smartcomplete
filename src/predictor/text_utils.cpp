#include "predictor/text_utils.h"

#include <algorithm>
#include <unordered_map>

namespace linuxcomplete {
namespace text_utils {

std::string to_lower_ascii(std::string text) {
    for (auto& c : text) {
        if (c >= 'A' && c <= 'Z') c += 32;
    }
    return text;
}

std::string fold_for_matching(std::string text) {
    text = to_lower_ascii(std::move(text));
    text.erase(std::remove(text.begin(), text.end(), '\''), text.end());
    return text;
}

std::string capitalize_for_display(const std::string& word) {
    if (word.empty()) return word;
    if (word[0] >= 'a' && word[0] <= 'z') {
        std::string result = word;
        result[0] = static_cast<char>(result[0] - 32);
        return result;
    }
    return word;
}

bool is_prefix_match(const std::string& buffer, const std::string& word) {
    if (buffer.empty()) return true;
    return fold_for_matching(word).rfind(fold_for_matching(buffer), 0) == 0;
}

std::string match_case_to_buffer(const std::string& buffer, std::string word) {
    if (buffer.empty() || word.empty()) return word;
    const auto first = static_cast<unsigned char>(buffer[0]);
    if (first >= 'a' && first <= 'z' && word[0] >= 'A' && word[0] <= 'Z') {
        word[0] = static_cast<char>(word[0] + 32);
    }
    return word;
}

std::string normalize_contraction_typo(std::string text) {
    static const std::unordered_map<std::string, std::string> typo_rewrites = {
        {"dont't", "don't"}, {"cant't", "can't"}, {"wont't", "won't"},
        {"isnt't", "isn't"}, {"arent't", "aren't"}, {"didnt't", "didn't"},
        {"hasnt't", "hasn't"}, {"havent't", "haven't"}, {"wouldnt't", "wouldn't"},
        {"shouldnt't", "shouldn't"}, {"couldnt't", "couldn't"},
        {"wasnt't", "wasn't"}, {"werent't", "weren't"},
        {"its's", "it's"}, {"that's", "that's"}, {"thats's", "that's"},
        {"there's", "there's"}, {"theres's", "there's"},
        {"youre're", "you're"}, {"were're", "we're"}, {"theyre're", "they're"},
        {"i'am", "I'm"}, {"i'v", "I've"}, {"i'will", "I'll"}
    };

    auto it = typo_rewrites.find(to_lower_ascii(text));
    return (it != typo_rewrites.end()) ? it->second : text;
}

} // namespace text_utils
} // namespace linuxcomplete
