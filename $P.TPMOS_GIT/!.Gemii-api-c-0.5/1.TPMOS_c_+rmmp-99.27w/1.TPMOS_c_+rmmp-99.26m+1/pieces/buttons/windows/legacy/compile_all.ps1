# compile_all.ps1 - Compile ALL CHTPM binaries for Windows
# TPM-Compliant: Binaries go to their respective +x directories.

Write-Host "=== CHTPM COMPILE ALL (Windows PowerShell) ===" -ForegroundColor Cyan
Write-Host "Date: $(Get-Date)"
Write-Host "Ensure MinGW-w64 is in your PATH." -ForegroundColor Yellow

$COMPILED = 0
$FAILED = 0

# KILL EXISTING PROCESSES FIRST
Write-Host "Killing existing TPM processes..." -ForegroundColor Gray
Get-Process | Where-Object { $_.Name -like "*+x*" -or $_.Name -eq "chtpm_parser" } | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

function Compile-Piece {
    param([string]$source, [string]$output, [string]$extra_flags = "")

    if (Test-Path $source) {
        $out_dir = [System.IO.Path]::GetDirectoryName($output)
        if (-not (Test-Path $out_dir)) {
            New-Item -ItemType Directory $out_dir -Force | Out-Null
        }

        Write-Host "  Compiling $source -> $output"
        $shim = "pieces/os/plugins/win_posix_shim.h"
        $inc = "-Ipieces/os/plugins -Ipieces/os/plugins/sys"
        & gcc -D_WIN32 -std=gnu11 -include $shim $inc $source -o $output $extra_flags -lws2_32 -lpthread -lm
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "    v Success" -ForegroundColor Green
            $script:COMPILED++
        } else {
            Write-Error "    x FAILED: $source"
            $script:FAILED++
        }
    }
}

Write-Host "`n=== 1. SYSTEM COMPONENTS ===" -ForegroundColor Cyan
Compile-Piece "pieces\keyboard\src\keyboard_input_win.c" "pieces\keyboard\plugins\+x\keyboard_input.+x"

Write-Host "Compiling joystick_input (Windows/XInput)..." -ForegroundColor Gray
& gcc -D_WIN32 -std=gnu11 "pieces\joystick\plugins\joystick_input_win.c" -o "pieces\joystick\plugins\+x\joystick_input.+x" -lxinput -lpthread -lm
if ($LASTEXITCODE -eq 0) { $COMPILED++ } else { Write-Warning "  joystick_input_win skipped" }

Compile-Piece "pieces\chtpm\plugins\chtpm_parser.c" "pieces\chtpm\plugins\+x\chtpm_parser.+x"
Compile-Piece "pieces\chtpm\plugins\orchestrator.c" "pieces\chtpm\plugins\+x\orchestrator.+x"
Compile-Piece "pieces\display\windows_renderer.c" "pieces\display\plugins\+x\renderer.+x"
Compile-Piece "pieces\system\pdl\pdl_reader.c" "pieces\system\pdl\+x\pdl_reader.+x"

$GL_EXTRA = "-I`"C:/msys64/mingw64/include/freetype2`" -lfreeglut -lglu32 -lopengl32 -lfreetype -LC:/msys64/mingw64/lib"
Compile-Piece "pieces\display\gl_renderer.c" "pieces\display\plugins\+x\gl_renderer.+x" $GL_EXTRA

Write-Host "`n=== 1c. CHTPM OPS ===" -ForegroundColor Cyan
if (Test-Path "pieces\chtpm\ops") {
    $ops = Get-ChildItem "pieces\chtpm\ops\*.c"
    foreach ($op in $ops) {
        Compile-Piece $op.FullName "pieces\chtpm\ops\+x\$($op.BaseName).+x"
    }
}

Write-Host "`n=== 2. SYSTEM APPS ===" -ForegroundColor Cyan
Compile-Piece "pieces\apps\op-ed\plugins\op-ed_module.c" "pieces\apps\op-ed\plugins\+x\op-ed_module.+x"
Compile-Piece "pieces\apps\player_app\manager\player_manager.c" "pieces\apps\player_app\manager\plugins\+x\player_manager.+x"

