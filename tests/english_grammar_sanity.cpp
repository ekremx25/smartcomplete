#include "predictor/grammar_rules.h"

#include <iostream>
#include <string>
#include <vector>

using linuxcomplete::GrammarPairRule;
using linuxcomplete::GrammarTripleRule;
using linuxcomplete::english_grammar_rules;
using linuxcomplete::english_grammar_triple_rules;
using linuxcomplete::english_typo_rules;
using linuxcomplete::set_rules_data_dir;

namespace {

bool has_pair_rule(const std::vector<GrammarPairRule>& rules,
                   const std::string& prev,
                   const std::string& cand,
                   int min_score = 1) {
    for (const auto& rule : rules) {
        if (rule.prev == prev && rule.cand == cand && rule.score >= min_score) {
            return true;
        }
    }
    return false;
}

bool has_triple_rule(const std::vector<GrammarTripleRule>& rules,
                     const std::string& prev2,
                     const std::string& prev1,
                     const std::string& cand,
                     int min_score = 1) {
    for (const auto& rule : rules) {
        if (rule.prev2 == prev2 && rule.prev1 == prev1 &&
            rule.cand == cand && rule.score >= min_score) {
            return true;
        }
    }
    return false;
}

bool lacks_positive_triple_rule(const std::vector<GrammarTripleRule>& rules,
                                const std::string& prev2,
                                const std::string& prev1,
                                const std::string& cand) {
    for (const auto& rule : rules) {
        if (rule.prev2 == prev2 && rule.prev1 == prev1 &&
            rule.cand == cand && rule.score > 0) {
            return false;
        }
    }
    return true;
}

bool has_typo_rule(const std::vector<std::pair<std::string, std::string>>& rules,
                   const std::string& typo,
                   const std::string& correction) {
    for (const auto& rule : rules) {
        if (rule.first == typo && rule.second == correction) {
            return true;
        }
    }
    return false;
}

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ok = false;
    }
}

} // namespace

int main() {
    set_rules_data_dir(RULES_SOURCE_DIR);
    const auto& pair_rules = english_grammar_rules();
    const auto& triple_rules = english_grammar_triple_rules();
    const auto& typo_rules = english_typo_rules();

    bool ok = true;

    // Positive checks: natural, high-signal English patterns we want to preserve.
    expect(has_pair_rule(pair_rules, "to", "understand", 150),
           "expected pair rule: to -> understand", ok);
    expect(has_pair_rule(pair_rules, "looking", "for", 150),
           "expected pair rule: looking -> for", ok);
    expect(has_pair_rule(pair_rules, "important", "question", 140),
           "expected pair rule: important -> question", ok);

    expect(has_triple_rule(triple_rules, "i", "am", "working", 150),
           "expected triple rule: i am working", ok);
    expect(has_triple_rule(triple_rules, "it", "is", "important", 150),
           "expected triple rule: it is important", ok);
    expect(has_triple_rule(triple_rules, "the", "important", "issue", 150),
           "expected triple rule: the important issue", ok);
    expect(has_triple_rule(triple_rules, "what", "do", "you", 150),
           "expected triple rule: what do you", ok);
    expect(has_triple_rule(triple_rules, "have", "been", "working", 150),
           "expected triple rule: have been working", ok);
    expect(has_typo_rule(typo_rules, "dont", "don't"),
           "expected typo rule: dont -> don't", ok);

    // Negative checks: unnatural combinations should not be positively generated.
    expect(lacks_positive_triple_rule(triple_rules, "very", "good", "the"),
           "unexpected positive triple rule: very good the", ok);
    expect(lacks_positive_triple_rule(triple_rules, "whether", "do", "you"),
           "unexpected positive triple rule: whether do you", ok);
    expect(lacks_positive_triple_rule(triple_rules, "been", "been", "working"),
           "unexpected positive triple rule: been been working", ok);

    if (!ok) {
        return 1;
    }

    std::cout << "English grammar sanity checks passed.\n";
    std::cout << "Pair rules: " << pair_rules.size() << '\n';
    std::cout << "Triple rules: " << triple_rules.size() << '\n';
    return 0;
}
