#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "aho_corasick.h"

// Exit codes follow grep's convention.
static constexpr int kExitSuccess = 0;
static constexpr int kExitNoMatch = 1;
static constexpr int kExitError = 2;

struct Options
{
    bool ignoreCase = false;
    bool showMatches = false;
    bool countMatches = false;

    std::vector<std::string> patternTexts;
    std::vector<std::string> patternFiles;
    std::vector<std::string> inputFiles;
};

enum class ParseResult
{
    Ok,
    Help,
    Error
};

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

// ASCII-only case folding. Non-ASCII bytes (e.g. UTF-8) are left untouched.
static std::string toLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return text;
}

// getline leaves the '\r' of Windows "\r\n" line endings in place.
static void stripCarriageReturn(std::string& line)
{
    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }
}

static void printUsage()
{
    std::cout
        << "Usage:\n"
        << "  acgrep [options] --pattern-text <pattern>... <input-file>...\n"
        << "  acgrep [options] --pattern-file <file>... <input-file>...\n"
        << "  acgrep [options] --pattern-text <pattern>... --pattern-file <file>... <input-file>...\n\n"

        << "Patterns:\n"
        << "  --pattern-text <pattern>  Search for a pattern given directly (must not be empty)\n"
        << "  --pattern-file <file>     Read one pattern per line from a file\n"
        << "                            (blank lines are skipped, trailing \\r is removed)\n"
        << "  Every pattern is tracked independently, even if it appears more than once.\n\n"

        << "Search options:\n"
        << "  --ignore-case             Match without regard to letter case (ASCII only)\n"
        << "  --show-matches            Show matched patterns and positions\n"
        << "  --count                   Show the number of occurrences of each pattern\n"
        << "                            (occurrences, not matching lines)\n\n"

        << "Other options:\n"
        << "  --help                    Show this help message\n"
        << "  --                        Treat all remaining arguments as input files\n"
        << "  -                         Read from standard input (as an input file)\n\n"

        << "Exit status:\n"
        << "  0  success (a match was found, or --help was shown)\n"
        << "  1  no matches were found\n"
        << "  2  an error occurred\n\n"

        << "Examples:\n"
        << "  acgrep --pattern-text \"PowerShell\" access.log\n"
        << "  acgrep --pattern-text \"PowerShell\" --pattern-text \"cmd.exe\" access.log\n"
        << "  acgrep --pattern-file patterns.txt access.log\n"
        << "  acgrep --pattern-text \"404\" --pattern-file patterns.txt access.log\n"
        << "  acgrep --ignore-case --pattern-text \"powershell\" access.log\n"
        << "  acgrep --pattern-file patterns.txt --count access.log\n"
        << "  acgrep --pattern-file patterns.txt --show-matches access.log\n"
        << "  cat access.log | acgrep --pattern-text \"404\" -\n";
}

// ---------------------------------------------------------------------------
// Argument parsing
// ---------------------------------------------------------------------------

// Parses everything up front so option order does not affect behaviour.
static ParseResult parseArguments(int argc, char* argv[], Options& options)
{
    bool endOfOptions = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];

        if (endOfOptions)
        {
            options.inputFiles.push_back(argument);
        }
        else if (argument == "--")
        {
            endOfOptions = true;
        }
        else if (argument == "--pattern-text" || argument == "--pattern-file")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: " << argument << " requires a value.\n";
                return ParseResult::Error;
            }

            auto& target = (argument == "--pattern-text")
                ? options.patternTexts
                : options.patternFiles;

            target.push_back(argv[++i]);
        }
        else if (argument == "--ignore-case")
        {
            options.ignoreCase = true;
        }
        else if (argument == "--show-matches")
        {
            options.showMatches = true;
        }
        else if (argument == "--count")
        {
            options.countMatches = true;
        }
        else if (argument == "--help")
        {
            printUsage();
            return ParseResult::Help;
        }
        else if (argument.rfind("--", 0) == 0)
        {
            std::cerr << "Error: unknown option: " << argument << '\n';
            return ParseResult::Error;
        }
        else
        {
            options.inputFiles.push_back(argument);
        }
    }

    if (options.showMatches && options.countMatches)
    {
        std::cerr << "Error: --count and --show-matches cannot be used together.\n";
        return ParseResult::Error;
    }

    if (options.patternTexts.empty() && options.patternFiles.empty())
    {
        std::cerr << "Error: at least one pattern must be supplied "
            << "using --pattern-text or --pattern-file.\n\n";
        printUsage();
        return ParseResult::Error;
    }

    if (options.inputFiles.empty())
    {
        std::cerr << "Error: at least one input file is required.\n\n";
        printUsage();
        return ParseResult::Error;
    }

    return ParseResult::Ok;
}

// ---------------------------------------------------------------------------
// Automaton construction
// ---------------------------------------------------------------------------

