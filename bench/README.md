# bench

Compares acgrep with existing fixed-string search tools.

## Windows (PowerShell): `bench.ps1`

Compares acgrep with `grep -F` (if installed) and the built-in `findstr /L`.

1. Build the release binary from a Visual Studio Developer PowerShell in the repo root:

   ```text
   cmake --preset x64-release
   cmake --build --preset x64-release
   ```

2. Get a GNU grep (optional but recommended). Git for Windows ships one at
   `C:\Program Files\Git\usr\bin\grep.exe`; the script finds it automatically. Without it, only acgrep
   and `findstr` are compared.
3. Run:

   ```text
   .\bench\bench.ps1
   ```

   If the script is blocked by the execution policy, use
   `powershell -ExecutionPolicy Bypass -File .\bench\bench.ps1`.
   If acgrep is not at `out\build\x64-release\acgrep\acgrep.exe`, pass `-Acgrep <path>`.

The first run generates `bench\data\` (a 50 MB log plus pattern files of 1 to 10,000 patterns) and
takes a minute or two. Results print to the console and are saved to `bench\results.txt`.
Options: `-SizeMB 50`, `-Runs 3`, `-TimeoutSec 300`.

## Linux: `gen.py` + `run.sh`

Needs `python3`, `bc` and GNU grep.

```text
cmake --preset release && cmake --build --preset release
./bench/run.sh                    # generates bench/data/ on first run
./bench/run.sh /path/to/acgrep    # or point at another binary
```

## Method and pitfalls

- The data is a reproducible random-word log where half of each pattern set occurs in the log and half
  almost never does. Both scripts use a fixed seed, but the Windows and Linux generators are different
  programs, so the two data sets are not identical and their numbers should not be compared directly.
- Best-of-N wall-clock time, and a check that every tool reports the same number of matching lines.
- Never send grep's output to `/dev/null` (or `NUL`) when timing it. GNU grep then stops at the first
  match and looks instantly fast. Both scripts write output to a file or pipe it instead.
- Synthetic data, one machine, wall-clock time only. GNU grep switches internal algorithms depending on
  the pattern set, so treat ratios as indicative.
