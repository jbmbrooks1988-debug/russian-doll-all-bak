#!/bin/sh
# clean_mv2bin.sh - Removes binaries from /bin/ listed in ~/.mv2bin_history.txt and clears history 🪄

# Define history file
history_file="$HOME/.mv2bin_history.txt"

# Check if history file exists
if [ ! -f "$history_file" ]; then
    echo "❌ Error: History file '$history_file' does not exist!"
    exit 1
fi

# Check if history file is empty
if [ ! -s "$history_file" ]; then
    echo "⚠️ Warning: History file '$history_file' is empty. Nothing to clean."
    exit 0
fi

# Read each binary path from history file
error_occurred=0
while read -r binary; do
    # Skip empty lines
    if [ -z "$binary" ]; then
        continue
    fi

    # Check if binary exists in /bin/
    if [ -f "$binary" ]; then
        sudo rm "$binary"
        if [ $? -eq 0 ]; then
            echo "✨ Removed '$binary' from /bin/."
        else
            echo "❌ Error: Failed to remove '$binary'. Check sudo permissions."
            error_occurred=1
        fi
    else
        echo "⚠️ Warning: '$binary' not found in /bin/."
    fi
done < "$history_file"

# Clear history file only if no errors occurred
if [ $error_occurred -eq 0 ]; then
    : > "$history_file"
    if [ $? -eq 0 ]; then
        echo "✨ Cleared '$history_file'."
    else
        echo "❌ Error: Failed to clear '$history_file'."
        exit 1
    fi
else
    echo "❌ Errors occurred during cleanup. History file '$history_file' not cleared to preserve records."
    exit 1
fi

echo "🎉 Cleanup complete! Verify with 'cat $history_file' or 'ls /bin/'."
echo "🧙‍♂️ Binaries removed and history cleared. Run your next spell!"
