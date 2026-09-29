#include "aho_corasick.h"

#include <queue>
#include <stdexcept>

namespace
{
    constexpr int ALPHABET_SIZE = 256;
}

AhoCorasick::Node::Node()
    : next(ALPHABET_SIZE, -1)
{
}

void AhoCorasick::addPattern(const std::string& pattern)
{
    if (built)
    {
        throw std::logic_error(
            "AhoCorasick::addPattern called after build()");
    }

    // An empty pattern would "match" at every position with end < start.
    if (pattern.empty())
    {
        throw std::invalid_argument(
            "AhoCorasick::addPattern called with an empty pattern");
    }

    // Ensure the root node exists before inserting the pattern.
    if (trie.empty())
    {
        trie.emplace_back();
    }

    int current = 0;

    for (unsigned char character : pattern)
    {
        if (trie[current].next[character] == -1)
        {
            trie[current].next[character] =
                static_cast<int>(trie.size());

            trie.emplace_back();
        }

        current = trie[current].next[character];
    }

    patterns.push_back(pattern);

    trie[current].output.push_back(
        static_cast<int>(patterns.size()) - 1
    );
}

void AhoCorasick::build()
{
    if (built)
    {
        return;
    }

    if (trie.empty())
    {
        trie.emplace_back();
    }

    std::queue<int> queue;

    for (int character = 0;
        character < ALPHABET_SIZE;
        ++character)
    {
        int child = trie[0].next[character];

        if (child == -1)
        {
            trie[0].next[character] = 0;
        }
        else
        {
            trie[child].failure = 0;
            queue.push(child);
        }
    }

    while (!queue.empty())
    {
        int current = queue.front();
        queue.pop();

        int failure = trie[current].failure;

        for (int patternIndex : trie[failure].output)
        {
            trie[current].output.push_back(patternIndex);
        }

        for (int character = 0;
            character < ALPHABET_SIZE;
            ++character)
        {
            int child = trie[current].next[character];

            if (child == -1)
            {
                trie[current].next[character] =
                    trie[failure].next[character];
            }
            else
            {
                trie[child].failure =
                    trie[failure].next[character];

                queue.push(child);
            }
        }
    }

    built = true;
}

std::vector<Match> AhoCorasick::search(
    const std::string& text) const
{
    if (!built)
    {
        throw std::logic_error(
            "AhoCorasick::search called before build()");
    }

    std::vector<Match> matches;

    int current = 0;

    for (int position = 0;
        position < static_cast<int>(text.size());
        ++position)
    {
        unsigned char character = text[position];

        current = trie[current].next[character];

        for (int patternIndex : trie[current].output)
        {
            int patternLength =
                static_cast<int>(
                    patterns[patternIndex].size()
                    );

            matches.push_back({
                patternIndex,
                position - patternLength + 1,
                position
                });
        }
    }

    return matches;
}

const std::string& AhoCorasick::getPattern(
    int patternIndex
) const
{
    return patterns[patternIndex];
}

int AhoCorasick::getPatternCount() const
{
    return static_cast<int>(patterns.size());
}