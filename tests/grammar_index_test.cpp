#include "predictor/grammar_rules.h"

#include <iostream>
#include <string>

using namespace linuxcomplete;

namespace {

int failures = 0;

#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAIL " << __func__ << ":" << __LINE__ << " " #cond "\n"; \
        failures++; \
    } \
} while (0)

void test_index_built_after_load() {
    set_rules_data_dir(RULES_SOURCE_DIR);

    // The index should mirror the vector content.
    const auto& pair_rules = english_grammar_rules();
    if (pair_rules.empty()) {
        std::cerr << "[skip] no grammar pair rules loaded\n";
        return;
    }

    // Pick a deterministic rule and verify O(1) lookup returns the same score.
    const auto& r = pair_rules[0];
    const int indexed = lookup_pair_score(r.prev, r.cand);
    EXPECT(indexed != 0);
    // Score should be at least the single rule's score (could be summed if dup keys exist).
    EXPECT(indexed * (r.score >= 0 ? 1 : -1) >= r.score * (r.score >= 0 ? 1 : -1));
}

void test_lookup_returns_zero_for_missing() {
    set_rules_data_dir(RULES_SOURCE_DIR);
    EXPECT(lookup_pair_score("__definitely_not_a_rule__", "__nope__") == 0);
    EXPECT(lookup_triple_score("xxx", "yyy", "zzz") == 0);
    EXPECT(lookup_phrase_bonus("qqq", "rrr") == 0);
    EXPECT(lookup_context_bonus("aaa", "bbb", "ccc") == 0);
}

void test_phrase_bonus_index_built() {
    set_rules_data_dir(RULES_SOURCE_DIR);
    const auto& bonus = english_phrase_bonus_rules();
    if (bonus.empty()) {
        std::cerr << "[skip] no phrase bonus rules loaded\n";
        return;
    }
    const auto& r = bonus[0];
    const int indexed = lookup_phrase_bonus(r.prev, r.cand);
    // Non-zero confirms the index was built.
    EXPECT(indexed != 0);
}

void test_context_bonus_index_built() {
    set_rules_data_dir(RULES_SOURCE_DIR);
    const auto& bonus = english_context_bonus_rules();
    if (bonus.empty()) {
        std::cerr << "[skip] no context bonus rules loaded\n";
        return;
    }
    const auto& r = bonus[0];
    const int indexed = lookup_context_bonus(r.prev2, r.prev1, r.cand);
    EXPECT(indexed != 0);
}

void test_penalty_rules_migrated() {
    // These rules were moved from english_filter_penalty() to the phrase bonus file.
    set_rules_data_dir(RULES_SOURCE_DIR);
    EXPECT(lookup_phrase_bonus("love", "us") <= -420);
    EXPECT(lookup_phrase_bonus("my", "wife") <= -420);
    EXPECT(lookup_phrase_bonus("how", "working") <= -520);
}

} // namespace

int main() {
    test_index_built_after_load();
    test_lookup_returns_zero_for_missing();
    test_phrase_bonus_index_built();
    test_context_bonus_index_built();
    test_penalty_rules_migrated();

    if (failures == 0) {
        std::cout << "[grammar_index_test] All tests passed.\n";
        return 0;
    }
    std::cerr << "[grammar_index_test] " << failures << " failure(s).\n";
    return 1;
}
