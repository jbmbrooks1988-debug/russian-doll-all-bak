#!/bin/bash

# Check for dependencies
if ! command -v gcc &> /dev/null; then
    echo "Error: GCC not found. Please install build-essential."
    exit 1
fi

if ! pkg-config --libs --cflags readline &> /dev/null; then
    echo "Error: libreadline-dev not found. Install it with:"
    echo "  sudo apt install libreadline-dev  # Debian/Ubuntu"
    echo "  sudo dnf install readline-devel  # Red Hat/Fedora"
    exit 1
fi

# Compile the program
echo "Compiling nav.c to nav.+x..."
gcc -o nav.+x nav.c -lreadline
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

# Create /usr/+x/ if it doesn't exist
echo "Setting up /usr/+x/..."
sudo mkdir -p /usr/+x
sudo chmod 755 /usr/+x

# Move and set permissions
echo "Installing nav.+x to /usr/+x/..."
sudo mv nav.+x /usr/+x/
sudo chmod +x /usr/+x/nav.+x

# Update PATH in ~/.bashrc
if ! grep -q "/usr/+x" ~/.bashrc; then
    echo "Adding /usr/+x to PATH in ~/.bashrc..."
    echo 'export PATH=$PATH:/usr/+x' >> ~/.bashrc
fi

echo "Installation complete! 🎉"
echo "Run 'source ~/.bashrc' to update your PATH."
echo "Then type 'nav.+x' to start navigating! 🚀"
