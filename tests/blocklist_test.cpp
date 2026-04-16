// Verifies that Predictor::is_program_disabled() correctly identifies
// terminal emulators and other apps where SmartComplete should pass through.
#include "predictor/predictor.h"

#include <iostream>

using linuxcomplete::Predictor;
using linuxcomplete::PredictorConfig;

namespace {

int failures = 0;

#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAIL " << __func__ << ":" << __LINE__ << " " #cond "\n"; \
        failures++; \
    } \
} while (0)

void test_default_blocklist_has_common_terminals() {
    Predictor p({});
    EXPECT(p.is_program_disabled("kitty"));
    EXPECT(p.is_program_disabled("alacritty"));
    EXPECT(p.is_program_disabled("foot"));
    EXPECT(p.is_program_disabled("wezterm"));
    EXPECT(p.is_program_disabled("gnome-terminal"));
    EXPECT(p.is_program_disabled("konsole"));
    EXPECT(p.is_program_disabled("xterm"));
    EXPECT(p.is_program_disabled("tmux"));
    EXPECT(p.is_program_disabled("screen"));
}

void test_non_terminals_are_allowed() {
    Predictor p({});
    EXPECT(!p.is_program_disabled("firefox"));
    EXPECT(!p.is_program_disabled("chromium"));
    EXPECT(!p.is_program_disabled("code"));
    EXPECT(!p.is_program_disabled("thunderbird"));
    EXPECT(!p.is_program_disabled(""));
}

void test_case_insensitive() {
    Predictor p({});
    EXPECT(p.is_program_disabled("Kitty"));
    EXPECT(p.is_program_disabled("ALACRITTY"));
    EXPECT(p.is_program_disabled("GNOME-Terminal"));
}

void test_prefix_match_for_custom_class_variants() {
    // Users often set custom window classes like "kittyfloat" for floating windows.
    // Prefix match keeps the blocklist usable without requiring every variant.
    Predictor p({});
    EXPECT(p.is_program_disabled("kittyfloat"));
    EXPECT(p.is_program_disabled("kitty-scratchpad"));
    EXPECT(p.is_program_disabled("alacritty-dropdown"));
    // But substrings in the middle should NOT match:
    EXPECT(!p.is_program_disabled("my-kitty-app"));
    EXPECT(!p.is_program_disabled("super-alacritty"));
}

void test_custom_blocklist_replaces_default() {
    PredictorConfig cfg;
    cfg.disabled_programs = {"vim", "emacs"};
    Predictor p(cfg);
    EXPECT(p.is_program_disabled("vim"));
    EXPECT(p.is_program_disabled("emacs"));
    EXPECT(!p.is_program_disabled("kitty"));  // defaults were replaced
}

void test_empty_blocklist_allows_everything() {
    PredictorConfig cfg;
    cfg.disabled_programs = {};
    Predictor p(cfg);
    EXPECT(!p.is_program_disabled("kitty"));
    EXPECT(!p.is_program_disabled("firefox"));
}

} // namespace

int main() {
    test_default_blocklist_has_common_terminals();
    test_non_terminals_are_allowed();
    test_case_insensitive();
    test_prefix_match_for_custom_class_variants();
    test_custom_blocklist_replaces_default();
    test_empty_blocklist_allows_everything();

    if (failures == 0) {
        std::cout << "[blocklist_test] All tests passed.\n";
        return 0;
    }
    std::cerr << "[blocklist_test] " << failures << " failure(s).\n";
    return 1;
}