if (Test-Path "pieces\apps\player_app\world\plugins") {
    $plugins = Get-ChildItem "pieces\apps\player_app\world\plugins\*.c"
    foreach ($p in $plugins) {
        Compile-Piece $p.FullName "pieces\apps\player_app\world\plugins\+x\$($p.BaseName).+x"
    }
}

Write-Host "`n=== 2b. GL-OS APP ===" -ForegroundColor Cyan
$GL_OS_LIBS = "-lfreeglut -lglu32 -lopengl32 -lwinmm -lgdi32 -luser32 -LC:/msys64/mingw64/lib"
Compile-Piece "pieces\apps\gl_os\plugins\gl_desktop.c" "pieces\apps\gl_os\plugins\+x\gl_desktop.+x" $GL_OS_LIBS
Compile-Piece "pieces\apps\gl_os\plugins\gl_os_renderer.c" "pieces\apps\gl_os\plugins\+x\gl_os_renderer.+x" $GL_OS_LIBS
Compile-Piece "pieces\apps\gl_os\plugins\gl_os_session.c" "pieces\apps\gl_os\plugins\+x\gl_os_session.+x"
Compile-Piece "pieces\apps\gl_os\plugins\gl_os_loader.c" "pieces\apps\gl_os\plugins\+x\gl_os_loader.+x"
Compile-Piece "pieces\apps\gl_os\plugins\gl_os.c" "pieces\apps\gl_os\plugins\+x\gl_os.+x"

Write-Host "`n=== 3. PLAYRM OPS (SHARED) ===" -ForegroundColor Cyan
if (Test-Path "pieces\apps\playrm\ops\src") {
    $ops = Get-ChildItem "pieces\apps\playrm\ops\src\*.c"
    foreach ($op in $ops) {
        Compile-Piece $op.FullName "pieces\apps\playrm\ops\+x\$($op.BaseName).+x"
    }
}

Write-Host "`n=== 3b. PLAYRM PLUGINS ===" -ForegroundColor Cyan
Compile-Piece "pieces\apps\playrm\plugins\playrm_module.c" "pieces\apps\playrm\plugins\+x\playrm_module.+x"
Compile-Piece "pieces\apps\playrm\loader\loader_module.c" "pieces\apps\playrm\loader\plugins\+x\loader_module.+x"

Write-Host "`n=== 4. PROJECT MANAGERS ===" -ForegroundColor Cyan
$projects = Get-ChildItem "projects" | Where-Object { $_.PSIsContainer -and $_.Name -ne "trunk" }
foreach ($proj in $projects) {
    $proj_name = $proj.Name
    $manager_src = "projects\$proj_name\manager\$($proj_name)_manager.c"
    if ($proj_name -eq "gl-os") { $manager_src = "projects\gl-os\manager\gl_os_manager.c" }
    
    if (Test-Path $manager_src) {
        $extra = ""
        if ($proj_name -eq "user") { $extra = "-lcrypto" }
        Compile-Piece $manager_src "projects\$proj_name\manager\+x\$($proj_name)_manager.+x" $extra
    }

    if ($proj_name -eq "op-ed") {
        Compile-Piece "projects\op-ed\manager\pal_editor_module.c" "projects\op-ed\manager\+x\pal_editor_module.+x"
    }
}

Write-Host "`n=== 5. PROJECT OPS ===" -ForegroundColor Cyan
foreach ($proj in $projects) {
    $proj_name = $proj.Name
    $op_dirs = @("projects\$proj_name\ops", "projects\$proj_name\ops\src")
    foreach ($dir in $op_dirs) {
        if (Test-Path $dir) {
            $ops = Get-ChildItem "$dir\*.c"
            foreach ($op in $ops) {
                Compile-Piece $op.FullName "projects\$proj_name\ops\+x\$($op.BaseName).+x"
            }
        }
    }
}

Write-Host "`n=== 7. SHARED APP TRAITS ===" -ForegroundColor Cyan
Compile-Piece "pieces\apps\fuzzpet_app\traits\zombie_ai.c" "pieces\apps\fuzzpet_app\traits\+x\zombie_ai.+x"

Write-Host "`n=== COMPILE SUMMARY ===" -ForegroundColor Cyan
Write-Host "  Compiled: $COMPILED binaries"
if ($FAILED -gt 0) {
    Write-Host "  Failed:   $FAILED binaries" -ForegroundColor Red
} else {
    Write-Host "  All binaries compiled successfully!" -ForegroundColor Green
}
