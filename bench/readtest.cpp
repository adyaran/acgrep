// Diagnostic: where does acgrep's time go on Windows?
//
// Times three ways of walking a file, so the read cost can be separated from the search cost:
//   1. getline only          - read lines with std::getline, do nothing else
//   2. getline + search      - what acgrep does now (minus printing)
//   3. block read + search   - fread 1 MB chunks, split lines by hand, search each line
//
// Build (Developer PowerShell, repo root):
//   cl /nologo /O2 /EHsc /std:c++20 /I acgrep /Fo:bench\ bench\readtest.cpp acgrep\aho_corasick.cpp /Fe:bench\readtest.exe
// Build (Linux):
//   g++ -O2 -std=c++20 -I acgrep bench/readtest.cpp acgrep/aho_corasick.cpp -o bench/readtest
// Run:
//   .\bench\readtest.exe .\bench\data\big.log .\bench\data\p1.txt

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "aho_corasick.h"

using Clock = std::chrono::steady_clock;

static double seconds(Clock::time_point start)
{
    return std::chrono::duration<double>(Clock::now() - start).count();
}

static void stripCr(std::string& line)
{
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
}

struct Result
{
    double best = 1e9;
    long long lines = 0;
    long long matches = 0;
};

template <typename Fn>
static Result bestOf(int runs, Fn fn)
{
    Result r;
    for (int i = 0; i < runs; ++i)
    {
        Result one;
        auto start = Clock::now();
        fn(one);
        one.best = seconds(start);
        if (one.best < r.best)
            r = one;
    }
    return r;
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "usage: readtest <log-file> <pattern-file>\n";
        return 2;
    }

    const std::string logPath = argv[1];

    AhoCorasick automaton;
    {
        std::ifstream patterns(argv[2]);
        std::string p;
        while (std::getline(patterns, p))
        {
            stripCr(p);
            if (!p.empty())
                automaton.addPattern(p);
        }
    }

    auto buildStart = Clock::now();
    automaton.build();
    std::cout << "build: " << seconds(buildStart) << " s ("
        << automaton.getPatternCount() << " patterns)\n";

    const int runs = 3;

    Result a = bestOf(runs, [&](Result& r)
        {
            std::ifstream in(logPath);
            std::string line;
            while (std::getline(in, line))
                ++r.lines;
        });

    Result b = bestOf(runs, [&](Result& r)
        {
            std::ifstream in(logPath);
            std::string line;
            while (std::getline(in, line))
            {
                ++r.lines;
                stripCr(line);
                r.matches += static_cast<long long>(automaton.search(line).size());
            }
        });

    Result c = bestOf(runs, [&](Result& r)
        {
            std::FILE* f = std::fopen(logPath.c_str(), "rb");
            if (!f)
                return;

            std::vector<char> buffer(1 << 20);
            std::string line;
            std::size_t got;
            while ((got = std::fread(buffer.data(), 1, buffer.size(), f)) > 0)
            {
                for (std::size_t i = 0; i < got; ++i)
                {
                    if (buffer[i] == '\n')
                    {
                        ++r.lines;
                        stripCr(line);
                        r.matches += static_cast<long long>(automaton.search(line).size());
                        line.clear();
                    }
                    else
                    {
                        line.push_back(buffer[i]);
                    }
                }
            }
            std::fclose(f);
        });

    std::cout << "best of " << runs << " runs (seconds):\n"
        << "  1. getline only        " << a.best << "  (" << a.lines << " lines)\n"
        << "  2. getline + search    " << b.best << "  (" << b.matches << " matches)\n"
        << "  3. block read + search " << c.best << "  (" << c.matches << " matches)\n";

    if (b.matches != c.matches || a.lines != b.lines || b.lines != c.lines)
        std::cout << "WARNING: the three methods disagree; do not trust the timings.\n";

    return 0;
}
