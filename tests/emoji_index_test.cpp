#include "predictor/emoji.h"

#include <iostream>
#include <string>

using linuxcomplete::EmojiModel;

namespace {

int failures = 0;

#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAIL " << __func__ << ":" << __LINE__ << " " #cond "\n"; \
        failures++; \
    } \
} while (0)

void test_loads_from_tsv() {
    EmojiModel model(EMOJI_SOURCE_DIR "/en_emoji.tsv");
    EXPECT(model.size() > 100);
}

void test_empty_path_gives_empty_model() {
    EmojiModel model("");
    EXPECT(model.size() == 0);
    EXPECT(model.predict(":sm", 5).empty());
}

void test_prefix_lookup_finds_smile() {
    EmojiModel model(EMOJI_SOURCE_DIR "/en_emoji.tsv");
    auto results = model.predict(":sm", 5);
    // Should include :smile at least.
    bool found_smile = false;
    for (const auto& r : results) {
        if (r.shortcode == ":smile") found_smile = true;
    }
    EXPECT(found_smile);
}

void test_case_insensitive_prefix() {
    EmojiModel model(EMOJI_SOURCE_DIR "/en_emoji.tsv");
    auto lower = model.predict(":he", 5);
    auto upper = model.predict(":HE", 5);
    EXPECT(lower.size() == upper.size());
    if (!lower.empty() && !upper.empty()) {
        EXPECT(lower[0].shortcode == upper[0].shortcode);
    }
}

void test_results_sorted_by_score() {
    EmojiModel model(EMOJI_SOURCE_DIR "/en_emoji.tsv");
    auto results = model.predict(":h", 10);
    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT(results[i - 1].score >= results[i].score);
    }
}

void test_max_results_honored() {
    EmojiModel model(EMOJI_SOURCE_DIR "/en_emoji.tsv");
    auto results = model.predict(":", 3);
    EXPECT(results.size() <= 3);
}

void test_unknown_prefix_returns_empty() {
    EmojiModel model(EMOJI_SOURCE_DIR "/en_emoji.tsv");
    auto results = model.predict(":zzzzzzz_not_a_real_emoji", 5);
    EXPECT(results.empty());
}

} // namespace

int main() {
    test_loads_from_tsv();
    test_empty_path_gives_empty_model();
    test_prefix_lookup_finds_smile();
    test_case_insensitive_prefix();
    test_results_sorted_by_score();
    test_max_results_honored();
    test_unknown_prefix_returns_empty();

    if (failures == 0) {
        std::cout << "[emoji_index_test] All tests passed.\n";
        return 0;
    }
    std::cerr << "[emoji_index_test] " << failures << " failure(s).\n";
    return 1;
}
