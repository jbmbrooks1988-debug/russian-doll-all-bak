#!/bin/bash
PROJECT_ROOT="moke-pet-project-2.0"
WORLD="world_tank_01"
MAP="map_enclosure"
PETS=("liz_bulb" "liz_char")

for PET in "${PETS[@]}"; do
    echo "Compiling ops for $PET..."
    OPS_DIR="$PROJECT_ROOT/pieces/$WORLD/$MAP/$PET/ops"
    mkdir -p "$OPS_DIR/+x"
    for src in "$OPS_DIR"/*.c; do
        [ -f "$src" ] || continue
        basename=$(basename "$src" .c)
        gcc -o "$OPS_DIR/+x/$basename.+x" "$src" && echo "  ✓ $basename compiled"
    done
done
