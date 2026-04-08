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

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ok = false;
    }
}

linuxcomplete::PredictorConfig make_config(const fs::path& root) {
    linuxcomplete::PredictorConfig config;
    config.dict_dir = (root / "dict").string();
    config.user_dict_path = (root / "home/.local/share/linuxcomplete/user/learned.txt").string();
    config.max_candidates = 5;
    return config;
}

void seed_common_files(const fs::path& root) {
    write_file(root / "dict/en_US.txt",
               "how\nare\nyou\ndoing\ntoday\nweather\nworks\nfix\nhello\n");
    write_file(root / "dict/tr_TR.txt",
               "merhaba\nbugün\nhava\nnasılsın\n");
    write_file(root / "frequency/en_freq.txt",
               "how 10\nare 9\nyou 8\ndoing 7\ntoday 6\nweather 5\nworks 4\nfix 3\nhello 2\n");
    write_file(root / "frequency/tr_freq.txt",
               "merhaba 10\nbugün 8\nhava 7\nnasılsın 6\n");
    write_file(root / "ngram/tr_bigrams.txt", "");
    write_file(root / "rules/en_grammar_pair_rules.txt", "");
    write_file(root / "rules/en_grammar_triple_rules.txt", "");
    write_file(root / "rules/en_typo_map.txt", "");
    write_file(root / "rules/tr_phrase_rules.txt", "");
    write_file(root / "rules/tr_context_rules.txt", "");
}

} // namespace

int main() {
    bool ok = true;

    {
        const fs::path root =
            fs::temp_directory_path() / ("linuxcomplete_quality_phrase_" + std::to_string(::getpid()));
        fs::create_directories(root / "home");
        setenv("HOME", (root / "home").c_str(), 1);
        seed_common_files(root);
        write_file(root / "ngram/en_bigrams.txt",
                   "are you 40\nare today 20\n");
        write_file(root / "ngram/en_phrases.txt",
                   "how are you doing 500\nhow are you today 490\n");

        linuxcomplete::Predictor predictor(make_config(root));
        expect(predictor.init(), "predictor should initialize for phrase quality test", ok);
        predictor.on_word_accepted("how");
        predictor.on_word_accepted("are");

        const auto next = predictor.predict_next_word();
        expect(!next.empty(), "next-word predictions should not be empty", ok);
        expect(next.front().word == "you doing",
               "highest-ranked phrase completion should stay first", ok);
        expect(next.size() >= 2 && next[1].word == "you today",
               "second phrase completion should be preserved after deduplication", ok);

        fs::remove_all(root);
    }

    {
        const fs::path root =
            fs::temp_directory_path() / ("linuxcomplete_quality_lang_" + std::to_string(::getpid()));
        fs::create_directories(root / "home");
        setenv("HOME", (root / "home").c_str(), 1);
        seed_common_files(root);
        write_file(root / "ngram/en_bigrams.txt",
                   "bugün you 30\nbugün today 20\nbugün weather 10\n");
        write_file(root / "ngram/en_phrases.txt", "");

        linuxcomplete::Predictor predictor(make_config(root));
        expect(predictor.init(), "predictor should initialize for language filter test", ok);
        predictor.on_word_accepted("bugün");

        const auto next = predictor.predict_next_word();
        bool has_english = false;
        for (const auto& candidate : next) {
            if (candidate.word == "you" || candidate.word == "today" || candidate.word == "weather") {
                has_english = true;
            }
        }
        expect(!has_english,
               "Turkish context should not surface English next-word suggestions", ok);

        fs::remove_all(root);
    }

    if (!ok) {
        return 1;
    }

    std::cout << "Predictor quality regression checks passed.\n";
    return 0;
}
