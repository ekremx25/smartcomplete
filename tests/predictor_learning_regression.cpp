#include "predictor/predictor.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

void write_file(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    out << content;
}

bool file_contains(const fs::path& path, const std::string& needle) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

int read_freq_value(const fs::path& path, const std::string& word) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return -1;
    }

    std::string entry;
    int freq = 0;
    while (in >> entry >> freq) {
        if (entry == word) {
            return freq;
        }
    }
    return -1;
}

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ok = false;
    }
}

} // namespace

int main() {
    bool ok = true;

    const fs::path temp_root =
        fs::temp_directory_path() / ("linuxcomplete_predictor_test_" + std::to_string(::getpid()));
    const fs::path dict_dir = temp_root / "dict";
    const fs::path freq_dir = temp_root / "frequency";
    const fs::path ngram_dir = temp_root / "ngram";
    const fs::path rules_dir = temp_root / "rules";
    const fs::path home_dir = temp_root / "home";

    fs::create_directories(home_dir);
    setenv("HOME", home_dir.c_str(), 1);

    write_file(dict_dir / "en_US.txt", "how\nare\nyou\ndoing\nhello\n");
    write_file(dict_dir / "tr_TR.txt", "merhaba\n");
    write_file(freq_dir / "en_freq.txt", "how 10\nare 9\nyou 8\ndoing 7\nhello 6\n");
    write_file(freq_dir / "tr_freq.txt", "merhaba 5\n");
    write_file(ngram_dir / "en_bigrams.txt", "");
    write_file(ngram_dir / "tr_bigrams.txt", "");
    write_file(ngram_dir / "en_phrases.txt", "");
    write_file(rules_dir / "en_grammar_pair_rules.txt", "");
    write_file(rules_dir / "en_grammar_triple_rules.txt", "");
    write_file(rules_dir / "en_typo_map.txt", "");
    write_file(rules_dir / "tr_phrase_rules.txt", "");
    write_file(rules_dir / "tr_context_rules.txt", "");

    linuxcomplete::PredictorConfig config;
    config.dict_dir = dict_dir.string();
    config.user_dict_path = (home_dir / ".local/share/linuxcomplete/user/learned.txt").string();

    {
        linuxcomplete::Predictor predictor(config);
        expect(predictor.init(), "predictor should initialize", ok);

        predictor.on_word_accepted("how");
        predictor.on_word_accepted("are");
        predictor.on_text_accepted("you doing");
        predictor.save();
    }

    const fs::path learned_words = home_dir / ".local/share/linuxcomplete/user/learned.txt";
    const fs::path learned_bigrams = home_dir / ".local/share/linuxcomplete/user/learned_bigrams.txt";
    const fs::path user_freq = home_dir / ".local/share/linuxcomplete/user/user_freq.txt";

    expect(!file_contains(learned_words, "how "),
           "existing dictionary words should not be persisted into learned.txt", ok);
    expect(!file_contains(learned_words, "you doing"),
           "multi-word phrases must not be persisted as a single learned word", ok);
    expect(file_contains(learned_bigrams, "how are 2"),
           "accepted words should still teach sequential bigrams", ok);
    expect(file_contains(learned_bigrams, "are you 2"),
           "phrase acceptance should learn the first token transition", ok);
    expect(file_contains(learned_bigrams, "you doing 2"),
           "phrase acceptance should learn internal phrase transitions", ok);
    expect(file_contains(user_freq, "you 1"),
           "phrase acceptance should increase user frequency per token", ok);
    expect(file_contains(user_freq, "doing 1"),
           "all tokens from an accepted phrase should be tracked individually", ok);

    {
        linuxcomplete::Predictor predictor(config);
        expect(predictor.init(), "predictor should reinitialize from persisted state", ok);
        predictor.on_word_accepted("how");
        predictor.save();
    }

    expect(read_freq_value(user_freq, "how") == 2,
           "reloading should preserve a single frequency stream without double-counting", ok);
    expect(!file_contains(learned_words, "how 2"),
           "existing dictionary words should remain out of learned.txt after reload", ok);

    fs::remove_all(temp_root);

    if (!ok) {
        return 1;
    }

    std::cout << "Predictor learning regression checks passed.\n";
    return 0;
}
