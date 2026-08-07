# button.ps1 - Windows launcher for mutaclsym (parity with button.sh)
# All-in-one game (no board-viewer widget). Linux stays on button.sh.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\button.ps1 <action>
# Actions: compile|c|build | run|r|start | gl|mirror | kill|k|stop
#          check|verify | generate|gen | help
#
# Env: NO_GL=1 skip freeglut; NO_TERM=1 skip same-console renderer
#      PAL_LAYOUT=... or -Pal for pieces/chtpm/layouts/game.chtpm via PAL_LAYOUT

param(
    [Parameter(Position = 0)]
    [string]$Action = "help",
    [switch]$Pal,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Rest
)

$ErrorActionPreference = "Continue"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $SCRIPT_DIR

$MSYS = "C:\msys64\mingw64\bin"
if (Test-Path $MSYS) {
    if ($env:Path -notlike "*$MSYS*") { $env:Path = "$MSYS;$env:Path" }
}

if ($Rest) {
    foreach ($a in $Rest) {
        if ($a -eq "--pal") { $Pal = $true }
    }
}

# House root (parent of 101.mutaclsym*) for donor Win PE / freeglut
$HOUSE_DIR = Split-Path $SCRIPT_DIR -Parent

function Get-Bin([string]$rel) {
    $exe = Join-Path $SCRIPT_DIR ($rel + ".exe")
    if (Test-Path -LiteralPath $exe) { return $exe }
    $plain = Join-Path $SCRIPT_DIR $rel
    if (Test-Path -LiteralPath $plain) {
        # Reject non-PE (Linux ELF left on disk from Linux builds)
        try {
            $fs = [System.IO.File]::OpenRead($plain)
            $b0 = $fs.ReadByte(); $b1 = $fs.ReadByte(); $fs.Close()
            if ($b0 -eq 0x4D -and $b1 -eq 0x5A) { return $plain }
        } catch {}
    }
    return $null
}

function Write-Utf8NoBom([string]$Path, [string]$Text) {
    $dir = Split-Path -Parent $Path
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($Path, $Text, $utf8)
}

# Stage . +x PE to temp .exe (Windows has no PATHEXT for . +x)
function Invoke-HouseBin {
    param(
        [string]$Path,
        [string]$WorkDir = $SCRIPT_DIR,
        [string[]]$BinArgs = $null,
        [switch]$Wait,
        [switch]$Hidden
    )
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) {
        Write-Host "  SKIP missing bin: $Path" -ForegroundColor Yellow
        return $null
    }
    $wd = if ($WorkDir -and (Test-Path -LiteralPath $WorkDir)) { $WorkDir } else { $SCRIPT_DIR }
    $runPath = $Path
    $tmp = $null
    if ($Path -match '\.\+x$') {
        $tmp = Join-Path $env:TEMP ("muta_{0}_{1}.exe" -f $PID, [Guid]::NewGuid().ToString("N").Substring(0, 8))
        Copy-Item -LiteralPath $Path -Destination $tmp -Force
        $runPath = $tmp
    }
    $winStyle = if ($Hidden) { "Hidden" } else { "Normal" }
    try {
        $sp = @{
            FilePath         = $runPath
            WorkingDirectory = $wd
            WindowStyle      = $winStyle
            PassThru         = $true
        }
        if ($BinArgs -and $BinArgs.Count -gt 0) { $sp.ArgumentList = $BinArgs }
        $proc = Start-Process @sp
        if ($Wait -and $proc) {
            Wait-Process -Id $proc.Id -ErrorAction SilentlyContinue
            return $null
        }
        return $proc
    } finally {
        if ($Wait -and $tmp) {
            Remove-Item -LiteralPath $tmp -Force -EA SilentlyContinue
        }
    }
}

function Find-WsrSystem([string]$name) {
    $wsr = Get-ChildItem -LiteralPath $HOUSE_DIR -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like '014.wsr*' } | Select-Object -First 1
    if (-not $wsr) { return $null }
    foreach ($c in @(
        (Join-Path $wsr.FullName "system\$name.exe"),
        (Join-Path $wsr.FullName "system\$name")
    )) {
        if (Test-Path -LiteralPath $c) {
            try {
                $fs = [System.IO.File]::OpenRead($c)
                $b0 = $fs.ReadByte(); $b1 = $fs.ReadByte(); $fs.Close()
                if ($b0 -eq 0x4D -and $b1 -eq 0x5A) { return $c }
            } catch {}
        }
    }
    return $null
}

