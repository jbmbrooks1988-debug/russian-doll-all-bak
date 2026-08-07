# button.ps1 - Windows launcher for $.crypts (parity with button.sh)
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\button.ps1 <action>
# Actions: run|r|start|restart | on | off | status | compile|c|build | check | help
#
# ASCII only (no smart quotes / em-dashes).
# Does not hang on Get-Process.Path — kill uses ProcessName + taskkill.

param(
    [Parameter(Position = 0)]
    [string]$Action = "help"
)

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $SCRIPT_DIR

$MSYS_BIN = "C:\msys64\mingw64\bin"
if (Test-Path $MSYS_BIN) {
    if ($env:Path -notlike "*$MSYS_BIN*") {
        $env:Path = "$MSYS_BIN;$env:Path"
    }
}

$PDL = Join-Path $SCRIPT_DIR "autostart.pdl"
$BIN_DIR = Join-Path $SCRIPT_DIR "ops\+x"
$BIN = Join-Path $BIN_DIR "crypt_autostart.+x"
$BIN_EXE = Join-Path $BIN_DIR "crypt_autostart.+x.exe"
$SRC = Join-Path $SCRIPT_DIR "ops\crypt_autostart.c"
$HOUSE = Split-Path -Parent $SCRIPT_DIR

function Get-CryptBin {
    $plain = Join-Path $BIN_DIR "crypt_autostart.exe"
    if (Test-Path -LiteralPath $plain) { return $plain }
    if (Test-Path -LiteralPath $BIN_EXE) { return $BIN_EXE }
    if (Test-Path -LiteralPath $BIN) { return $BIN }
    return $null
}

function Invoke-CompileCrypt {
    if (-not (Test-Path -LiteralPath $BIN_DIR)) {
        New-Item -ItemType Directory -Path $BIN_DIR -Force | Out-Null
    }
    # Windows: plain .exe (ld rejects ".+x" under emoji house paths)
    $out = "crypt_autostart.exe"
    Write-Host "gcc crypt_autostart.c -> ops/+x/$out"
    Push-Location -LiteralPath $BIN_DIR
    try {
        & gcc -Wall -O2 -o $out "..\crypt_autostart.c"
        if ($LASTEXITCODE -ne 0) {
            Write-Host "FAIL crypt_autostart"
            return $LASTEXITCODE
        }
    } finally {
        Pop-Location
    }
    Write-Host "OK crypt_autostart"
    return 0
}

