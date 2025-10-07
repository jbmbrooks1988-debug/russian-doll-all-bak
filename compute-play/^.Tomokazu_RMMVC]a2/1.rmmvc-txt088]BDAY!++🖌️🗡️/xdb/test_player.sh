#!/bin/bash
# Test script for the player program

echo "Testing player program..."

# Create a test project directory
mkdir -p test_project/data
mkdir -p test_project/build

# Create a simple test map file
echo "x,y,z,emoji_idx,fg_color_idx,bg_color_idx" > test_project/data/map_0.txt
echo "5,5,0,0,0,7" >> test_project/data/map_0.txt
echo "6,5,0,1,0,7" >> test_project/data/map_0.txt
echo "7,5,0,2,0,7" >> test_project/data/map_0.txt

# Create a simple test event file
echo "events[1].id,1" > test_project/data/map_0_events.txt
echo "events[1].name,test-event-1" >> test_project/data/map_0_events.txt
echo "events[1].note," >> test_project/data/map_0_events.txt
echo "events[1].pages[0].list[1].parameters[0],default map1 test" >> test_project/data/map_0_events.txt
echo "events[1].x,4" >> test_project/data/map_0_events.txt
echo "events[1].y,3" >> test_project/data/map_0_events.txt
echo "events[1].z,9" >> test_project/data/map_0_events.txt

# Copy test project to player data directory
cp -r test_project/data/* player/data/

# Build the player
cd player && ./build.sh

# Check if build was successful
if [ -f "player_main" ]; then
    echo "Player built successfully!"
    echo "To run the player, use: ./player_main test_project"
else
    echo "Failed to build player!"
fi