function Ensure-FreeGlut {
    foreach ($glut in @("$MSYS\libfreeglut.dll", "$MSYS\freeglut.dll")) {
        if (-not (Test-Path $glut)) { continue }
        $name = Split-Path $glut -Leaf
        $dest = Join-Path $SCRIPT_DIR "system\$name"
        if (-not (Test-Path -LiteralPath $dest)) {
            Copy-Item -LiteralPath $glut -Destination $dest -Force -EA SilentlyContinue
        }
        break
    }
}

function Invoke-Kill {
    Write-Host "Stopping mutaclsym processes (name-only)..."
    $names = @(
        "keyboard_input", "renderer", "prisc+x", "chtpm_parser_pal",
        "chtpm_rgb_render", "gl_mirror", "orchestrator",
        "compose_frame", "compose_rgb_frame", "game_dispatch",
        "move_player", "title_input", "tick_monsters"
    )
    foreach ($n in $names) {
        Get-Process -ErrorAction SilentlyContinue |
            Where-Object { $_.ProcessName -eq $n -or $_.ProcessName -like "$n*" } |
            ForEach-Object { try { Stop-Process -Id $_.Id -Force } catch {} }
        & taskkill /F /IM "$n.exe" 2>$null | Out-Null
        & taskkill /F /IM $n 2>$null | Out-Null
    }
    foreach ($f in @(
        "pieces\system\gl_focus.lock",
        "pieces\system\quit_flag.txt",
        "pieces\os\proc_list.txt"
    )) {
        $p = Join-Path $SCRIPT_DIR $f
        if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Force -EA SilentlyContinue }
    }
    Write-Host "done"
}

function Invoke-Compile {
    Write-Host "=== mutaclsym compile (Win / MinGW) ===" -ForegroundColor Cyan
    if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
        Write-Host "gcc not on PATH - install MSYS2 mingw64 or add $MSYS" -ForegroundColor Yellow
        Write-Host "Will still use any existing .exe / donor PE from 014.wsr-pal"
    }

    New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "system") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "ops\+x") -Force | Out-Null
    Ensure-FreeGlut

    # Donor keyboard (muta local tree is often Linux-only termios)
    if (-not (Get-Bin "system\keyboard_input")) {
        $donor = Find-WsrSystem "keyboard_input"
        if ($donor) {
            Copy-Item -LiteralPath $donor -Destination (Join-Path $SCRIPT_DIR "system\keyboard_input.exe") -Force
            Write-Host "OK   system/keyboard_input.exe (from 014.wsr-pal)"
        } else {
            Write-Host "MISS system/keyboard_input.exe (no wsr donor)"
        }
    } else {
        Write-Host "OK   system/keyboard_input (present)"
    }

    # Keep muta's own freeglut gl_mirror if PE; else copy wsr
    $gl = Get-Bin "system\gl_mirror"
    if (-not $gl) {
        $donor = Find-WsrSystem "gl_mirror"
        if ($donor) {
            Copy-Item -LiteralPath $donor -Destination (Join-Path $SCRIPT_DIR "system\gl_mirror.exe") -Force
            Write-Host "OK   system/gl_mirror.exe (from 014.wsr-pal)"
        }
    } else {
        Write-Host "OK   system/gl_mirror (present)"
    }

    # Optional: recompile system .c when gcc exists (parser win_spawn is load-bearing)
    if (Get-Command gcc -ErrorAction SilentlyContinue) {
        $sysPairs = @(
            @{ Src = "system\chtpm_parser_pal.c"; Out = "system\chtpm_parser_pal.exe" },
            @{ Src = "system\chtpm_rgb_render.c"; Out = "system\chtpm_rgb_render.exe" },
            @{ Src = "system\renderer.c"; Out = "system\renderer.exe" },
            @{ Src = "system\prisc+x.c"; Out = "system\prisc+x.exe" }
        )
        foreach ($pair in $sysPairs) {
            $src = Join-Path $SCRIPT_DIR $pair.Src
            $out = Join-Path $SCRIPT_DIR $pair.Out
            if (-not (Test-Path -LiteralPath $src)) { continue }
            if ((Test-Path -LiteralPath $out) -and
                (Get-Item -LiteralPath $out).LastWriteTime -ge (Get-Item -LiteralPath $src).LastWriteTime) {
                Write-Host "OK   $($pair.Out) (up to date)"
                continue
            }
            & gcc -Wall -O2 -o $out $src 2>$null
            if ($LASTEXITCODE -eq 0) { Write-Host "OK   $($pair.Out)" }
            else { Write-Host "SKIP $($pair.Out) (gcc failed - keep existing if any)" }
        }

        # Ops that are pure C (no X11) - best effort
        $ops = @(
            "compose_frame", "compose_rgb_frame", "compose_title_frame",
            "title_input", "game_dispatch", "move_player", "end_turn",
            "tick_monsters", "choice", "pdl_reader", "save_game",
            "pickup", "drop", "eat", "examine", "craft", "generate_map",
            "dump_rgb_png", "toggle_emoji", "camera_control"
        )
        foreach ($op in $ops) {
            $src = Join-Path $SCRIPT_DIR "ops\$op.c"
            $out = Join-Path $SCRIPT_DIR "ops\+x\$op.+x"
            if (-not (Test-Path -LiteralPath $src)) { continue }
            if ((Test-Path -LiteralPath $out)) {
                try {
                    $fs = [System.IO.File]::OpenRead($out)
                    $b0 = $fs.ReadByte(); $b1 = $fs.ReadByte(); $fs.Close()
                    if ($b0 -eq 0x4D -and $b1 -eq 0x5A -and
                        (Get-Item $out).LastWriteTime -ge (Get-Item $src).LastWriteTime) {
                        continue
                    }
                } catch {}
            }
            & gcc -Wall -O2 -o $out $src 2>$null
            if ($LASTEXITCODE -eq 0) { Write-Host "OK   ops/+x/$op.+x" }
        }
    }

    # Report stack
    $core = @(
        "system\chtpm_parser_pal.exe", "system\chtpm_rgb_render.exe",
        "system\prisc+x.exe", "system\renderer.exe", "system\gl_mirror.exe"
    )
    $ok = 0
    foreach ($b in $core) {
        if (Test-Path (Join-Path $SCRIPT_DIR $b)) { $ok++ }
        elseif (Get-Bin ($b -replace '\.exe$', '')) { $ok++ }
    }
    Write-Host "core PE present: $ok / $($core.Count) (+ keyboard donor if copied)"
    return 0
}

