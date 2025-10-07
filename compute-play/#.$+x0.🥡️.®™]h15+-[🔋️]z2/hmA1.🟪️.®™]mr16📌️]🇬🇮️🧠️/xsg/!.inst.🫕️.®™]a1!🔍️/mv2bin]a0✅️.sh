#!/bin/sh
# mv2bin.sh - Moves binary to /bin/ and logs to ~/.mv2bin_history.txt 🪄

# Check if exactly one argument is provided
if [ $# -ne 1 ]; then
    echo "❌ Error: One argument required! Usage: $0 <binary-path>"
    echo "Example: $0 \\!.inst.🫕️.®™/+x/hi.+x"
    exit 1
fi

# Get argument
binary_path="$1"  # e.g., \!.inst.🫕️.®™/+x/hi.+x

# Combine with $PWD
full_binary_path="$PWD/$binary_path"

# Check if binary exists
if [ ! -f "$full_binary_path" ]; then
    echo "❌ Error: Binary '$full_binary_path' does not exist!"
    exit 1
fi

# Ensure binary is executable
chmod +x "$full_binary_path"
if [ $? -ne 0 ]; then
    echo "❌ Error: Failed to make '$full_binary_path' executable!"
    exit 1
fi

# Get binary name for /bin/
binary_name=$(basename "$binary_path")  # e.g., hi.+x
dest_path="/bin/$binary_name"  # e.g., /bin/hi.+x

# Move binary to /bin/ (requires sudo)
sudo mv "$full_binary_path" "$dest_path"
if [ $? -eq 0 ]; then
    echo "✨ Moved '$full_binary_path' to '$dest_path'."
else
    echo "❌ Error: Failed to move '$full_binary_path' to '$dest_path'. Check sudo permissions."
    exit 1
fi

# Log to ~/.mv2bin_history.txt
history_file="$HOME/.mv2bin_history.txt"
echo "$dest_path" >> "$history_file"
if [ $? -eq 0 ]; then
    echo "✨ Logged '$dest_path' to '$history_file'."
else
    echo "❌ Error: Failed to log to '$history_file'."
    exit 1
fi

echo "🎉 Done! Test with '$binary_name' or check history with 'cat $history_file'."
echo "🧙‍♂️ Run '$binary_name' to verify it works from /bin/."
