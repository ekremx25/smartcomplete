#pragma once

#include <string>

namespace linuxcomplete {
namespace text_utils {

// ASCII lowercase (A-Z only, safe for English text).
std::string to_lower_ascii(std::string text);

// Lowercase + strip apostrophes — for fuzzy prefix matching.
std::string fold_for_matching(std::string text);

// Capitalize first letter if lowercase ASCII.
std::string capitalize_for_display(const std::string& word);

// Check if `word` starts with `buffer` after folding.
bool is_prefix_match(const std::string& buffer, const std::string& word);

// Match the case of `word` to the first character of `buffer`.
std::string match_case_to_buffer(const std::string& buffer, std::string word);

// Fix common contraction double-typing (e.g. "dont't" → "don't").
std::string normalize_contraction_typo(std::string text);

} // namespace text_utils
} // namespace linuxcomplete