function Invoke-CompileKhtpm {
    # Minimal Win32 stubs — output plain .exe (crypt_autostart resolves .exe)
    $tpOps = Join-Path $HOUSE "&.widgits\tile-picker\ops"
    $tpOutDir = Join-Path $tpOps "+x"
    $tbOps = Join-Path $HOUSE "&.widgits\livedesk-taskbar\ops"
    $tbOutDir = Join-Path $tbOps "+x"

    foreach ($d in @($tpOutDir, $tbOutDir)) {
        if (-not (Test-Path -LiteralPath $d)) {
            New-Item -ItemType Directory -Path $d -Force | Out-Null
        }
    }

    $rc = 0
    # Shared core + Win plat (WIN-COMPAT-RULE / KHTPM-ARCH.txt)
    $core = Join-Path $tpOps "khtpm_core.c"
    $main = Join-Path $tpOps "khtpm_main.c"
    $plat = Join-Path $tpOps "khtpm_plat_win.c"
    if ((Test-Path -LiteralPath $core) -and (Test-Path -LiteralPath $main) -and (Test-Path -LiteralPath $plat)) {
        Write-Host "gcc khtpm_main+core+plat_win -> tp_desktop_window.exe"
        Push-Location -LiteralPath $tpOutDir
        try {
            & gcc -Wall -O2 -o "tp_desktop_window.exe" `
                "..\khtpm_main.c" "..\khtpm_core.c" "..\khtpm_plat_win.c" `
                -lopengl32 -lgdi32 -luser32
            if ($LASTEXITCODE -ne 0) { $rc = 1; Write-Host "FAIL tp_desktop_window (shared)" }
            else { Write-Host "OK tp_desktop_window (shared core)" }
        } finally { Pop-Location }
    } elseif (Test-Path -LiteralPath (Join-Path $tpOps "tp_desktop_window_win.c")) {
        Write-Host "FALLBACK gcc tp_desktop_window_win.c"
        Push-Location -LiteralPath $tpOutDir
        try {
            & gcc -Wall -O2 -o "tp_desktop_window.exe" "..\tp_desktop_window_win.c" -lopengl32 -lgdi32 -luser32
            if ($LASTEXITCODE -ne 0) { $rc = 1; Write-Host "FAIL tp_desktop_window" }
            else { Write-Host "OK tp_desktop_window (legacy win)" }
        } finally { Pop-Location }
    } else {
        Write-Host "MISS khtpm sources"
        $rc = 1
    }
    # Shared taskbar core + Win plat (NO separate toolbar logic)
    $tbCore = Join-Path $tpOps "khtpm_taskbar_core.c"
    $tbMain = Join-Path $tpOps "khtpm_taskbar_main.c"
    $tbPlat = Join-Path $tpOps "khtpm_taskbar_plat_win.c"
    if ((Test-Path -LiteralPath $tbCore) -and (Test-Path -LiteralPath $tbMain) -and (Test-Path -LiteralPath $tbPlat)) {
        Write-Host "gcc khtpm_taskbar main+core+plat_win -> tp_taskbar.exe"
        # build into tile-picker +x then copy to livedesk-taskbar +x
        Push-Location -LiteralPath $tpOutDir
        try {
            & gcc -Wall -O2 -o "tp_taskbar.exe" `
                "..\khtpm_taskbar_main.c" "..\khtpm_taskbar_core.c" "..\khtpm_taskbar_plat_win.c" `
                -lgdi32 -luser32
            if ($LASTEXITCODE -ne 0) { $rc = 1; Write-Host "FAIL tp_taskbar (shared)" }
            else {
                Write-Host "OK tp_taskbar (shared core)"
                if (-not (Test-Path -LiteralPath $tbOutDir)) {
                    New-Item -ItemType Directory -Path $tbOutDir -Force | Out-Null
                }
                Copy-Item -LiteralPath "tp_taskbar.exe" -Destination (Join-Path $tbOutDir "tp_taskbar.exe") -Force
            }
        } finally { Pop-Location }
    } else {
        Write-Host "MISS khtpm_taskbar shared sources"
        $rc = 1
    }
    return $rc
}

function Invoke-KillLivedesk {
    Write-Host "Stopping KHTPM processes (name-only)..."
    $names = @("tp_desktop_window", "tp_taskbar", "crypt_autostart",
               "tp_desktop_window.+x", "tp_taskbar.+x", "crypt_autostart.+x")
    foreach ($n in $names) {
        Get-Process -ErrorAction SilentlyContinue |
            Where-Object { $_.ProcessName -eq $n -or $_.ProcessName -like "$n*" } |
            ForEach-Object {
                try { Stop-Process -Id $_.Id -Force -ErrorAction Stop } catch { }
            }
        & taskkill /F /IM "$n.exe" 2>$null | Out-Null
        & taskkill /F /IM $n 2>$null | Out-Null
    }
    Write-Host "done"
}

function Set-AutostartEnabled([int]$On) {
    if (-not (Test-Path -LiteralPath $PDL)) {
        Write-Host "MISSING $PDL"
        return 1
    }
    $lines = Get-Content -LiteralPath $PDL
    $out = foreach ($line in $lines) {
        if ($line -match '^\s*STATE\s*\|\s*enabled\s*\|') {
            if ($On -eq 1) {
                ($line -replace '\|\s*[01]\s*$', '| 1')
            } else {
                ($line -replace '\|\s*[01]\s*$', '| 0')
            }
        } else {
            $line
        }
    }
    $out | Set-Content -LiteralPath $PDL -Encoding utf8
    if ($On -eq 1) { Write-Host "autostart: ON" } else { Write-Host "autostart: OFF" }
    return 0
}

