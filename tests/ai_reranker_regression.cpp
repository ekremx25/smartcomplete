#include "predictor/ai_reranker.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ok = false;
    }
}

} // namespace

int main() {
    bool ok = true;

    {
        const std::string response = R"({"ranking":["you doing","you today","doing"]})";
        const auto parsed = linuxcomplete::AiReranker::parse_ranked_words(response);
        expect(parsed.size() == 3, "parser should extract ranking entries", ok);
        expect(parsed[0] == "you doing", "parser should preserve order", ok);
    }

    {
        const std::vector<linuxcomplete::Candidate> candidates = {
            {"doing", 300},
            {"you doing", 900},
            {"you today", 850}
        };
        const std::vector<std::string> preferred = {"you today", "you doing"};
        const auto ranked = linuxcomplete::AiReranker::apply_ranking(candidates, preferred);
        expect(ranked.size() == 3, "reranked list should keep all candidates", ok);
        expect(ranked[0].word == "you today", "AI preferred candidate should move to front", ok);
        expect(ranked[1].word == "you doing", "second preferred candidate should follow", ok);
        expect(ranked[2].word == "doing", "unranked candidates should be appended", ok);
    }

    {
        linuxcomplete::AiReranker reranker({false, true, false, "gpt-5-mini", 1200, 160, 8});
        const std::vector<linuxcomplete::Candidate> candidates = {
            {"hello", 100},
            {"help", 90}
        };
        const auto ranked = reranker.rerank(candidates, {"he", "", "", false, false});
        expect(ranked[0].word == "hello", "disabled reranker should preserve order", ok);
        expect(ranked[1].word == "help", "disabled reranker should keep remaining entries", ok);
    }

    {
        setenv("OPENAI_API_KEY", "test-key", 1);
        linuxcomplete::AiReranker reranker({true, true, false, "gpt-5-mini", 1200, 160, 8});
        const std::vector<linuxcomplete::Candidate> confident = {
            {"hello", 500},
            {"help", 120},
            {"held", 90}
        };
        expect(!reranker.should_rerank(confident, {"hel", "", "", false, false}),
               "large score gaps should skip AI under smart fallback", ok);

        const std::vector<linuxcomplete::Candidate> uncertain = {
            {"you're", 260},
            {"your", 220},
            {"you", 210}
        };
        expect(reranker.should_rerank(uncertain, {"you", "", "", false, false}),
               "context-sensitive candidates should trigger AI reranking", ok);
    }

    {
        setenv("OPENAI_API_KEY", "test-key", 1);
        setenv("LINUXCOMPLETE_AI_TEST_RESPONSE", R"({"ranking":["you today","you doing","doing"]})", 1);
        linuxcomplete::AiReranker reranker({true, false, false, "gpt-5-mini", 1200, 160, 8});
        const std::vector<linuxcomplete::Candidate> candidates = {
            {"doing", 300},
            {"you doing", 900},
            {"you today", 850}
        };
        const linuxcomplete::AiRerankRequest request{"", "how", "are", false, true};
        const auto first = reranker.rerank(candidates, request);
        expect(first[0].word == "you today", "injected AI response should reorder candidates", ok);

        setenv("LINUXCOMPLETE_AI_TEST_RESPONSE", R"({"ranking":["doing","you doing","you today"]})", 1);
        const auto second = reranker.rerank(candidates, request);
        expect(second[0].word == "you today", "cache should keep first rerank for identical request", ok);
        unsetenv("LINUXCOMPLETE_AI_TEST_RESPONSE");
    }

    if (!ok) {
        return 1;
    }

    std::cout << "AI reranker regression checks passed.\n";
    return 0;
}
