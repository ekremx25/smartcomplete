#include "predictor/grammar_rules.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace linuxcomplete {

namespace {

struct RuleStore {
    std::vector<GrammarPairRule> tr_phrase_rules;
    std::vector<GrammarTripleRule> tr_context_rules;
    std::vector<GrammarPairRule> en_pair_rules;
    std::vector<GrammarTripleRule> en_triple_rules;
    std::vector<std::pair<std::string, std::string>> en_typo_rules;
    std::string loaded_dir;
    bool loaded = false;
};

RuleStore& store() {
    static RuleStore data;
    return data;
}

std::vector<std::string> split_tab_fields(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, '\t')) {
        fields.push_back(field);
    }
    return fields;
}

bool load_pair_rules_file(const std::filesystem::path& path,
                          std::vector<GrammarPairRule>& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        auto fields = split_tab_fields(line);
        if (fields.size() != 3) {
            continue;
        }
        try {
            out.push_back({fields[0], fields[1], std::stoi(fields[2])});
        } catch (...) {
            continue;
        }
    }
    return true;
}

bool load_triple_rules_file(const std::filesystem::path& path,
                            std::vector<GrammarTripleRule>& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        auto fields = split_tab_fields(line);
        if (fields.size() != 4) {
            continue;
        }
        try {
            out.push_back({fields[0], fields[1], fields[2], std::stoi(fields[3])});
        } catch (...) {
            continue;
        }
    }
    return true;
}

bool load_typo_rules_file(const std::filesystem::path& path,
                          std::vector<std::pair<std::string, std::string>>& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        auto fields = split_tab_fields(line);
        if (fields.size() != 2) {
            continue;
        }
        out.emplace_back(fields[0], fields[1]);
    }
    return true;
}

void load_rules_from_dir(const std::string& dir) {
    auto& data = store();
    if (data.loaded && data.loaded_dir == dir) {
        return;
    }

    data = RuleStore{};
    data.loaded_dir = dir;

    const std::filesystem::path base(dir);

    load_pair_rules_file(base / "tr_phrase_rules.txt", data.tr_phrase_rules);
    load_triple_rules_file(base / "tr_context_rules.txt", data.tr_context_rules);
    load_pair_rules_file(base / "en_grammar_pair_rules.txt", data.en_pair_rules);
    load_triple_rules_file(base / "en_grammar_triple_rules.txt", data.en_triple_rules);
    load_typo_rules_file(base / "en_typo_map.txt", data.en_typo_rules);

    data.loaded = true;

    std::cout << "[LinuxComplete] Loaded rule data from " << dir
              << " (en pair=" << data.en_pair_rules.size()
              << ", en triple=" << data.en_triple_rules.size()
              << ", tr pair=" << data.tr_phrase_rules.size()
              << ", tr triple=" << data.tr_context_rules.size()
              << ", typo=" << data.en_typo_rules.size() << ")" << std::endl;
}

void ensure_loaded() {
    auto& data = store();
    if (!data.loaded) {
        load_rules_from_dir(RULES_INSTALL_DIR);
    }
}

} // namespace

void set_rules_data_dir(const std::string& dir) {
    load_rules_from_dir(dir);
}

const std::vector<GrammarPairRule>& turkish_phrase_rules() {
    ensure_loaded();
    return store().tr_phrase_rules;
}

const std::vector<GrammarTripleRule>& turkish_context_rules() {
    ensure_loaded();
    return store().tr_context_rules;
}

const std::vector<GrammarPairRule>& english_grammar_rules() {
    ensure_loaded();
    return store().en_pair_rules;
}

const std::vector<GrammarTripleRule>& english_grammar_triple_rules() {
    ensure_loaded();
    return store().en_triple_rules;
}

const std::vector<std::pair<std::string, std::string>>& english_typo_rules() {
    ensure_loaded();
    return store().en_typo_rules;
}

} // namespace linuxcomplete
