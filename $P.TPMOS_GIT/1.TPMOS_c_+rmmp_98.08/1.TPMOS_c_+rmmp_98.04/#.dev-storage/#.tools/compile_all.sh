#!/bin/bash
# compile_all.sh - Compile ALL CHTPM binaries (projects + system)
# Usage: ./#.dev-storage/#.tools/compile_all.sh

echo "=== CHTPM COMPILE ALL ==="
echo "Date: $(date)"
echo ""

COMPILED=0
FAILED=0

compile_op() {
    local src="$1"
    local dst="$2"

    if [ -f "$src" ]; then
        mkdir -p "$(dirname "$dst")"
        if gcc -o "$dst" "$src" -pthread -lm -Wno-format-truncation -Wno-format-extra-args; then
            if [ -f "$dst" ]; then
                echo "  ✓ $dst"
                ((COMPILED++))
            else
                echo "  ✗ FAILED (No binary): $src"
                ((FAILED++))
            fi
        else
            echo "  ✗ FAILED: $src"
            ((FAILED++))
        fi
    fi
}

compile_gl() {
    local src="$1"
    local dst="$2"
    local libs="${3:--lglut -lGLU -lGL -lm}"

    if [ -f "$src" ]; then
        mkdir -p "$(dirname "$dst")"
        if gcc -o "$dst" "$src" $libs -Wno-format-truncation -Wno-format-extra-args; then
            if [ -f "$dst" ]; then
                echo "  ✓ $dst"
                ((COMPILED++))
            else
                echo "  ✗ FAILED (No binary): $src"
                ((FAILED++))
            fi
        else
            echo "  ✗ FAILED: $src"
            ((FAILED++))
        fi
    fi
}

echo "=== 1. SYSTEM COMPONENTS ==="
compile_op "pieces/keyboard/src/keyboard_input_linux.c" "pieces/keyboard/plugins/+x/keyboard_input.+x"
compile_op "pieces/joystick/plugins/joystick_input.c" "pieces/joystick/plugins/+x/joystick_input.+x"
compile_op "pieces/chtpm/plugins/chtpm_parser.c" "pieces/chtpm/plugins/+x/chtpm_parser.+x"
compile_op "pieces/chtpm/plugins/orchestrator.c" "pieces/chtpm/plugins/+x/orchestrator.+x"
compile_op "pieces/display/renderer.c" "pieces/display/plugins/+x/renderer.+x"
compile_gl "pieces/display/gl_renderer.c" "pieces/display/plugins/+x/gl_renderer.+x" "-lglut -lGLU -lGL -I/usr/include/freetype2 -lfreetype"
compile_op "pieces/system/pdl/pdl_reader.c" "pieces/system/pdl/+x/pdl_reader.+x"

# Process management (proc_manager CLI tool)
compile_op "pieces/os/plugins/proc_manager.c" "pieces/os/plugins/+x/proc_manager.+x"
echo ""

echo "=== 1b. WINDOWS-SPECIFIC COMPONENTS (if on Windows) ==="
# These are compiled separately on Windows via compile_all.ps1
# Included here for documentation and MSYS2/MinGW cross-compilation
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    echo "  Windows detected - use pieces/buttons/windows/legacy/compile_all.ps1 instead"
    echo "  Compiles: keyboard_input_win.c, joystick_input_win.c (XInput), windows_renderer.c"
fi
echo ""

echo "=== 2. SYSTEM APPS ==="
# op-ed
compile_op "pieces/apps/op-ed/plugins/op-ed_module.c" "pieces/apps/op-ed/plugins/+x/op-ed_module.+x"

# player_app
compile_op "pieces/apps/player_app/manager/player_manager.c" "pieces/apps/player_app/manager/plugins/+x/player_manager.+x"

