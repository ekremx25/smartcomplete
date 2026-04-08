#include "predictor/ngram.h"
#include "predictor/phrase.h"

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

} // namespace

int main() {
    bool ok = true;
    const fs::path root =
        fs::temp_directory_path() / ("linuxcomplete_data_quality_" + std::to_string(::getpid()));
    fs::create_directories(root);

    {
        write_file(root / "dupe_bigrams.txt",
                   "hello world 10\n"
                   "hello world 25\n"
                   "hello there 5\n");
        linuxcomplete::NgramModel model;
        expect(model.load((root / "dupe_bigrams.txt").string()),
               "ngram model should load duplicate data", ok);
        const auto next = model.predict_next("hello", 5);
        expect(next.size() == 2, "duplicate bigrams should collapse into unique predictions", ok);
        expect(!next.empty() && next.front().word == "world" && next.front().score == 25,
               "duplicate bigram entries should keep the strongest frequency", ok);
    }

    {
        write_file(root / "dupe_phrases.txt",
                   "how are you doing 200\n"
                   "how are you doing 500\n"
                   "how are you today 300\n");
        linuxcomplete::PhraseModel model;
        expect(model.load((root / "dupe_phrases.txt").string()),
               "phrase model should load duplicate data", ok);
        const auto next = model.predict("how", "are", 5);
        expect(next.size() == 2, "duplicate phrase entries should collapse into unique completions", ok);
        expect(!next.empty() && next.front().completion == "you doing" && next.front().score == 500,
               "duplicate phrase entries should keep the strongest score", ok);
    }

    fs::remove_all(root);

    if (!ok) {
        return 1;
    }

    std::cout << "Data quality regression checks passed.\n";
    return 0;
}
