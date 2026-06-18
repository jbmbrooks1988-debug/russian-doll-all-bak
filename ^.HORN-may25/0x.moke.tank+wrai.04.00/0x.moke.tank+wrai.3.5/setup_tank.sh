#!/bin/bash

# Moke-Pet Environment Setup Script
# Usage: ./setup_tank.sh [pet_name_1] [pet_name_2] ...

PROJECT="moke-pet-project-2.0"
WORLD="world_tank_01"
MAP="map_enclosure"
PETS=("$@")

if [ ${#PETS[@]} -eq 0 ]; then
    echo "No pets specified. Defaulting to: liz_bulb liz_char"
    PETS=("liz_bulb" "liz_char")
fi

echo "Setting up $PROJECT..."

# Shared Resources and Logs
mkdir -p "$PROJECT/pieces/$WORLD/$MAP/food_pool"
mkdir -p "$PROJECT/pieces/$WORLD/$MAP/rock_pool"
mkdir -p "$PROJECT/pieces/$WORLD/logs"

# Sovereign Entities
for PET in "${PETS[@]}"; do
    echo "Creating pet: $PET"
    mkdir -p "$PROJECT/pieces/$WORLD/$MAP/$PET/stomach"
    mkdir -p "$PROJECT/pieces/$WORLD/$MAP/$PET/ops"
    mkdir -p "$PROJECT/pieces/$WORLD/$MAP/$PET/memory"
    # Placeholder PDL
    echo "METHOD | eat | pieces/$WORLD/$MAP/$PET/ops/eat.+x" > "$PROJECT/pieces/$WORLD/$MAP/$PET/piece.pdl"
done

echo "Setup complete."