# player_app world plugins (modern loader, etc.)
mkdir -p pieces/apps/player_app/world/plugins/+x
for src in pieces/apps/player_app/world/plugins/*.c; do
    if [ -f "$src" ]; then
        op_name=$(basename "$src" .c)
        compile_op "$src" "pieces/apps/player_app/world/plugins/+x/${op_name}.+x"
    fi
done
echo ""

echo "=== 2b. GL-OS APP (OpenGL) ==="
compile_gl "pieces/apps/gl_os/plugins/gl_desktop.c" "pieces/apps/gl_os/plugins/+x/gl_desktop.+x"
compile_gl "pieces/apps/gl_os/plugins/gl_os_renderer.c" "pieces/apps/gl_os/plugins/+x/gl_os_renderer.+x"
compile_op "pieces/apps/gl_os/plugins/gl_os_session.c" "pieces/apps/gl_os/plugins/+x/gl_os_session.+x"
compile_op "pieces/apps/gl_os/plugins/gl_os_loader.c" "pieces/apps/gl_os/plugins/+x/gl_os_loader.+x"
compile_op "pieces/apps/gl_os/plugins/gl_os.c" "pieces/apps/gl_os/plugins/+x/gl_os.+x"
echo ""

echo "=== 3. PLAYRM OPS (SHARED) ==="
mkdir -p pieces/apps/playrm/ops/+x
for src in pieces/apps/playrm/ops/src/*.c; do
    if [ -f "$src" ]; then
        op_name=$(basename "$src" .c)
        compile_op "$src" "pieces/apps/playrm/ops/+x/${op_name}.+x"
    fi
done
echo ""

echo "=== 3b. PLAYRM PLUGINS ==="
mkdir -p pieces/apps/playrm/plugins/+x
compile_op "pieces/apps/playrm/plugins/playrm_module.c" "pieces/apps/playrm/plugins/+x/playrm_module.+x"
echo ""

echo "=== 3c. PLAYRM LOADER (Project Scanner Daemon) ==="
mkdir -p pieces/apps/playrm/loader/plugins/+x
compile_op "pieces/apps/playrm/loader/loader_module.c" "pieces/apps/playrm/loader/plugins/+x/loader_module.+x"
echo ""

echo "=== 4. PROJECT MANAGERS ==="
for proj_dir in projects/*/; do
    proj_name=$(basename "$proj_dir")

    # Skip trunk and non-project dirs
    if [ "$proj_name" = "trunk" ]; then continue; fi

    # Check for manager source
    if [ -f "${proj_dir}manager/${proj_name}_manager.c" ]; then
        if [ "$proj_name" = "user" ]; then
            # user_manager requires libcrypto for password hashing
            mkdir -p "${proj_dir}manager/+x"
            if gcc -o "${proj_dir}manager/+x/${proj_name}_manager.+x" "${proj_dir}manager/${proj_name}_manager.c" -pthread -lcrypto -Wno-format-truncation -Wno-format-extra-args 2>/dev/null; then
                echo "  ✓ ${proj_dir}manager/+x/${proj_name}_manager.+x"
                ((COMPILED++))
            else
                echo "  ✗ FAILED: ${proj_dir}manager/${proj_name}_manager.c"
                ((FAILED++))
            fi
        else
            compile_op "${proj_dir}manager/${proj_name}_manager.c" "${proj_dir}manager/+x/${proj_name}_manager.+x"
        fi
    fi

    # Special case: pal_editor_module
    if [ "$proj_name" = "op-ed" ] && [ -f "${proj_dir}manager/pal_editor_module.c" ]; then
        compile_op "${proj_dir}manager/pal_editor_module.c" "${proj_dir}manager/+x/pal_editor_module.+x"
    fi

    # Special case: gl-os uses underscore instead of hyphen
    if [ "$proj_name" = "gl-os" ] && [ -f "${proj_dir}manager/gl_os_manager.c" ]; then
        compile_op "${proj_dir}manager/gl_os_manager.c" "${proj_dir}manager/+x/gl_os_manager.+x"
    fi

    # GL-Op-Ed Manager
    if [ "$proj_name" = "op-ed-gl" ] && [ -f "${proj_dir}manager/op-ed-gl_manager.c" ]; then
        compile_op "${proj_dir}manager/op-ed-gl_manager.c" "${proj_dir}manager/+x/op-ed-gl_manager.+x"
    fi
done
echo ""

echo "=== 5. PROJECT OPS ==="
for proj_dir in projects/*/; do
    proj_name=$(basename "$proj_dir")
    
    # Skip trunk
    if [ "$proj_name" = "trunk" ]; then continue; fi
    
    # Check for ops directory (both root and src/ subdirectory)
    for op_subdir in "ops" "ops/src"; do
        if [ -d "${proj_dir}${op_subdir}/" ]; then
            for src in "${proj_dir}${op_subdir}/"*.c; do
                if [ -f "$src" ]; then
                    op_name=$(basename "$src" .c)
                    mkdir -p "${proj_dir}ops/+x"
                    compile_op "$src" "${proj_dir}ops/+x/${op_name}.+x"
                fi
            done
        fi
    done
done
echo ""

echo "=== 6. LEGACY APPS (TRUNK) ==="
if [ -d "projects/trunk/legacy_archive/" ]; then
    for app_dir in projects/trunk/legacy_archive/*/; do
        app_name=$(basename "$app_dir")

        # Compile managers
        if [ -f "${app_dir}manager/${app_name}_manager.c" ]; then
            compile_op "${app_dir}manager/${app_name}_manager.c" "${app_dir}manager/+x/${app_name}_manager.+x"
        fi

        # Compile ops/traits
        for subdir in ops traits; do
            if [ -d "${app_dir}${subdir}/" ]; then
                for src in "${app_dir}${subdir}/"*.c; do
                    if [ -f "$src" ]; then
                        op_name=$(basename "$src" .c)
                        compile_op "$src" "${app_dir}${subdir}/+x/${op_name}.+x"
                    fi
                done
            fi
        done
    done
fi
echo ""

echo "=== 7. SHARED APP TRAITS (FUZZPET_APP) ==="
# zombie_ai - Zombie chase AI for fuzz-op project
compile_op "pieces/apps/fuzzpet_app/traits/zombie_ai.c" "pieces/apps/fuzzpet_app/traits/+x/zombie_ai.+x"
echo ""

echo "=== COMPILE SUMMARY ==="
echo "  Compiled: $COMPILED binaries"
echo "  Failed:   $FAILED binaries"
echo ""

if [ $FAILED -gt 0 ]; then
    echo "⚠ WARNING: $FAILED binaries failed to compile!"
    echo "Check error messages above."
    exit 1
else
    echo "✓ All binaries compiled successfully!"
    exit 0
fi