function Invoke-Check {
    $bins = @(
        "system\prisc+x", "system\keyboard_input", "system\renderer",
        "system\chtpm_parser_pal", "system\chtpm_rgb_render",
        "ops\+x\move_player.+x", "ops\+x\end_turn.+x", "ops\+x\compose_frame.+x",
        "ops\+x\compose_rgb_frame.+x", "ops\+x\title_input.+x",
        "ops\+x\compose_title_frame.+x", "ops\+x\game_dispatch.+x",
        "ops\+x\generate_map.+x", "ops\+x\tick_monsters.+x"
    )
    foreach ($b in $bins) {
        $p = Get-Bin $b
        if (-not $p) {
            $try = Join-Path $SCRIPT_DIR $b
            if (Test-Path -LiteralPath $try) { $p = $try }
            $trye = Join-Path $SCRIPT_DIR ($b + ".exe")
            if (-not $p -and (Test-Path -LiteralPath $trye)) { $p = $trye }
        }
        if ($p) { Write-Host "OK   $b" } else { Write-Host "MISSING $b" }
    }
    $gl = Get-Bin "system\gl_mirror"
    if ($gl) { Write-Host "OK   system/gl_mirror (GL)" }
    else { Write-Host "SKIP system/gl_mirror (optional - freeglut)" }
    return 0
}

function Enable-ConsoleUtf8 {
    # Frame is UTF-8. Default OEM code page shows mojibake; use UTF-8 console.
    try { chcp 65001 | Out-Null } catch {}
    try {
        [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
        [Console]::InputEncoding  = [System.Text.Encoding]::UTF8
        $global:OutputEncoding    = [System.Text.Encoding]::UTF8
    } catch {}
}

# 8.3 short path - Start-Process WorkingDirectory + MinGW fopen fail on emoji dirs
function Get-ShortPath([string]$Path) {
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) { return $Path }
    try {
        $fso = New-Object -ComObject Scripting.FileSystemObject
        if (Test-Path -LiteralPath $Path -PathType Container) {
            return $fso.GetFolder($Path).ShortPath
        }
        return $fso.GetFile($Path).ShortPath
    } catch {
        return $Path
    }
}

