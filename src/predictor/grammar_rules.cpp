#include "predictor/grammar_rules.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

namespace linuxcomplete {

namespace {

// Composite key for O(1) pair/triple lookup.
struct PairKey {
    std::string a, b;
    bool operator==(const PairKey& o) const { return a == o.a && b == o.b; }
};
struct TripleKey {
    std::string a, b, c;
    bool operator==(const TripleKey& o) const { return a == o.a && b == o.b && c == o.c; }
};
struct PairKeyHash {
    size_t operator()(const PairKey& k) const {
        size_t h = std::hash<std::string>{}(k.a);
        h ^= std::hash<std::string>{}(k.b) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};
struct TripleKeyHash {
    size_t operator()(const TripleKey& k) const {
        size_t h = std::hash<std::string>{}(k.a);
        h ^= std::hash<std::string>{}(k.b) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<std::string>{}(k.c) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

using PairIndex   = std::unordered_map<PairKey, int, PairKeyHash>;
using TripleIndex = std::unordered_map<TripleKey, int, TripleKeyHash>;

struct RuleStore {
    std::vector<GrammarPairRule> en_pair_rules;
    std::vector<GrammarTripleRule> en_triple_rules;
    std::vector<std::pair<std::string, std::string>> en_typo_rules;
    std::vector<GrammarPairRule> en_phrase_bonus;
    std::vector<GrammarTripleRule> en_context_bonus;
    // O(1) indexes — built after loading.
    PairIndex   pair_index;
    TripleIndex triple_index;
    PairIndex   phrase_bonus_index;
    TripleIndex context_bonus_index;
    std::string loaded_dir;
    bool loaded = false;
};

void build_pair_index(PairIndex& idx, const std::vector<GrammarPairRule>& rules) {
    idx.clear();
    idx.reserve(rules.size());
    for (const auto& r : rules) {
        auto [it, inserted] = idx.emplace(PairKey{r.prev, r.cand}, r.score);
        if (!inserted) it->second += r.score; // sum duplicate keys
    }
}

void build_triple_index(TripleIndex& idx, const std::vector<GrammarTripleRule>& rules) {
    idx.clear();
    idx.reserve(rules.size());
    for (const auto& r : rules) {
        auto [it, inserted] = idx.emplace(TripleKey{r.prev2, r.prev1, r.cand}, r.score);
        if (!inserted) it->second += r.score;
    }
}

int query_pair(const PairIndex& idx, const std::string& a, const std::string& b) {
    auto it = idx.find(PairKey{a, b});
    return (it != idx.end()) ? it->second : 0;
}

int query_triple(const TripleIndex& idx, const std::string& a, const std::string& b, const std::string& c) {
    auto it = idx.find(TripleKey{a, b, c});
    return (it != idx.end()) ? it->second : 0;
}

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

    load_pair_rules_file(base / "en_grammar_pair_rules.txt", data.en_pair_rules);
    load_triple_rules_file(base / "en_grammar_triple_rules.txt", data.en_triple_rules);
    load_typo_rules_file(base / "en_typo_map.txt", data.en_typo_rules);
    load_pair_rules_file(base / "en_phrase_bonus_rules.txt", data.en_phrase_bonus);
    load_triple_rules_file(base / "en_context_bonus_rules.txt", data.en_context_bonus);

    // Build O(1) hash map indexes.
    build_pair_index(data.pair_index, data.en_pair_rules);
    build_triple_index(data.triple_index, data.en_triple_rules);
    build_pair_index(data.phrase_bonus_index, data.en_phrase_bonus);
    build_triple_index(data.context_bonus_index, data.en_context_bonus);

    data.loaded = true;

    std::cout << "[SmartComplete] Loaded rule data from " << dir
              << " (grammar=" << data.en_pair_rules.size()
              << ", triple=" << data.en_triple_rules.size()
              << ", typo=" << data.en_typo_rules.size()
              << ", phrase_bonus=" << data.en_phrase_bonus.size()
              << ", context_bonus=" << data.en_context_bonus.size() << ")" << std::endl;
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

const std::vector<GrammarPairRule>& english_phrase_bonus_rules() {
    ensure_loaded();
    return store().en_phrase_bonus;
}

const std::vector<GrammarTripleRule>& english_context_bonus_rules() {
    ensure_loaded();
    return store().en_context_bonus;
}

// O(1) indexed lookups.
int lookup_pair_score(const std::string& prev, const std::string& cand) {
    ensure_loaded();
    return query_pair(store().pair_index, prev, cand);
}

int lookup_triple_score(const std::string& prev2, const std::string& prev1, const std::string& cand) {
    ensure_loaded();
    return query_triple(store().triple_index, prev2, prev1, cand);
}

int lookup_phrase_bonus(const std::string& prev, const std::string& cand) {
    ensure_loaded();
    return query_pair(store().phrase_bonus_index, prev, cand);
}

int lookup_context_bonus(const std::string& prev2, const std::string& prev1, const std::string& cand) {
    ensure_loaded();
    return query_triple(store().context_bonus_index, prev2, prev1, cand);
}

} // namespace linuxcomplete
