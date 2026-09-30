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

## Linux

The Linux benchmark run described in the report was done by Claude in its own environment. Its scripts
and results are not part of this repository. `readtest.cpp` also builds on Linux (see its header comment).

## Method and pitfalls

- The data is a reproducible random-word log where half of each pattern set occurs in the log and half
  almost never does. `bench.ps1` uses a fixed seed. The Linux run used a different generator, so its data
  set is not identical and its numbers should not be compared directly with the Windows ones.
- Best-of-N wall-clock time, and a check that every tool reports the same number of matching lines.
- Never send grep's output to `/dev/null` (or `NUL`) when timing it. GNU grep then stops at the first
  match and looks instantly fast. `bench.ps1` writes output to a file instead.
- Synthetic data, one machine, wall-clock time only. GNU grep switches internal algorithms depending on
  the pattern set, so treat ratios as indicative.