# Stage PE under %TEMP% so CreateProcess does not load from emoji path
function Stage-Pe([string]$src) {
    if (-not $src -or -not (Test-Path -LiteralPath $src)) { return $null }
    $name = [IO.Path]::GetFileNameWithoutExtension($src)
    $tmp = Join-Path $env:TEMP ("muta_{0}_{1}.exe" -f $name, $PID)
    Copy-Item -LiteralPath $src -Destination $tmp -Force
    # freeglut next to staged gl_mirror
    if ($name -match 'gl_mirror') {
        foreach ($dll in @("libfreeglut.dll", "freeglut.dll")) {
            $cand = Join-Path (Split-Path $src -Parent) $dll
            if (Test-Path -LiteralPath $cand) {
                Copy-Item -LiteralPath $cand -Destination (Join-Path $env:TEMP $dll) -Force -EA SilentlyContinue
            }
        }
    }
    return $tmp
}

function Start-MutaPe {
    param(
        [string]$ExePath,
        [string]$WorkDirShort,
        [string[]]$BinArgs = $null,
        [switch]$Hidden,
        [switch]$NoNewWindow
    )
    $staged = Stage-Pe $ExePath
    if (-not $staged) { return $null }
    $sp = @{
        FilePath         = $staged
        WorkingDirectory = $WorkDirShort
        PassThru         = $true
    }
    if ($NoNewWindow) { $sp.NoNewWindow = $true }
    elseif ($Hidden) { $sp.WindowStyle = "Hidden" }
    else { $sp.WindowStyle = "Normal" }
    if ($BinArgs -and $BinArgs.Count -gt 0) { $sp.ArgumentList = $BinArgs }
    return Start-Process @sp
}