function Invoke-Run {
    Write-Host "=== $.crypts Windows launcher ===" -ForegroundColor Cyan
    Write-Host "[1/3] Ensure crypt_autostart binary..."
    $bin = Get-CryptBin
    if (-not $bin) {
        $rc = Invoke-CompileCrypt
        if ($rc -ne 0) { exit $rc }
        $bin = Get-CryptBin
    }
    if (-not $bin) {
        Write-Host "FAIL: no crypt_autostart binary after compile"
        exit 1
    }

    Write-Host "[2/3] Ensure KHTPM Win stubs (if missing)..."
    $tp = Join-Path $HOUSE "&.widgits\tile-picker\ops\+x\tp_desktop_window.exe"
    $tp2 = Join-Path $HOUSE "&.widgits\tile-picker\ops\+x\tp_desktop_window.+x.exe"
    $tb = Join-Path $HOUSE "&.widgits\livedesk-taskbar\ops\+x\tp_taskbar.exe"
    $tb2 = Join-Path $HOUSE "&.widgits\livedesk-taskbar\ops\+x\tp_taskbar.+x.exe"
    $needK = -not ((Test-Path -LiteralPath $tp) -or (Test-Path -LiteralPath $tp2)) -or
             -not ((Test-Path -LiteralPath $tb) -or (Test-Path -LiteralPath $tb2))
    if ($needK) {
        $null = Invoke-CompileKhtpm
    } else {
        Write-Host "  KHTPM bins present"
    }

    Write-Host "[3/3] Run crypt_autostart (CWD=house, relative pdl)"
    # CWD = house root: emoji-safe relative paths (no long abs Unicode in argv)
    Push-Location -LiteralPath $HOUSE
    try {
        # Relative path from house root avoids CreateProcessA/ACP issues
        $relPdl = "$.crypts\autostart.pdl"
        & $bin $relPdl
        $rc = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    if ($null -eq $rc) { $rc = 0 }
    Write-Host "crypt_autostart exit=$rc"
    return $rc
}

$act = $Action.ToLowerInvariant()

if ($act -eq "run" -or $act -eq "r" -or $act -eq "start" -or $act -eq "restart") {
    exit (Invoke-Run)
}
elseif ($act -eq "on") {
    exit (Set-AutostartEnabled 1)
}
elseif ($act -eq "off") {
    exit (Set-AutostartEnabled 0)
}
elseif ($act -eq "status") {
    if (Test-Path -LiteralPath $PDL) {
        Select-String -LiteralPath $PDL -Pattern "enabled" | ForEach-Object { $_.Line }
    } else {
        Write-Host "MISSING $PDL"
        exit 1
    }
    exit 0
}
elseif ($act -eq "compile" -or $act -eq "c" -or $act -eq "build") {
    $rc = Invoke-CompileCrypt
    $rc2 = Invoke-CompileKhtpm
    if ($rc -ne 0) { exit $rc }
    if ($rc2 -ne 0) { exit $rc2 }
    exit 0
}
elseif ($act -eq "check") {
    $bin = Get-CryptBin
    if ($bin) { Write-Host "OK $bin" } else { Write-Host "MISSING crypt_autostart" }
    if (Test-Path -LiteralPath $PDL) { Write-Host "OK $PDL" } else { Write-Host "MISSING $PDL" }
    $tp = Join-Path $HOUSE "&.widgits\tile-picker\ops\+x"
    $tb = Join-Path $HOUSE "&.widgits\livedesk-taskbar\ops\+x"
    Get-ChildItem -LiteralPath $tp -Filter "tp_desktop_window*" -ErrorAction SilentlyContinue |
        ForEach-Object { Write-Host "OK $($_.FullName)" }
    Get-ChildItem -LiteralPath $tb -Filter "tp_taskbar*" -ErrorAction SilentlyContinue |
        ForEach-Object { Write-Host "OK $($_.FullName)" }
    exit 0
}
elseif ($act -eq "kill") {
    Invoke-KillLivedesk
    exit 0
}
elseif ($act -eq "install-xdg") {
    Write-Host "install-xdg is Linux-only (XDG autostart). On Windows use Startup folder later."
    exit 0
}
else {
    Write-Host @'
$.crypts — house-wide autostart control (Windows)

  .\button.ps1 run            # quit livedesk, then launch autostart.pdl
  .\button.ps1 restart        # same as run
  .\button.ps1 on | off       # toggle STATE|enabled in autostart.pdl
  .\button.ps1 status         # show enabled state
  .\button.ps1 compile        # rebuild crypt_autostart + KHTPM Win stubs
  .\button.ps1 check          # verify binary + pdl + KHTPM bins
  .\button.ps1 kill           # stop tp_desktop_window / tp_taskbar by name

Linux: sh button.sh ...
PDL paths must be house-relative (see !.linux-absolute-FIXME-a6.txt).
'@
    exit 0
}
