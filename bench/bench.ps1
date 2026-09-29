# Windows benchmark: acgrep vs grep -F (if installed) vs findstr /L.
# Works in Windows PowerShell 5.1 and PowerShell 7. Run from anywhere:
#
#   .\bench\bench.ps1
#   .\bench\bench.ps1 -Acgrep C:\path\to\acgrep.exe -SizeMB 50 -Runs 3
#
# First run generates bench\data\ (a 50 MB log and 5 pattern files); this takes a minute or two.
# Results are also written to bench\results.txt.
#
# Method notes:
#  * Each tool's output is redirected to a temp FILE, never to NUL. GNU grep notices when its output
#    is /dev/null (NUL) and stops at the first match, which would make it look instantly fast.
#  * Wall-clock time of the whole process, best of -Runs.
#  * Matching-line counts are compared with acgrep's to check the tools agree.

param(
    [string]$Acgrep = (Join-Path $PSScriptRoot '..\out\build\x64-release\acgrep\acgrep.exe'),
    [int]$SizeMB = 50,
    [int]$Runs = 3,
    [int]$TimeoutSec = 300
)

$ErrorActionPreference = 'Stop'
$dataDir = Join-Path $PSScriptRoot 'data'
$log = Join-Path $dataDir 'big.log'
$counts = 1, 10, 100, 1000, 10000

if (-not (Test-Path $Acgrep)) {
    throw "acgrep.exe not found at '$Acgrep'. Build it (cmake --preset x64-release; cmake --build --preset x64-release) or pass -Acgrep <path>."
}
$Acgrep = (Resolve-Path $Acgrep).Path

# ---------------------------------------------------------------- data generation
function New-BenchData {
    Write-Host "Generating benchmark data in $dataDir (about $SizeMB MB, be patient)..."
    New-Item -ItemType Directory -Force -Path $dataDir | Out-Null

    $rng = New-Object System.Random 42
    $utf8 = New-Object System.Text.UTF8Encoding $false

    $letters = 'abcdefghijklmnopqrstuvwxyz'
    $vocab = New-Object 'string[]' 5000
    for ($i = 0; $i -lt 5000; $i++) {
        $len = $rng.Next(3, 11)
        $chars = New-Object 'char[]' $len
        for ($j = 0; $j -lt $len; $j++) { $chars[$j] = $letters[$rng.Next(26)] }
        $vocab[$i] = -join $chars
    }

    $statuses = '200', '404', '500'
    $sw = New-Object System.IO.StreamWriter $log, $false, $utf8
    $sw.NewLine = "`n"
    $sb = New-Object System.Text.StringBuilder
    $written = 0
    $target = [long]$SizeMB * 1000000
    while ($written -lt $target) {
        [void]$sb.Clear()
        $words = $rng.Next(8, 17)
        for ($w = 0; $w -lt $words; $w++) {
            if ($w -gt 0) { [void]$sb.Append(' ') }
            [void]$sb.Append($vocab[$rng.Next(5000)])
        }
        [void]$sb.Append(' status=').Append($statuses[$rng.Next(3)])
        [void]$sb.Append(' id=').Append($rng.Next(0, 1000001))
        $sw.WriteLine($sb.ToString())
        $written += $sb.Length + 1
    }
    $sw.Close()

    # Pattern files: half are words that occur in the log (hits), half random junk (almost all misses).
    $junk = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
    foreach ($n in $counts) {
        # partial Fisher-Yates to pick distinct vocabulary words
        $pool = $vocab.Clone()
        $hitCount = if ($n -gt 1) { [int][math]::Floor($n / 2) } else { 1 }
        $lines = New-Object System.Collections.Generic.List[string]
        for ($i = 0; $i -lt $hitCount; $i++) {
            $k = $rng.Next($i, $pool.Length)
            $tmp = $pool[$i]; $pool[$i] = $pool[$k]; $pool[$k] = $tmp
            $lines.Add($pool[$i])
        }
        for ($i = $hitCount; $i -lt $n; $i++) {
            $len = $rng.Next(6, 13)
            $chars = New-Object 'char[]' $len
            for ($j = 0; $j -lt $len; $j++) { $chars[$j] = $junk[$rng.Next($junk.Length)] }
            $lines.Add((-join $chars))
        }
        [System.IO.File]::WriteAllText((Join-Path $dataDir "p$n.txt"), (($lines -join "`n") + "`n"), $utf8)
    }
}

if (-not (Test-Path $log)) { New-BenchData }