function Invoke-Run {
    Write-Host "=== mutaclsym Windows launcher (all-in-1, no widgets) ===" -ForegroundColor Cyan
    Enable-ConsoleUtf8

    # Ensure keyboard + freeglut
    if (-not (Get-Bin "system\keyboard_input")) {
        Write-Host "keyboard_input PE missing - compile/donor..."
        $null = Invoke-Compile
    }
    Ensure-FreeGlut

    $parser = Get-Bin "system\chtpm_parser_pal"
    $kb = Get-Bin "system\keyboard_input"
    $prisc = Get-Bin "system\prisc+x"
    if (-not $parser -or -not $kb) {
        Write-Error "MISSING chtpm_parser_pal or keyboard_input PE. Run: .\button.ps1 compile"
        return 1
    }
    if (-not $prisc) {
        Write-Host "WARN: no prisc+x PE - Control Hero will fail (parser win_spawn needs .exe)" -ForegroundColor Yellow
    }

    Write-Host "[1/3] Kill previous..."
    Invoke-Kill

    Write-Host "[2/3] Seed markers + dirs..."
    foreach ($d in @(
        "pieces\display", "pieces\apps\player_app", "pieces\keyboard",
        "pieces\system", "pieces\os"
    )) {
        New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR $d) -Force | Out-Null
    }
    foreach ($f in @(
        "pieces\display\renderer_pulse.txt",
        "pieces\display\frame_changed.txt",
        "pieces\apps\player_app\history.txt",
        "pieces\apps\player_app\interact_relay.txt",
        "pieces\keyboard\history.txt",
        "pieces\system\quit_flag.txt",
        "pieces\system\gl_focus.lock",
        "pieces\os\proc_list.txt"
    )) {
        $fp = Join-Path $SCRIPT_DIR $f
        if ($f -like "*gl_focus.lock") {
            if (Test-Path -LiteralPath $fp) { Remove-Item -LiteralPath $fp -Force -EA SilentlyContinue }
        } else {
            Write-Utf8NoBom $fp ""
        }
    }

    # CRITICAL: relative root (emoji house absolute paths break MinGW fopen)
    $env:PRISC_PROJECT_ROOT = "."
    $env:PRISC_PROJECT_ID = "mutaclsym"
    $env:SKIP_ORCH_COMPILE = "1"
    if ($Pal -or $env:PAL_LAYOUT) {
        if (-not $env:PAL_LAYOUT) {
            $env:PAL_LAYOUT = "pieces/chtpm/layouts/game.chtpm"
        }
        Write-Host "Mode: PAL_LAYOUT=$($env:PAL_LAYOUT)"
    } else {
        $env:PAL_LAYOUT = "pieces/chtpm/layouts/game.chtpm"
        Write-Host "Mode: CHTPM game.chtpm (default)"
    }

    $wdShort = Get-ShortPath $SCRIPT_DIR
    Write-Host ("[3/3] Launch stack wd={0} PRISC_PROJECT_ROOT=." -f $wdShort)

    $rend = Get-Bin "system\renderer"
    $rgb = Get-Bin "system\chtpm_rgb_render"
    $gl = Get-Bin "system\gl_mirror"

    $pRend = $null
    $pParser = $null
    $pRgb = $null
    $pGl = $null

    $layout = if ($env:PAL_LAYOUT) { $env:PAL_LAYOUT } else { "pieces/chtpm/layouts/game.chtpm" }
    # Stage PE to %TEMP% + short WorkingDirectory so relative pieces/* open
    $pParser = Start-MutaPe -ExePath $parser -WorkDirShort $wdShort -BinArgs @($layout) -Hidden
    if ($pParser) { Write-Host "parser pid=$($pParser.Id) layout=$layout" }
    else { Write-Error "parser failed to start"; return 1 }

    $frame = Join-Path $SCRIPT_DIR "pieces\display\current_frame.txt"
    for ($i = 0; $i -lt 50; $i++) {
        if ((Test-Path -LiteralPath $frame) -and ((Get-Item -LiteralPath $frame).Length -gt 0)) {
            Write-Host ("  frame ready ({0} bytes)" -f (Get-Item -LiteralPath $frame).Length)
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not (Test-Path -LiteralPath $frame) -or ((Get-Item -LiteralPath $frame).Length -eq 0)) {
        Write-Host "WARN: current_frame.txt still empty - check debug.txt" -ForegroundColor Yellow
        $dbg = Join-Path $SCRIPT_DIR "debug.txt"
        if (Test-Path -LiteralPath $dbg) { Get-Content -LiteralPath $dbg -Tail 20 }
    }

    # Term: separate window by default so THIS console stays free for keyboard
    if ($rend -and $env:NO_TERM -ne "1") {
        if ($env:TERM_SAME -eq "1") {
            $pRend = Start-MutaPe -ExePath $rend -WorkDirShort $wdShort -NoNewWindow
            Write-Host ("TERM renderer pid={0} (same console)" -f $pRend.Id)
        } else {
            $pRend = Start-MutaPe -ExePath $rend -WorkDirShort $wdShort
            Write-Host ("TERM renderer pid={0} (own window)" -f $pRend.Id)
        }
    }

    if ($env:NO_GL -ne "1") {
        if ($rgb) {
            $pRgb = Start-MutaPe -ExePath $rgb -WorkDirShort $wdShort -Hidden
            Write-Host "RGB chtpm_rgb_render pid=$($pRgb.Id)"
        }
        $raw = Join-Path $SCRIPT_DIR "pieces\display\rgb_frame.raw"
        for ($i = 0; $i -lt 60; $i++) {
            if ((Test-Path -LiteralPath $raw) -and ((Get-Item -LiteralPath $raw).Length -gt 10000)) {
                Write-Host ("  rgb_frame.raw ready ({0} bytes)" -f (Get-Item -LiteralPath $raw).Length)
                break
            }
            Start-Sleep -Milliseconds 100
        }
        if ($gl) {
            $pGl = Start-MutaPe -ExePath $gl -WorkDirShort $wdShort
            Start-Sleep -Milliseconds 500
            if ($pGl -and $pGl.HasExited) {
                Write-Host ("FAIL: gl_mirror exited (code={0}) - freeglut DLL?" -f $pGl.ExitCode) -ForegroundColor Red
            } elseif ($pGl) {
                Write-Host ("GL gl_mirror pid={0}" -f $pGl.Id)
            }
        } else {
            Write-Host "OPTIONAL: no gl_mirror PE (ASCII only)" -ForegroundColor Yellow
        }
    }

    Write-Host ""
    Write-Host "INPUT: focus THIS console for menu/game keys (native WT/conhost)."
    Write-Host "       Or click freeglut GL window - same history files."
    Write-Host "Enter = Control Hero / confirm. Ctrl+C quits."
    Write-Host ""

    # Keyboard MUST run with CWD = project (short) so history fopen works.
    # Stage PE; NoNewWindow keeps stdin on this console for _getch.
    $kbStaged = Stage-Pe $kb
    if (-not $kbStaged) {
        Write-Error "failed to stage keyboard_input"
        return 1
    }
    try {
        Set-Location -LiteralPath $wdShort
        # ProcessStartInfo so WorkingDirectory is honored without shell.exe
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $kbStaged
        $psi.WorkingDirectory = $wdShort
        $psi.UseShellExecute = $false
        $psi.RedirectStandardInput = $false
        $psi.RedirectStandardOutput = $false
        $psi.RedirectStandardError = $false
        $psi.CreateNoWindow = $false
        $pKb = [Diagnostics.Process]::Start($psi)
        if ($pKb) {
            Write-Host ("keyboard pid={0} (wait...)" -f $pKb.Id)
            $pKb.WaitForExit()
        } else {
            # Fallback: call operator after Set-Location
            & $kbStaged
        }
    } finally {
        Set-Location -LiteralPath $SCRIPT_DIR
        foreach ($p in @($pGl, $pRgb, $pRend, $pParser)) {
            if ($p -and -not $p.HasExited) {
                try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
            }
        }
        Get-Process -EA SilentlyContinue |
            Where-Object { $_.ProcessName -eq "prisc+x" -or $_.ProcessName -like "prisc*" } |
            ForEach-Object { try { Stop-Process -Id $_.Id -Force } catch {} }
        Invoke-Kill
    }
    return 0
}

