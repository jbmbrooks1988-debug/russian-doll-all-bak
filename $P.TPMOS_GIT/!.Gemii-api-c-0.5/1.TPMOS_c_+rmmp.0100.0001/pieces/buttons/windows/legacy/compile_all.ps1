# compile_all.ps1 - Compile all CHTPM v0.01 executables for Windows
# TPM-Compliant: Binaries go to their respective piece directories.

Write-Host "=== Compiling CHTPM v0.01 (Windows PowerShell) ===" -ForegroundColor Cyan
Write-Host "Ensure MinGW-w64 is in your PATH." -ForegroundColor Yellow

# KILL EXISTING PROCESSES FIRST (prevent "Permission denied" errors)
Write-Host "Killing existing TPM processes..." -ForegroundColor Gray
Get-Process | Where-Object {$_.Path -like "*TPMOS*"} | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2

$GL_FLAGS = "-LC:/msys64/mingw64/lib -lopengl32 -lglu32 -lfreeglut -lwinmm -lgdi32 -luser32"
$FREETYPE_FLAGS = "-LC:/msys64/mingw64/lib -lfreetype"
$THREAD_FLAGS = "-lpthread"
# -D_WIN32 is typically auto-defined by MinGW, but we include it explicitly
$CFLAGS = "-D_WIN32 -std=gnu11"

function Compile-Piece {
    param([string]$source, [string]$output, [string]$extra_flags = "")

    $out_dir = [System.IO.Path]::GetDirectoryName($output)
    if (-not (Test-Path $out_dir)) {
        New-Item -ItemType Directory $out_dir -Force | Out-Null
    }

    Write-Host "Compiling $source -> $output"
    # MinGW GCC requires libraries AFTER source files
    # -D_WIN32 is auto-defined by MinGW on Windows, but we include it explicitly
    & gcc -D_WIN32 -std=gnu11 $source -o $output $extra_flags $THREAD_FLAGS
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to compile $source"
    }
}

# --- Editor App Piece ---
Write-Host "Compiling editor components..." -ForegroundColor Gray
Compile-Piece "pieces\apps\editor\plugins\editor_module.c" "pieces\apps\editor\plugins\+x\editor_module.+x"
Compile-Piece "pieces\apps\editor\plugins\db_editor_module.c" "pieces\apps\editor\plugins\+x\db_editor_module.+x"
Compile-Piece "pieces\apps\editor\plugins\pal_editor_module.c" "pieces\apps\editor\plugins\+x\pal_editor_module.+x"

Compile-Piece "pieces\apps\editor\ops\place_tile.c" "pieces\apps\editor\ops\+x\place_tile.+x"
Compile-Piece "pieces\apps\editor\ops\delete_piece.c" "pieces\apps\editor\ops\+x\delete_piece.+x"

# --- FuzzPet Module ---
Write-Host "Compiling fuzzpet_app..." -ForegroundColor Gray
Compile-Piece "pieces\apps\fuzzpet_app\fuzzpet\plugins\fuzzpet.c" "pieces\apps\fuzzpet_app\fuzzpet\plugins\+x\fuzzpet.+x"
Compile-Piece "pieces\apps\fuzzpet_app\fuzzpet\plugins\fuzzpet_reset.c" "pieces\apps\fuzzpet_app\fuzzpet\plugins\+x\fuzzpet_reset.+x"
Compile-Piece "pieces\apps\fuzzpet_app\manager\fuzzpet_manager.c" "pieces\apps\fuzzpet_app\manager\plugins\+x\fuzzpet_manager.+x"

# --- Keyboard & Joystick ---
Compile-Piece "pieces\keyboard\src\keyboard_input_win.c" "pieces\keyboard\plugins\+x\keyboard_input.+x"
# Windows: Use XInput for Xbox controllers (built into Windows)
Write-Host "Compiling joystick_input (Windows/XInput)..." -ForegroundColor Gray
& gcc -D_WIN32 -std=gnu11 "pieces\joystick\plugins\joystick_input_win.c" -o "pieces\joystick\plugins\+x\joystick_input.+x" -lxinput -lpthread
if ($LASTEXITCODE -ne 0) {
    Write-Warning "joystick_input skipped - XInput not available"
    Write-Host "  Falling back to keyboard-only input" -ForegroundColor Yellow
}