// `originalPatterns[i]` is the pattern as the user typed it, for the automaton's
// pattern index i. The automaton itself may only ever see the lowercased form.
static bool addPattern(
    AhoCorasick& automaton,
    std::vector<std::string>& originalPatterns,
    const std::string& pattern,
    bool ignoreCase)
{
    if (pattern.empty())
    {
        std::cerr << "Error: empty patterns are not supported.\n";
        return false;
    }

    automaton.addPattern(ignoreCase ? toLower(pattern) : pattern);
    originalPatterns.push_back(pattern);

    return true;
}

static bool addPatternsFromFile(
    AhoCorasick& automaton,
    std::vector<std::string>& originalPatterns,
    const std::string& filename,
    bool ignoreCase)
{
    std::ifstream stream(filename);

    if (!stream)
    {
        std::cerr << "Error: could not open pattern file: " << filename << '\n';
        return false;
    }

    std::string pattern;

    while (std::getline(stream, pattern))
    {
        stripCarriageReturn(pattern);

        // Blank lines in a pattern file are ignored (unlike an empty
        // --pattern-text, which is an error).
        if (pattern.empty())
        {
            continue;
        }

        if (!addPattern(automaton, originalPatterns, pattern, ignoreCase))
        {
            return false;
        }
    }

    return true;
}

static bool buildAutomaton(
    const Options& options,
    AhoCorasick& automaton,
    std::vector<std::string>& originalPatterns)
{
    for (const std::string& pattern : options.patternTexts)
    {
        if (!addPattern(automaton, originalPatterns, pattern, options.ignoreCase))
        {
            return false;
        }
    }

    for (const std::string& filename : options.patternFiles)
    {
        if (!addPatternsFromFile(
            automaton, originalPatterns, filename, options.ignoreCase))
        {
            return false;
        }
    }

    if (originalPatterns.empty())
    {
        std::cerr << "Error: no patterns were loaded.\n";
        return false;
    }

    automaton.build();

    return true;
}

// ---------------------------------------------------------------------------
// Output modes
// ---------------------------------------------------------------------------

static void printLine(
    const std::string& source, std::size_t lineNumber, const std::string& line)
{
    std::cout << source << ':' << lineNumber << ": " << line << '\n';
}

static void printMatchDetails(
    const std::string& source,
    std::size_t lineNumber,
    const std::string& line,
    const std::vector<Match>& matches,
    const std::vector<std::string>& patterns)
{
    for (const auto& match : matches)
    {
        std::cout << source << ':' << lineNumber << ": ["
            << patterns[match.patternIndex]
            << " @ " << match.start << '-' << match.end << "] "
            << line << '\n';
    }
}

static void printCounts(
    const std::string& source,
    const std::vector<std::string>& patterns,
    const std::vector<std::size_t>& counts)
{
    for (std::size_t i = 0; i < patterns.size(); ++i)
    {
        std::cout << source << ": " << patterns[i] << ": " << counts[i] << '\n';
    }
}

// ---------------------------------------------------------------------------
// Searching
// ---------------------------------------------------------------------------

// Returns true if at least one line matched.
static bool processStream(
    std::istream& input,
    const std::string& source,
    const Options& options,
    const AhoCorasick& automaton,
    const std::vector<std::string>& patterns)
{
    std::vector<std::size_t> counts(patterns.size(), 0);

    bool anyMatch = false;
    std::size_t lineNumber = 0;
    std::string line;

    while (std::getline(input, line))
    {
        ++lineNumber;
        stripCarriageReturn(line);

        const auto matches = automaton.search(
            options.ignoreCase ? toLower(line) : line);

        if (matches.empty())
        {
            continue;
        }

        anyMatch = true;

        if (options.countMatches)
        {
            for (const auto& match : matches)
            {
                ++counts[match.patternIndex];
            }
        }
        else if (options.showMatches)
        {
            printMatchDetails(source, lineNumber, line, matches, patterns);
        }
        else
        {
            printLine(source, lineNumber, line);
        }
    }

    if (options.countMatches)
    {
        printCounts(source, patterns, counts);
    }

    return anyMatch;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    Options options;

    switch (parseArguments(argc, argv, options))
    {
    case ParseResult::Help:  return kExitSuccess;
    case ParseResult::Error: return kExitError;
    case ParseResult::Ok:    break;
    }

    AhoCorasick automaton;
    std::vector<std::string> patterns;

    if (!buildAutomaton(options, automaton, patterns))
    {
        return kExitError;
    }

    bool anyMatch = false;
    bool hadError = false;

    for (const std::string& inputFile : options.inputFiles)
    {
        if (inputFile == "-")
        {
            anyMatch |= processStream(
                std::cin, "(standard input)", options, automaton, patterns);
            continue;
        }

        std::ifstream stream(inputFile);

        if (!stream)
        {
            std::cerr << "Error: could not open input file: " << inputFile << '\n';
            hadError = true;
            continue;
        }

        anyMatch |= processStream(stream, inputFile, options, automaton, patterns);
    }

    if (hadError)
    {
        return kExitError;
    }

    return anyMatch ? kExitSuccess : kExitNoMatch;
}