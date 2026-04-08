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

// Turkish phrase rules for bigram context
const std::vector<GrammarPairRule>& turkish_phrase_rules();

// Turkish triple context rules
const std::vector<GrammarTripleRule>& turkish_context_rules();

// Additional English grammar rules (verb tense, modal, preposition patterns)
const std::vector<GrammarPairRule>& english_grammar_rules();

// Additional English triple rules
const std::vector<GrammarTripleRule>& english_grammar_triple_rules();

const std::vector<std::pair<std::string, std::string>>& english_typo_rules();

} // namespace linuxcomplete
