#!/bin/bash

# liz-init.sh - Initialize the Moke-Pet Tank environment (Flat Structure)
PROJECT="moke-pet-project-2.0"
WORLD="world_tank_01"
MAP="map_enclosure"
TANK_ROOT="$PROJECT/pieces/$WORLD/$MAP"

echo "Initializing Tank: $WORLD/$MAP"

# 1. Clean up map (but keep pets)
find "$TANK_ROOT" -maxdepth 1 -not -name "liz_*" -not -name "map_enclosure" -exec rm -rf {} +

# 2. Populate pellets (each is a piece)
for i in {1..3}; do
    PELLET="pellet_$i"
    mkdir -p "$TANK_ROOT/$PELLET"
    echo "type | food" > "$TANK_ROOT/$PELLET/state.txt"
    echo "name | Pellet $i" >> "$TANK_ROOT/$PELLET/state.txt"
    echo "Created food piece: $PELLET"
done

# 3. Populate rocks
for i in {1..2}; do
    ROCK="rock_$i"
    mkdir -p "$TANK_ROOT/$ROCK"
    echo "type | rock" > "$TANK_ROOT/$ROCK/state.txt"
    echo "name | Rock $i" >> "$TANK_ROOT/$ROCK/state.txt"
    echo "Created rock piece: $ROCK"
done

# 4. Initialize world state
echo "epoch=1" > "$PROJECT/pieces/$WORLD/state.txt"
echo "status=active" >> "$PROJECT/pieces/$WORLD/state.txt"

# 5. Reset pet stomachs and stats
for PET in "liz_bulb" "liz_char"; do
    rm -rf "$TANK_ROOT/$PET/stomach/"*
    echo "type | lizard" > "$TANK_ROOT/$PET/state.txt"
    echo "name | $PET" >> "$TANK_ROOT/$PET/state.txt"
    
    mkdir -p "$TANK_ROOT/$PET/memory"
    echo "hp=10" > "$TANK_ROOT/$PET/memory/stats.txt"
    echo "hunger=0" >> "$TANK_ROOT/$PET/memory/stats.txt"
    
    # Scaffold piece.pdl
    cat <<EOF > "$TANK_ROOT/$PET/piece.pdl"
METHOD | scan        | pieces/world_tank_01/map_enclosure/$PET/ops/scan.+x
METHOD | eat         | pieces/world_tank_01/map_enclosure/$PET/ops/eat.+x
METHOD | breathe     | pieces/world_tank_01/map_enclosure/$PET/ops/breathe.+x
METHOD | check_death | pieces/world_tank_01/map_enclosure/$PET/ops/check_death.+x
METHOD | train       | pieces/world_tank_01/map_enclosure/$PET/ops/train.+x
METHOD | attack      | pieces/world_tank_01/map_enclosure/$PET/ops/attack.+x
METHOD | mate        | pieces/world_tank_01/map_enclosure/$PET/ops/mate.+x
METHOD | rest        | pieces/world_tank_01/map_enclosure/$PET/ops/rest.+x
METHOD | nothing     | pieces/world_tank_01/map_enclosure/$PET/ops/nothing.+x
METHOD | release     | projects/xo-pet-v1/ops/+x/possess.+x xelector
EOF

    echo "Cleared stomach, initialized stats and piece.pdl for $PET"
done

echo "Initialization complete."
