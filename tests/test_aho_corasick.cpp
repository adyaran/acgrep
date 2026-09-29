#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../acgrep/aho_corasick.h"

// Unlike assert(), CHECK is not compiled out when NDEBUG is defined,
// so these tests really run in Release builds too.
#define CHECK(condition)                                                \
    do                                                                  \
    {                                                                   \
        if (!(condition))                                               \
        {                                                               \
            std::cerr << __FILE__ << ':' << __LINE__                    \
                      << ": CHECK failed: " #condition "\n";            \
            std::exit(1);                                               \
        }                                                               \
    } while (false)

// True if `matches` contains exactly this (pattern, start, end) triple.
static bool hasMatch(
    const std::vector<Match>& matches,
    int patternIndex,
    int start,
    int end)
{
    for (const Match& match : matches)
    {
        if (match.patternIndex == patternIndex &&
            match.start == start &&
            match.end == end)
        {
            return true;
        }
    }

    return false;
}

static int countForPattern(const std::vector<Match>& matches, int patternIndex)
{
    int count = 0;

    for (const Match& match : matches)
    {
        if (match.patternIndex == patternIndex)
        {
            ++count;
        }
    }

    return count;
}

template <typename Function>
static bool throwsInvalidArgument(Function function)
{
    try
    {
        function();
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }

    return false;
}

