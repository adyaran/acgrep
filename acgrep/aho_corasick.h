#pragma once

#include <string>
#include <vector>

struct Match
{
    int patternIndex;
    int start;
    int end;
};

// Usage: addPattern() any number of times, then build() once, then search().
// addPattern() after build(), or search() before build(), throws std::logic_error.
// addPattern() with an empty pattern throws std::invalid_argument.
class AhoCorasick
{
public:
    void addPattern(const std::string& pattern);
    void build();
    std::vector<Match> search(const std::string& text) const;

    const std::string& getPattern(int patternIndex) const;
    int getPatternCount() const;

private:
    struct Node
    {
        std::vector<int> next;
        int failure = 0;
        std::vector<int> output;

        Node();
    };

    std::vector<Node> trie;
    std::vector<std::string> patterns;
    bool built = false;
};