function Invoke-Gl {
    Ensure-FreeGlut
    $gl = Get-Bin "system\gl_mirror"
    if (-not $gl) {
        Write-Error "system/gl_mirror not built. Run: .\button.ps1 compile"
        return 1
    }
    New-Item -ItemType Directory -Path (Join-Path $SCRIPT_DIR "pieces\display") -Force | Out-Null
    $env:PRISC_PROJECT_ROOT = "."
    $env:PRISC_PROJECT_ID = "mutaclsym"
    Write-Host "Standalone gl_mirror (need run stack so rgb_frame.raw updates)."
    Write-Host "Receipts: pieces/display/gl_display.receipt.txt , rgb_frame.receipt.txt"
    & $gl
    return $LASTEXITCODE
}

function Invoke-Generate {
    $env:PRISC_PROJECT_ROOT = "."
    $env:PRISC_PROJECT_ID = "mutaclsym"
    $gen = Get-Bin "ops\+x\generate_map.+x"
    if (-not $gen) {
        $cand = Join-Path $SCRIPT_DIR "ops\+x\generate_map.+x"
        if (Test-Path -LiteralPath $cand) { $gen = $cand }
    }
    if (-not $gen) {
        Write-Error "ops/+x/generate_map.+x missing - compile first"
        return 1
    }
    $null = Invoke-HouseBin -Path $gen -WorkDir $SCRIPT_DIR -Wait -BinArgs $Rest
    return 0
}

function Show-Help {
    Write-Host @"
mutaclsym button.ps1 (Windows)

Usage: powershell -ExecutionPolicy Bypass -File .\button.ps1 ACTION

Actions:
  compile, c, build   - MinGW build + donor PE (keyboard/gl from wsr)
  run, r, start       - CHTPM stack: parser + rgb + gl + keyboard
  gl, mirror          - Standalone gl_mirror only
  kill, k, stop       - Kill mutaclsym process names
  check, verify       - List required bins
  generate, gen ...   - ops/+x/generate_map.+x
  help, h             - This text

Env:
  NO_GL=1             - Skip freeglut
  NO_TERM=1           - Skip ASCII renderer
  TERM_SAME=1         - Renderer shares this console (default: own window)

Notes:
  chcp 65001 for UTF-8 map glyphs.
  Parser win_spawn = wsr CreateProcessW + .exe (needed for Control Hero).

  1. .\button.ps1 compile
  2. .\button.ps1 check
  3. .\button.ps1 run
"@
}

$a = $Action.ToLowerInvariant()
switch ($a) {
    { $_ -in @("compile", "c", "build") } { exit (Invoke-Compile) }
    { $_ -in @("run", "r", "start") }     { exit (Invoke-Run) }
    { $_ -in @("gl", "mirror") }          { exit (Invoke-Gl) }
    { $_ -in @("kill", "k", "stop") }     { Invoke-Kill; exit 0 }
    { $_ -in @("check", "verify") }       { exit (Invoke-Check) }
    { $_ -in @("generate", "gen") }       { exit (Invoke-Generate) }
    { $_ -in @("help", "h", "-h", "--help") } { Show-Help; exit 0 }
    default {
        Write-Host "Unknown action: $Action"
        Show-Help
        exit 1
    }
}