# --- CHTPM Core ---
Compile-Piece "pieces\chtpm\plugins\chtpm_parser.c" "pieces\chtpm\plugins\+x\chtpm_parser.+x"
Compile-Piece "pieces\chtpm\plugins\chtpm_player.c" "pieces\chtpm\plugins\+x\chtpm_player.+x"
Compile-Piece "pieces\chtpm\plugins\orchestrator.c" "pieces\chtpm\plugins\+x\orchestrator.+x"

# --- Display ---
# Windows: Use windows_renderer.c for text rendering in PowerShell/CMD terminal
# This is the Windows-specific modular renderer (separate from Linux renderer.c)
Write-Host "Compiling display/windows_renderer (Windows text renderer)..." -ForegroundColor Gray
Compile-Piece "pieces\display\windows_renderer.c" "pieces\display\plugins\+x\renderer.+x"
# gl_renderer needs FreeType and OpenGL
Write-Host "Compiling display/gl_renderer (OpenGL/FreeType)..." -ForegroundColor Gray
& gcc -D_WIN32 -std=gnu11 -IC:/msys64/mingw64/include/freetype2 "pieces\display\gl_renderer.c" -o "pieces\display\plugins\+x\gl_renderer.+x" -LC:/msys64/mingw64/lib -lfreeglut -lglu32 -lopengl32 -lfreetype
if ($LASTEXITCODE -ne 0) {
    Write-Warning "gl_renderer skipped - check dependencies (freeglut, freetype)"
}

# --- Master Ledger ---
Compile-Piece "pieces\master_ledger\plugins\piece_manager.c" "pieces\master_ledger\plugins\+x\piece_manager.+x"
Compile-Piece "pieces\master_ledger\plugins\response_handler.c" "pieces\master_ledger\plugins\+x\response_handler.+x"

# --- Clock ---
Compile-Piece "pieces\apps\fuzzpet_app\clock\plugins\increment_time.c" "pieces\apps\fuzzpet_app\clock\plugins\+x\increment_time.+x"
Compile-Piece "pieces\apps\fuzzpet_app\clock\plugins\increment_turn.c" "pieces\apps\fuzzpet_app\clock\plugins\+x\increment_turn.+x"
Compile-Piece "pieces\apps\fuzzpet_app\clock\plugins\end_turn.c" "pieces\apps\fuzzpet_app\clock\plugins\+x\end_turn.+x"

# --- World & Traits ---
Compile-Piece "pieces\apps\fuzzpet_app\world\plugins\map_render.c" "pieces\apps\fuzzpet_app\world\plugins\+x\map_render.+x"
Compile-Piece "pieces\apps\fuzzpet_app\traits\move_entity.c" "pieces\apps\fuzzpet_app\traits\+x\move_entity.+x"
Compile-Piece "pieces\apps\fuzzpet_app\traits\storage_trait.c" "pieces\apps\fuzzpet_app\traits\+x\storage_trait.+x"
Compile-Piece "pieces\apps\fuzzpet_app\traits\zombie_ai.c" "pieces\apps\fuzzpet_app\traits\+x\zombie_ai.+x"

# --- Player App ---
Compile-Piece "pieces\apps\player_app\manager\player_manager.c" "pieces\apps\player_app\manager\plugins\+x\player_manager.+x"
Compile-Piece "pieces\apps\player_app\world\plugins\player_render.c" "pieces\apps\player_app\world\plugins\+x\player_render.+x"
Compile-Piece "pieces\apps\player_app\world\plugins\menu_op.c" "pieces\apps\player_app\world\plugins\+x\menu_op.+x"
Compile-Piece "pieces\apps\player_app\world\plugins\project_loader.c" "pieces\apps\player_app\world\plugins\+x\project_loader.+x"
Compile-Piece "pieces\apps\player_app\world\plugins\move_z.c" "pieces\apps\player_app\world\plugins\+x\move_z.+x"
Compile-Piece "pieces\apps\player_app\world\plugins\interact.c" "pieces\apps\player_app\world\plugins\+x\interact.+x"
Compile-Piece "pieces\apps\player_app\world\plugins\place_tile.c" "pieces\apps\player_app\world\plugins\+x\place_tile.+x"

