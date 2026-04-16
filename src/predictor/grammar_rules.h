#pragma once
#include <vector>
#include <string>
#include <utility>

namespace linuxcomplete {

struct GrammarPairRule {
    std::string prev;
    std::string cand;
    int score;
};

struct GrammarTripleRule {
    std::string prev2;
    std::string prev1;
    std::string cand;
    int score;
};

void set_rules_data_dir(const std::string& dir);

// English grammar rules (verb tense, modal, preposition patterns)
const std::vector<GrammarPairRule>& english_grammar_rules();
const std::vector<GrammarTripleRule>& english_grammar_triple_rules();
const std::vector<std::pair<std::string, std::string>>& english_typo_rules();

// Phrase bonus and context bonus rules (loaded from data files, not hardcoded)
const std::vector<GrammarPairRule>& english_phrase_bonus_rules();
const std::vector<GrammarTripleRule>& english_context_bonus_rules();

// O(1) indexed lookups — built lazily from the rule vectors above.
int lookup_pair_score(const std::string& prev, const std::string& cand);
int lookup_triple_score(const std::string& prev2, const std::string& prev1, const std::string& cand);
int lookup_phrase_bonus(const std::string& prev, const std::string& cand);
int lookup_context_bonus(const std::string& prev2, const std::string& prev1, const std::string& cand);

} // namespace linuxcomplete
