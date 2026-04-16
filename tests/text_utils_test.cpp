#include "predictor/text_utils.h"

#include <iostream>
#include <string>

using namespace linuxcomplete::text_utils;

namespace {

int failures = 0;

#define EXPECT_EQ(actual, expected) do { \
    if ((actual) != (expected)) { \
        std::cerr << "FAIL " << __func__ << ":" << __LINE__ \
                  << " expected \"" << (expected) << "\" got \"" << (actual) << "\"\n"; \
        failures++; \
    } \
} while (0)

#define EXPECT_TRUE(cond) do { \
    if (!(cond)) { \
        std::cerr << "FAIL " << __func__ << ":" << __LINE__ << " " #cond "\n"; \
        failures++; \
    } \
} while (0)

void test_to_lower_ascii() {
    EXPECT_EQ(to_lower_ascii("HELLO"), "hello");
    EXPECT_EQ(to_lower_ascii("Hello"), "hello");
    EXPECT_EQ(to_lower_ascii("hello"), "hello");
    EXPECT_EQ(to_lower_ascii(""), "");
    EXPECT_EQ(to_lower_ascii("123"), "123");
    EXPECT_EQ(to_lower_ascii("Mix3D"), "mix3d");
    // Non-ASCII bytes must not be corrupted.
    EXPECT_EQ(to_lower_ascii("Héllo"), "héllo");
}

void test_fold_for_matching() {
    EXPECT_EQ(fold_for_matching("Don't"), "dont");
    EXPECT_EQ(fold_for_matching("I'M"), "im");
    EXPECT_EQ(fold_for_matching("Hello"), "hello");
    EXPECT_EQ(fold_for_matching(""), "");
    EXPECT_EQ(fold_for_matching("'''"), "");
}

void test_capitalize_for_display() {
    EXPECT_EQ(capitalize_for_display("hello"), "Hello");
    EXPECT_EQ(capitalize_for_display("Hello"), "Hello");
    EXPECT_EQ(capitalize_for_display(""), "");
    EXPECT_EQ(capitalize_for_display("123abc"), "123abc");
    EXPECT_EQ(capitalize_for_display("a"), "A");
}

void test_is_prefix_match() {
    EXPECT_TRUE(is_prefix_match("hel", "Hello"));
    EXPECT_TRUE(is_prefix_match("Don", "don't"));
    EXPECT_TRUE(is_prefix_match("", "anything"));
    EXPECT_TRUE(is_prefix_match("i'm", "IM"));
    EXPECT_TRUE(!is_prefix_match("xyz", "hello"));
    EXPECT_TRUE(!is_prefix_match("hello", "hi"));
}

void test_match_case_to_buffer() {
    // Lowercase buffer → word should be lowercased.
    EXPECT_EQ(match_case_to_buffer("h", "Hello"), "hello");
    // Uppercase buffer → word preserved.
    EXPECT_EQ(match_case_to_buffer("H", "Hello"), "Hello");
    // Empty buffer → word unchanged.
    EXPECT_EQ(match_case_to_buffer("", "Hello"), "Hello");
}

void test_normalize_contraction_typo() {
    EXPECT_EQ(normalize_contraction_typo("dont't"), "don't");
    EXPECT_EQ(normalize_contraction_typo("cant't"), "can't");
    EXPECT_EQ(normalize_contraction_typo("i'am"), "I'm");
    // Non-typo passes through unchanged.
    EXPECT_EQ(normalize_contraction_typo("hello"), "hello");
}

} // namespace

int main() {
    test_to_lower_ascii();
    test_fold_for_matching();
    test_capitalize_for_display();
    test_is_prefix_match();
    test_match_case_to_buffer();
    test_normalize_contraction_typo();

    if (failures == 0) {
        std::cout << "[text_utils_test] All tests passed.\n";
        return 0;
    }
    std::cerr << "[text_utils_test] " << failures << " failure(s).\n";
    return 1;
}