# ---------------------------------------------------------------- tools to compare
$tools = @()
$tools += [pscustomobject]@{
    Name = 'acgrep'; Exe = $Acgrep
    Args = { param($p, $f) "--pattern-file `"$p`" `"$f`"" }
}

$grepCmd = Get-Command grep.exe -ErrorAction SilentlyContinue
if (-not $grepCmd) {
    foreach ($c in 'C:\Program Files\Git\usr\bin\grep.exe', 'C:\Program Files (x86)\Git\usr\bin\grep.exe') {
        if (Test-Path $c) { $grepCmd = Get-Item $c; break }
    }
}
if ($grepCmd) {
    $grepPath = if ($grepCmd.PSObject.Properties['Source']) { $grepCmd.Source } else { $grepCmd.FullName }
    $tools += [pscustomobject]@{
        Name = 'grep -F'; Exe = $grepPath
        Args = { param($p, $f) "-F -f `"$p`" `"$f`"" }
    }
} else {
    Write-Host "grep.exe not found (installing Git for Windows provides one); skipping grep -F." -ForegroundColor Yellow
}

$tools += [pscustomobject]@{
    Name = 'findstr /L'; Exe = (Join-Path $env:SystemRoot 'System32\findstr.exe')
    Args = { param($p, $f) "/L /G:`"$p`" `"$f`"" }
}

# Byte-wise matching for GNU grep, so it does the same job as acgrep.
$env:LC_ALL = 'C'

# ---------------------------------------------------------------- measurement
$tmpOut = Join-Path ([System.IO.Path]::GetTempPath()) 'acgrep_bench_out.txt'
$tmpErr = Join-Path ([System.IO.Path]::GetTempPath()) 'acgrep_bench_err.txt'

function Measure-Tool($tool, $patternFile) {
    $argString = & $tool.Args $patternFile $log
    $best = [double]::PositiveInfinity
    $lines = -1
    for ($r = 0; $r -lt $Runs; $r++) {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $proc = Start-Process -FilePath $tool.Exe -ArgumentList $argString -NoNewWindow -PassThru `
            -RedirectStandardOutput $tmpOut -RedirectStandardError $tmpErr
        $null = $proc.Handle   # keeps the exit code readable after exit
        if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
            $proc.Kill()
            return [pscustomobject]@{ Seconds = $null; Lines = $null; Note = "timeout >${TimeoutSec}s" }
        }
        $sw.Stop()
        if ($proc.ExitCode -ge 2) {
            $msg = (Get-Content $tmpErr -TotalCount 1)
            return [pscustomobject]@{ Seconds = $null; Lines = $null; Note = "error (exit $($proc.ExitCode)) $msg" }
        }
        if ($sw.Elapsed.TotalSeconds -lt $best) { $best = $sw.Elapsed.TotalSeconds }
    }
    $count = 0
    foreach ($l in [System.IO.File]::ReadLines($tmpOut)) { $count++ }
    return [pscustomobject]@{ Seconds = $best; Lines = $count; Note = '' }
}

# ---------------------------------------------------------------- run
$os = (Get-CimInstance Win32_OperatingSystem).Caption
$cpu = (Get-CimInstance Win32_Processor | Select-Object -First 1).Name
$logInfo = Get-Item $log
$header = @(
    "OS: $os",
    "CPU: $cpu ($([Environment]::ProcessorCount) logical cores)",
    "PowerShell: $($PSVersionTable.PSVersion)",
    "acgrep: $Acgrep",
    "grep: $(if ($grepCmd) { $grepPath } else { 'not found' })",
    "Input: $($logInfo.Length) bytes, best of $Runs runs"
)
$header | ForEach-Object { Write-Host $_ }

$rows = @()
foreach ($n in $counts) {
    $pf = Join-Path $dataDir "p$n.txt"
    $reference = $null
    foreach ($tool in $tools) {
        Write-Host ("  patterns={0,-6} {1} ..." -f $n, $tool.Name)
        $res = Measure-Tool $tool $pf
        if ($tool.Name -eq 'acgrep') { $reference = $res.Lines }
        $agree = if ($null -eq $res.Lines) { '-' } elseif ($res.Lines -eq $reference) { 'yes' } else { "NO ($($res.Lines) vs $reference)" }
        $rows += [pscustomobject]@{
            Patterns = $n
            Tool     = $tool.Name
            Seconds  = if ($null -ne $res.Seconds) { '{0:N2}' -f $res.Seconds } else { 'n/a' }
            Lines    = if ($null -ne $res.Lines) { $res.Lines } else { '-' }
            Agrees   = $agree
            Note     = $res.Note
        }
    }
}

$table = $rows | Format-Table -AutoSize | Out-String -Width 200
Write-Host $table
($header + '' + $table) | Set-Content -Path (Join-Path $PSScriptRoot 'results.txt') -Encoding UTF8
Write-Host "Saved to $(Join-Path $PSScriptRoot 'results.txt')"

Remove-Item $tmpOut, $tmpErr -ErrorAction SilentlyContinue