template <typename Function>
static bool throwsLogicError(Function function)
{
    try
    {
        function();
    }
    catch (const std::logic_error&)
    {
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Core matching
// ---------------------------------------------------------------------------

void testClassicExample()
{
    AhoCorasick automaton;

    automaton.addPattern("he");
    automaton.addPattern("she");
    automaton.addPattern("his");
    automaton.addPattern("hers");

    automaton.build();

    auto matches = automaton.search("ushers");

    CHECK(matches.size() == 3);
    CHECK(hasMatch(matches, 0, 2, 3));   // he
    CHECK(hasMatch(matches, 1, 1, 3));   // she
    CHECK(hasMatch(matches, 3, 2, 5));   // hers

    std::cout << "testClassicExample passed\n";
}

void testSubstringPatternsAreReported()
{
    AhoCorasick automaton;

    automaton.addPattern("he");
    automaton.addPattern("she");
    automaton.addPattern("hers");

    automaton.build();

    auto matches = automaton.search("she");

    // "she" ends here, and "he" is a suffix of it. "hers" does not match.
    CHECK(matches.size() == 2);
    CHECK(countForPattern(matches, 0) == 1);
    CHECK(countForPattern(matches, 1) == 1);
    CHECK(countForPattern(matches, 2) == 0);

    std::cout << "testSubstringPatternsAreReported passed\n";
}

void testNoMatches()
{
    AhoCorasick automaton;

    automaton.addPattern("cat");
    automaton.addPattern("dog");

    automaton.build();

    CHECK(automaton.search("hello world").empty());

    std::cout << "testNoMatches passed\n";
}

void testSinglePattern()
{
    AhoCorasick automaton;

    automaton.addPattern("abc");

    automaton.build();

    auto matches = automaton.search("xxabcxx");

    CHECK(matches.size() == 1);
    CHECK(hasMatch(matches, 0, 2, 4));

    std::cout << "testSinglePattern passed\n";
}

void testMultipleOccurrences()
{
    AhoCorasick automaton;

    automaton.addPattern("test");

    automaton.build();

    auto matches = automaton.search("test test test");

    CHECK(matches.size() == 3);
    CHECK(hasMatch(matches, 0, 0, 3));
    CHECK(hasMatch(matches, 0, 5, 8));
    CHECK(hasMatch(matches, 0, 10, 13));

    std::cout << "testMultipleOccurrences passed\n";
}

// ---------------------------------------------------------------------------
// Overlaps and suffix chains
// ---------------------------------------------------------------------------

void testOverlappingMatches()
{
    AhoCorasick automaton;

    automaton.addPattern("aa");
    automaton.addPattern("aaa");

    automaton.build();

    auto matches = automaton.search("aaaa");

    // "aa" at 0-1, 1-2, 2-3 and "aaa" at 0-2, 1-3.
    CHECK(matches.size() == 5);
    CHECK(countForPattern(matches, 0) == 3);
    CHECK(countForPattern(matches, 1) == 2);

    std::cout << "testOverlappingMatches passed\n";
}

void testOverlappingSamePattern()
{
    AhoCorasick automaton;

    automaton.addPattern("aba");

    automaton.build();

    auto matches = automaton.search("ababa");

    CHECK(matches.size() == 2);
    CHECK(hasMatch(matches, 0, 0, 2));
    CHECK(hasMatch(matches, 0, 2, 4));

    std::cout << "testOverlappingSamePattern passed\n";
}

void testPrefixPatterns()
{
    AhoCorasick automaton;

    automaton.addPattern("a");
    automaton.addPattern("ab");
    automaton.addPattern("abc");

    automaton.build();

    auto matches = automaton.search("abc");

    CHECK(matches.size() == 3);
    CHECK(hasMatch(matches, 0, 0, 0));
    CHECK(hasMatch(matches, 1, 0, 1));
    CHECK(hasMatch(matches, 2, 0, 2));

    std::cout << "testPrefixPatterns passed\n";
}

void testLongSuffixChain()
{
    AhoCorasick automaton;

    automaton.addPattern("a");
    automaton.addPattern("aa");
    automaton.addPattern("aaa");
    automaton.addPattern("aaaa");

    automaton.build();

    auto matches = automaton.search("aaaa");

    // 4 + 3 + 2 + 1 = 10
    CHECK(matches.size() == 10);
    CHECK(countForPattern(matches, 0) == 4);
    CHECK(countForPattern(matches, 1) == 3);
    CHECK(countForPattern(matches, 2) == 2);
    CHECK(countForPattern(matches, 3) == 1);

    std::cout << "testLongSuffixChain passed\n";
}

void testSharedPrefixes()
{
    AhoCorasick automaton;

    automaton.addPattern("car");
    automaton.addPattern("cart");
    automaton.addPattern("carbon");

    automaton.build();

    auto matches = automaton.search("cart");

    CHECK(matches.size() == 2);
    CHECK(hasMatch(matches, 0, 0, 2));
    CHECK(hasMatch(matches, 1, 0, 3));
    CHECK(countForPattern(matches, 2) == 0);

    std::cout << "testSharedPrefixes passed\n";
}

// ---------------------------------------------------------------------------
// Edges of the text
// ---------------------------------------------------------------------------

void testPatternAtBeginning()
{
    AhoCorasick automaton;

    automaton.addPattern("hello");

    automaton.build();

    auto matches = automaton.search("hello world");

    CHECK(matches.size() == 1);
    CHECK(hasMatch(matches, 0, 0, 4));

    std::cout << "testPatternAtBeginning passed\n";
}

void testPatternAtEnd()
{
    AhoCorasick automaton;

    automaton.addPattern("world");

    automaton.build();

    auto matches = automaton.search("hello world");

    CHECK(matches.size() == 1);
    CHECK(hasMatch(matches, 0, 6, 10));

    std::cout << "testPatternAtEnd passed\n";
}

void testPatternEqualsText()
{
    AhoCorasick automaton;

    automaton.addPattern("hello");

    automaton.build();

    auto matches = automaton.search("hello");

    CHECK(matches.size() == 1);
    CHECK(hasMatch(matches, 0, 0, 4));

    std::cout << "testPatternEqualsText passed\n";
}

void testPatternLongerThanText()
{
    AhoCorasick automaton;

    automaton.addPattern("longpattern");

    automaton.build();

    CHECK(automaton.search("short").empty());

    std::cout << "testPatternLongerThanText passed\n";
}

void testEmptyText()
{
    AhoCorasick automaton;

    automaton.addPattern("hello");

    automaton.build();

    CHECK(automaton.search("").empty());

    std::cout << "testEmptyText passed\n";
}

void testOneCharacterPattern()
{
    AhoCorasick automaton;

    automaton.addPattern("a");

    automaton.build();

    auto matches = automaton.search("banana");

    CHECK(matches.size() == 3);
    CHECK(hasMatch(matches, 0, 1, 1));
    CHECK(hasMatch(matches, 0, 3, 3));
    CHECK(hasMatch(matches, 0, 5, 5));

    std::cout << "testOneCharacterPattern passed\n";
}

// ---------------------------------------------------------------------------
// Content
// ---------------------------------------------------------------------------

void testPunctuationAndNumbers()
{
    AhoCorasick automaton;

    automaton.addPattern("cmd.exe");
    automaton.addPattern("/etc/passwd");
    automaton.addPattern("404");
    automaton.addPattern("!@#");

    automaton.build();

    auto matches = automaton.search(
        "cmd.exe returned 404 while checking /etc/passwd !@#");

    CHECK(matches.size() == 4);
    CHECK(countForPattern(matches, 0) == 1);
    CHECK(countForPattern(matches, 1) == 1);
    CHECK(countForPattern(matches, 2) == 1);
    CHECK(countForPattern(matches, 3) == 1);

    std::cout << "testPunctuationAndNumbers passed\n";
}

void testCaseSensitivity()
{
    AhoCorasick automaton;

    automaton.addPattern("PowerShell");

    automaton.build();

    auto matches = automaton.search("powershell PowerShell POWERSHELL");

    // The automaton itself is case-sensitive; --ignore-case is handled by
    // the front end, which lowercases both patterns and text.
    CHECK(matches.size() == 1);
    CHECK(hasMatch(matches, 0, 11, 20));

    std::cout << "testCaseSensitivity passed\n";
}

void testNonAsciiBytes()
{
    AhoCorasick automaton;

    automaton.addPattern("caf\xC3\xA9");   // "cafe" with an accented e, UTF-8

    automaton.build();

    auto matches = automaton.search("un caf\xC3\xA9 noir");

    // Bytes >= 0x80 must be indexed as unsigned char, not negative values.
    CHECK(matches.size() == 1);
    CHECK(hasMatch(matches, 0, 3, 7));     // byte offsets, not characters

    std::cout << "testNonAsciiBytes passed\n";
}

void testManyDifferentPatterns()
{
    AhoCorasick automaton;

    automaton.addPattern("apple");
    automaton.addPattern("banana");
    automaton.addPattern("orange");
    automaton.addPattern("grape");
    automaton.addPattern("pear");
    automaton.addPattern("watermelon");
    automaton.addPattern("melon");
    automaton.addPattern("berry");

    automaton.build();

    auto matches = automaton.search(
        "apple banana orange grape pear watermelon melon berry");

    // "melon" occurs once inside "watermelon" and once on its own: 9 total.
    CHECK(matches.size() == 9);
    CHECK(countForPattern(matches, 6) == 2);

    std::cout << "testManyDifferentPatterns passed\n";
}

// ---------------------------------------------------------------------------
// Duplicate patterns
// ---------------------------------------------------------------------------

void testDuplicatePatterns()
{
    AhoCorasick automaton;

    automaton.addPattern("test");
    automaton.addPattern("test");

    automaton.build();

    auto matches = automaton.search("test");

    // Both entries are reported, each with its own pattern index.
    CHECK(matches.size() == 2);
    CHECK(hasMatch(matches, 0, 0, 3));
    CHECK(hasMatch(matches, 1, 0, 3));

    std::cout << "testDuplicatePatterns passed\n";
}

void testDuplicateWithSubstring()
{
    AhoCorasick automaton;

    automaton.addPattern("he");
    automaton.addPattern("he");
    automaton.addPattern("she");

    automaton.build();

    auto matches = automaton.search("she");

    // "she" once, plus "he" once for each duplicate index.
    CHECK(matches.size() == 3);
    CHECK(countForPattern(matches, 0) == 1);
    CHECK(countForPattern(matches, 1) == 1);
    CHECK(countForPattern(matches, 2) == 1);

    std::cout << "testDuplicateWithSubstring passed\n";
}

// ---------------------------------------------------------------------------
// Build state and accessors
// ---------------------------------------------------------------------------

void testSearchBeforeBuildThrows()
{
    AhoCorasick automaton;

    automaton.addPattern("abc");

    CHECK(throwsLogicError([&] { automaton.search("abc"); }));

    std::cout << "testSearchBeforeBuildThrows passed\n";
}

void testAddPatternAfterBuildThrows()
{
    AhoCorasick automaton;

    automaton.addPattern("abc");
    automaton.build();

    CHECK(throwsLogicError([&] { automaton.addPattern("def"); }));

    std::cout << "testAddPatternAfterBuildThrows passed\n";
}

void testBuildTwiceIsIdempotent()
{
    AhoCorasick automaton;

    automaton.addPattern("he");
    automaton.addPattern("she");

    automaton.build();
    automaton.build();

    // A second build must not append the failure outputs again.
    CHECK(automaton.search("she").size() == 2);

    std::cout << "testBuildTwiceIsIdempotent passed\n";
}

void testEmptyPatternThrows()
{
    AhoCorasick automaton;

    CHECK(throwsInvalidArgument([&] { automaton.addPattern(""); }));

    // The rejected pattern must not have been recorded, and the automaton
    // must still work normally afterwards.
    CHECK(automaton.getPatternCount() == 0);

    automaton.addPattern("ab");
    automaton.build();

    auto matches = automaton.search("abc");

    CHECK(matches.size() == 1);
    CHECK(hasMatch(matches, 0, 0, 1));

    std::cout << "testEmptyPatternThrows passed\n";
}

void testPatternAccessors()
{
    AhoCorasick automaton;

    automaton.addPattern("first");
    automaton.addPattern("second");

    CHECK(automaton.getPatternCount() == 2);
    CHECK(automaton.getPattern(0) == "first");
    CHECK(automaton.getPattern(1) == "second");

    std::cout << "testPatternAccessors passed\n";
}

int main()
{
    testClassicExample();
    testSubstringPatternsAreReported();
    testNoMatches();
    testSinglePattern();
    testMultipleOccurrences();

    testOverlappingMatches();
    testOverlappingSamePattern();
    testPrefixPatterns();
    testLongSuffixChain();
    testSharedPrefixes();

    testPatternAtBeginning();
    testPatternAtEnd();
    testPatternEqualsText();
    testPatternLongerThanText();
    testEmptyText();
    testOneCharacterPattern();

    testPunctuationAndNumbers();
    testCaseSensitivity();
    testNonAsciiBytes();
    testManyDifferentPatterns();

    testDuplicatePatterns();
    testDuplicateWithSubstring();

    testSearchBeforeBuildThrows();
    testAddPatternAfterBuildThrows();
    testBuildTwiceIsIdempotent();
    testEmptyPatternThrows();
    testPatternAccessors();

    std::cout << "\nAll tests passed.\n";

    return 0;
}