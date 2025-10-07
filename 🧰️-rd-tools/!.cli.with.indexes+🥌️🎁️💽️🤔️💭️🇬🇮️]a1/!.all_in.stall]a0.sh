#!/bin/bash

# Check if exactly two arguments are provided
if [ $# -ne 2 ]; then
    echo "Error: Usage is $0 <binary_path> <command_name> 😕"
    echo "Example: $0 ./+/nav.+x test"
    exit 1
fi

BINARY_PATH="$1"
COMMAND_NAME="$2"

# Check if binary exists
if [ ! -f "$BINARY_PATH" ]; then
    echo "Error: Binary not found at $BINARY_PATH 😢"
    exit 1
fi

# Check if binary is executable
if [ ! -x "$BINARY_PATH" ]; then
    echo "Warning: Binary is not executable. Making it executable... 🛠️"
    chmod +x "$BINARY_PATH"
fi

# Create /usr/+x/ if it doesn't exist
echo "Setting up /usr/+x/... 📁"
sudo mkdir -p /usr/+x
sudo chmod 755 /usr/+x

# Copy binary to /usr/+x/ with command name
echo "Installing $BINARY_PATH as $COMMAND_NAME to /usr/+x/... 🚀"
sudo cp "$BINARY_PATH" "/usr/+x/$COMMAND_NAME"
sudo chmod +x "/usr/+x/$COMMAND_NAME"

# Update PATH in ~/.bashrc
if ! grep -q "/usr/+x" ~/.bashrc; then
    echo "Adding /usr/+x to PATH in ~/.bashrc... 🌟"
    echo 'export PATH=$PATH:/usr/+x' >> ~/.bashrc
fi

echo "Installation complete! 🎉"
echo "Run 'source ~/.bashrc' to update your PATH."
echo "Then type '$COMMAND_NAME' to run your binary! ⚡"
