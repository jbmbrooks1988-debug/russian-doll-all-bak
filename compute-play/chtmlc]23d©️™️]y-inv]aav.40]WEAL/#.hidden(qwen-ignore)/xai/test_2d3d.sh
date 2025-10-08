#!/bin/bash

# Test script for 2D/3D view switching functionality

echo "Building the application..."
make

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Build successful!"

# Set up the game module executable variable by modifying the main
# Since the main_prototype_0.c uses an external variable game_executable,
# we need to make sure the game module is run

echo "Setting up test files..."
# Create a default output.csv file if it doesn't exist
if [ ! -f "output.csv" ]; then
    echo "400,300" > output.csv
    echo "Created default output.csv with player position"
fi

echo "To test the 2D/3D functionality:"
echo "1. Run: ./main_prototype_0 test_2d3d.chtml"
echo "2. You should see buttons '2D View' and '3D View' at the top left"
echo "3. Click these buttons to switch between 2D and 3D views"
echo "4. The canvas should render differently based on the selected view mode"
echo "5. Arrow keys should still work to move the player (if game module is correctly integrated)"
echo ""
echo "To exit the application, press ESC key."

echo ""
echo "Creating a sample CHTML with player on platform and objects for the next phase..."