# acgrep

A small grep-style command-line tool that searches text for many patterns at once using the Aho–Corasick algorithm. All patterns are compiled into one automaton, then each input line is scanned in a single pass, with matching time proportional to the input length plus the matches reported.

It was written for UTS Advanced Algorithms 41052, Programming Assignment 1 (Track B: implementation plus a working tool). The written report is [`finalreport.pdf`](finalreport.pdf).

## Files

```text
acgrep/
├── CMakeLists.txt
├── CMakePresets.json
├── finalreport.pdf
├── acgrep/
│   ├── CMakeLists.txt
│   ├── acgrep.cpp
│   ├── aho_corasick.cpp
│   └── aho_corasick.h
├── bench/
│   ├── README.md
│   ├── bench.ps1
│   ├── readtest.cpp
│   ├── readtest.exe
│   ├── results.txt
│   └── data/
├── examples/
│   ├── access.log
│   └── patterns.txt
└── tests/
    └── test_aho_corasick.cpp
```

| File                                         | Purpose                                       |
| -------------------------------------------- | --------------------------------------------- |
| `acgrep/aho_corasick.h` / `aho_corasick.cpp` | Aho–Corasick automaton                        |
| `acgrep/acgrep.cpp`                          | Command-line interface                        |
| `bench/`                                     | Benchmark script, diagnostic and results (see [Benchmarks](#benchmarks)); `bench/data/` holds generated benchmark inputs |
| `examples/`                                  | Sample input files used in the examples below |
| `finalreport.pdf`                            | Written report for the assignment             |
| `tests/test_aho_corasick.cpp`                | Automaton unit tests                          |

## Requirements

* CMake 3.21+ (3.20 works if you skip presets)
* A C++20 compiler
* Git, if building from the repository

These are only needed to build from source; a [release download](#using-a-release-download) needs none of them.

## Building

| Platform                                     | Configure presets                                      |
| -------------------------------------------- | ------------------------------------------------------ |
| Windows (Visual Studio Developer PowerShell) | `x64-debug`, `x64-release`, `x86-debug`, `x86-release` |
| Linux / macOS                                | `debug`, `release`                                     |

```text
cmake --preset <preset>
cmake --build --preset <preset>
ctest --preset <preset>
```

For example, `cmake --preset debug` on Linux or `cmake --preset x64-debug` on Windows.

Everything is built into `out/build/<preset>/`:

```text
out/build/<preset>/acgrep/acgrep        (acgrep.exe on Windows)
out/build/<preset>/acgrep_tests         (acgrep_tests.exe on Windows)
```

Without presets, any platform can use plain CMake:

```text
cmake -S . -B build
cmake --build build
```

## Using a release download

Each release has one zip per platform:

* `windows-x64`: Windows, x86-64
* `linux-x64`: Linux, x86-64
* `macos-arm64`: macOS on Apple Silicon (M-series)

There is no Intel macOS build; on an Intel Mac, build from source as described above. Each zip contains the `acgrep` binary (`acgrep.exe` on Windows) and this README. It is a command-line tool, so run it from a terminal. The examples below use `acgrep`, so from the unzipped folder call the binary by its path instead:

| Platform      | Command                                              |
| ------------- | ---------------------------------------------------- |
| Windows       | `.\acgrep.exe --pattern-text "404" access.log`       |
| Linux / macOS | `./acgrep --pattern-text "404" access.log`           |

The `examples/` files are not in the zip. To try it, create a small log file in the same folder.

Linux / macOS:

```text
printf 'PowerShell returned 404\ncmd.exe returned error\n' > access.log
./acgrep --pattern-text "404" --pattern-text "cmd.exe" --count access.log
```

Windows (PowerShell):

```text
"PowerShell returned 404", "cmd.exe returned error" | Set-Content -Encoding ascii access.log
.\acgrep.exe --pattern-text "404" --pattern-text "cmd.exe" --count access.log
```

Both print:

```text
access.log: 404: 1
access.log: cmd.exe: 1
```

If you get a permission error on Linux or macOS, run `chmod +x acgrep`. Running with no arguments prints the usage message and exits with status 2.

## Usage

```text
acgrep [options] --pattern-text <pattern>... <input-file>...
acgrep [options] --pattern-file <file>... <input-file>...
acgrep [options] --pattern-text <pattern>... --pattern-file <file>... <input-file>...
```

### Patterns

| Option                     | Description                                                                      |
| -------------------------- | -------------------------------------------------------------------------------- |
| `--pattern-text <pattern>` | Search for a pattern given directly. Must not be empty. May be repeated.         |
| `--pattern-file <file>`    | Read one pattern per line from a file. Blank lines are skipped. May be repeated. |

Pattern sources can be combined.

### Search options

| Option           | Description                                |
| ---------------- | ------------------------------------------ |
| `--ignore-case`  | Case-insensitive matching (ASCII only)     |
| `--show-matches` | Show each match and its position           |
| `--count`        | Count occurrences of each pattern per file |

`--count` and `--show-matches` cannot be used together.

### Other options

| Option   | Description                              |
| -------- | ---------------------------------------- |
| `--help` | Show the help message                    |
| `--`     | Treat remaining arguments as input files |
| `-`      | Read from standard input                 |

Options can appear in any order.

### Exit status

| Code | Meaning                                  |
| ---- | ---------------------------------------- |
| `0`  | A match was found, or `--help` was shown |
| `1`  | No matches were found                    |
| `2`  | An error occurred                        |

## Behaviour notes

* Each line is searched separately, so a match never spans two lines. In plain mode a matching line is printed once, however many patterns hit it.
* `--count` counts occurrences, including overlapping matches and patterns that are substrings of other patterns.
* Every pattern is tracked independently. Duplicate patterns each get their own count and their own line in `--show-matches` output; nothing is deduplicated. With `--ignore-case`, `A` and `a` are two separate patterns.
* `--show-matches` and `--count` print each pattern as it was supplied, even with `--ignore-case`.
* An empty `--pattern-text` is an error. Blank lines in a pattern file are skipped.
* A trailing `\r` is removed from pattern-file lines and input lines, so files with Windows line endings behave the same as files with Unix line endings.
* If one input file cannot be opened, the remaining files are still searched and the exit code is `2`.

## Examples

The examples use the files in `examples/`. Forward slashes work on every platform, including Windows.

`examples/access.log`:

```text
PowerShell returned 404
powershell returned 404
POWERSHELL returned 200
cmd.exe returned error
CMD.EXE returned error
GET /index.html 404
```

`examples/patterns.txt`:

```text
PowerShell
cmd.exe
404
```

### Search

```text
acgrep --pattern-text "404" examples/access.log
```

```text
examples/access.log:1: PowerShell returned 404
examples/access.log:2: powershell returned 404
examples/access.log:6: GET /index.html 404
```

### Case-insensitive search

```text
acgrep --ignore-case --pattern-text "powershell" examples/access.log
```

```text
examples/access.log:1: PowerShell returned 404
examples/access.log:2: powershell returned 404
examples/access.log:3: POWERSHELL returned 200
```

### Show matches

```text
acgrep --show-matches --pattern-text "PowerShell" --pattern-text "404" examples/access.log
```

```text
examples/access.log:1: [PowerShell @ 0-9] PowerShell returned 404
examples/access.log:1: [404 @ 20-22] PowerShell returned 404
examples/access.log:2: [404 @ 20-22] powershell returned 404
examples/access.log:6: [404 @ 16-18] GET /index.html 404
```

Positions are 0-based byte offsets within the line, with an inclusive end.

### Patterns from a file, with counts

```text
acgrep --pattern-file examples/patterns.txt --count examples/access.log
```

```text
examples/access.log: PowerShell: 1
examples/access.log: cmd.exe: 1
examples/access.log: 404: 3
```

### Standard input

```text
cat examples/access.log | acgrep --pattern-text "404" -
```

```text
(standard input):1: PowerShell returned 404
(standard input):2: powershell returned 404
(standard input):6: GET /index.html 404
```

## Design

The automaton is a trie over bytes with a full 256-entry transition table per node.

1. **Insert.** `addPattern` adds a pattern to the trie and records its index in the output list of the node where it ends. Duplicate patterns simply add a second index to the same node.
2. **Build.** `build` processes the trie nodes breadth-first. For each node it sets the failure link, appends the failure node's outputs to its own, and fills missing transitions from the failure node. Afterwards the automaton is a complete state machine, so searching never has to follow a failure link.
3. **Search.** `search` walks the text once, moving one state per byte and emitting a match for every pattern index in the current node's output list.

Building takes O(N × 256) time for N trie nodes, where N ≤ P + 1 and P is the total number of pattern bytes, so it is O(P × 256), or O(P) if 256 is treated as a constant. Every trie node has 256 transitions to fill, and the 256 comes from this dense-table representation, not from Aho–Corasick itself. The constant matters in practice. Searching takes O(T + M) time, where T is the text length and M is the number of matches reported. The automaton uses O(N × 256) space for N trie nodes, plus the stored output indices.

The class checks how it is used: `search` before `build` and `addPattern` after `build` throw `std::logic_error`, and an empty pattern throws `std::invalid_argument`. Calling `build` twice is harmless.

## Testing

There are 27 unit tests for the automaton in `tests/test_aho_corasick.cpp`. Run them with `ctest --preset <preset>`, or run `acgrep_tests` directly.

The tests use a `CHECK` macro instead of `assert`, so they still run in Release builds, where `NDEBUG` removes every `assert`.

They cover the classic Aho–Corasick example, overlapping matches, suffix and prefix relationships, shared prefixes, text edges, punctuation, case sensitivity, non-ASCII bytes, duplicate patterns, empty patterns, and build-state errors.

Command-line behaviour was tested manually, including exit codes, pattern files, duplicate patterns, multiple input files, standard input, option handling, conflicting options, missing files and CRLF input. There are no automated tests of the command-line tool itself; the report lists adding them as future work.

## Benchmarks

The `bench/` folder compares acgrep with `grep -F` and `findstr /L` on a generated 50 MB log, using pattern sets of 1 to 10,000 patterns. See [`bench/README.md`](bench/README.md) for how to run it and [`finalreport.pdf`](finalreport.pdf) for the full analysis.

Summary of what the report found:

* acgrep found the same matching lines as `grep -F` wherever the two were compared (line counts only on Windows, line numbers and text on Linux).
* On Windows, acgrep was 1.3 to 4.2 times slower than `grep -F`. It is not competitive on speed.
* At 10,000 patterns, tests on Linux point to the number of matches, rather than the size of the automaton, as the main cost. This was not repeated on Windows.
* The Windows measurements were run by the project author. The Linux benchmark was run by Claude, and its scripts and results are not in this repository.

## Report and AI use

The report, [`finalreport.pdf`](finalreport.pdf), describes the implementation, the tool, the benchmarks, what was learned and how AI tools were used. Claude was used extensively (repository and release workflow, code, tests, benchmark scripts, and drafting and revising the report), and ChatGPT was tried for comparison. Section 4 of the report covers where the AI was wrong and what was and was not verified.

## Limitations

* `--ignore-case` performs ASCII-only case folding.
* Matching is byte-based, so positions for multi-byte encodings are byte offsets.
* Patterns are literal substrings; regular expressions and word boundaries are not supported.
* The dense transition table (about 1 KB per trie node) uses far more memory than a sparse representation for very large pattern sets.
* Match positions within a line are `int`, so a single line longer than about 2 GB is not supported.
* Input is read and searched one line at a time, so a match never crosses a line. A binary signature containing the byte `0x0A` would never be found, and a pattern file cannot contain a newline byte.
* Plain mode still collects every match on a line even though one is enough to print it, which slows large pattern sets that match almost every line.
* There are no automated tests of the command-line tool.

## Possible future work

* An early-exit path in plain mode.
* Block-based reading instead of line-by-line input.
* Automated tests of the command-line tool, run through CTest.
* A raw-byte mode, with patterns given as hex, for binary signatures.