# --- GL-OS ---
Write-Host "Compiling gl_os components..." -ForegroundColor Gray
& gcc -D_WIN32 -std=gnu11 "pieces\apps\gl_os\plugins\gl_desktop.c" -o "pieces\apps\gl_os\plugins\+x\gl_desktop.+x" -LC:/msys64/mingw64/lib -lfreeglut -lglu32 -lopengl32 -lwinmm -lgdi32 -luser32
if ($LASTEXITCODE -ne 0) { Write-Error "gl_desktop failed" }

Compile-Piece "projects\gl-os\manager\gl_os_manager.c" "projects\gl-os\manager\+x\gl_os_manager.+x"
Compile-Piece "pieces\apps\gl_os\plugins\gl_os_session.c" "pieces\apps\gl_os\plugins\+x\gl_os_session.+x"
Compile-Piece "pieces\apps\gl_os\plugins\gl_os_loader.c" "pieces\apps\gl_os\plugins\+x\gl_os_loader.+x"

# gl_os_renderer might fail if GL headers are missing
& gcc -D_WIN32 -std=gnu11 "pieces\apps\gl_os\plugins\gl_os_renderer.c" -o "pieces\apps\gl_os\plugins\+x\gl_os_renderer.+x" -LC:/msys64/mingw64/lib -lfreeglut -lglu32 -lopengl32 -lwinmm -lgdi32 -luser32
if ($LASTEXITCODE -ne 0) { Write-Warning "gl_os_renderer skipped" }

# --- Shared Ops ---
$ops_src = "pieces\apps\playrm\ops\src"
$ops_dest = "pieces\apps\playrm\ops\+x"
$ops_list = @("move_player", "move_z", "move_selector", "interact", "render_map", "menu_op", "project_loader", "console_print", "place_tile", "create_piece", "undo_action", "move_entity", "fuzzpet_action", "stat_decay")

foreach ($op in $ops_list) {
    Compile-Piece "$ops_src\$op.c" "$ops_dest\$op.+x"
}

Compile-Piece "pieces\apps\playrm\plugins\playrm_module.c" "pieces\apps\playrm\plugins\+x\playrm_module.+x"
Compile-Piece "pieces\apps\playrm\loader\loader_module.c" "pieces\apps\playrm\loader\plugins\+x\loader_module.+x"

# --- Prisc & System ---
Compile-Piece "pieces\system\prisc\prisc+x.c" "pieces\system\prisc\prisc+x"
Compile-Piece "pieces\system\pdl\pdl_reader.c" "pieces\system\pdl\+x\pdl_reader.+x"
Compile-Piece "pieces\apps\man-pal\plugins\man-pal_module.c" "pieces\apps\man-pal\plugins\+x\man-pal_module.+x"
Compile-Piece "pieces\apps\man-ops\plugins\man-ops_module.c" "pieces\apps\man-ops\plugins\+x\man-ops_module.+x"
Compile-Piece "pieces\apps\op-ed\plugins\op-ed_module.c" "pieces\apps\op-ed\plugins\+x\op-ed_module.+x"

# --- User (OpenSSL) ---
Write-Host "Compiling user_module (OpenSSL)..." -ForegroundColor Gray
& gcc -o "pieces\apps\user\plugins\+x\user_module.+x" "pieces\apps\user\plugins\user_module.c" $CFLAGS $THREAD_FLAGS -lssl -lcrypto
if ($LASTEXITCODE -ne 0) { Write-Warning "user_module skipped - check OpenSSL" }

# --- Locations & OS ---
Compile-Piece "pieces\locations\path_utils.c" "pieces\locations\+x\path_utils.+x"
Compile-Piece "pieces\os\plugins\proc_manager.c" "pieces\os\plugins\+x\proc_manager.+x"
Compile-Piece "pieces\system\clock_daemon\plugins\clock_daemon.c" "pieces\system\clock_daemon\plugins\+x\clock_daemon.+x"

Write-Host "=== Compilation Complete ===" -ForegroundColor Green
