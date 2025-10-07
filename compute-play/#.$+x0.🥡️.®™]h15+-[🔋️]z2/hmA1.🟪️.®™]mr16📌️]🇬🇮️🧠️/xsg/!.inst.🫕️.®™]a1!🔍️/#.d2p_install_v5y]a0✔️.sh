#!/bin/sh
# d2p_install_v3.sh - Adds directory to PATH and sources xshrc.sh, mimicking apt/snap 🪄

# Check if exactly two arguments are provided
if [ $# -ne 2 ]; then
    echo "❌ Error: Two arguments required! Usage: $0 <xshrc.sh-path> <executable-dir>"
    echo "Example: $0 \\!.inst.🫕️.®™/xshrc.sh \\!.inst.🫕️.®™/+x/"
    exit 1
fi

# Get arguments
xshrc_path="$1"  # e.g., \!.inst.🫕️.®™/xshrc.sh
exec_dir="$2"    # e.g., \!.inst.🫕️.®™/+x/

# Combine with $PWD, ensuring proper quoting
full_xshrc_path="$PWD/$xshrc_path"
full_exec_path="$PWD/$exec_dir"

# Check if xshrc.sh exists
if [ ! -f "$full_xshrc_path" ]; then
    echo "❌ Error: File '$full_xshrc_path' does not exist!"
    exit 1
fi

# Check if executable directory exists
if [ ! -d "$full_exec_path" ]; then
    echo "❌ Error: Directory '$full_exec_path' does not exist!"
    exit 1
fi

# Update xshrc.sh with PATH
if [ ! -f "$full_xshrc_path" ]; then
    echo "#!/bin/sh" > "$full_xshrc_path"
    chmod +x "$full_xshrc_path"
fi
# Avoid duplicate PATH entries
if ! grep -F "export PATH=\"$full_exec_path:\$PATH\"" "$full_xshrc_path" >/dev/null; then
    echo "export PATH=\"$full_exec_path:\$PATH\"" >> "$full_xshrc_path"
    echo "echo \"✨ xshrc.sh loaded! PATH=\$PATH ✨\"" >> "$full_xshrc_path"
    echo "✨ Updated '$full_xshrc_path' with new PATH."
else
    echo "🌟 Path already in '$full_xshrc_path'. No changes made."
fi

# Create /etc/profile.d/d2p.sh to add PATH system-wide (requires sudo)
profile_script="/etc/profile.d/d2p.sh"
if [ -w /etc/profile.d ]; then
    if ! grep -F "export PATH=\"$full_exec_path:\$PATH\"" "$profile_script" >/dev/null 2>/dev/null; then
        echo "export PATH=\"$full_exec_path:\$PATH\"" | sudo tee -a "$profile_script" >/dev/null
        echo "✨ Added '$full_exec_path' to '$profile_script' (system-wide PATH)."
    else
        echo "🌟 Path already in '$profile_script'. No changes made."
    fi
else
    echo "⚠️ Warning: No write access to /etc/profile.d. Run with sudo to add system-wide PATH."
fi

# Source xshrc.sh in ~/.bashrc
if ! grep -F "source \"$full_xshrc_path\"" ~/.bashrc >/dev/null; then
    echo "source \"$full_xshrc_path\"" >> ~/.bashrc
    echo "✨ Added 'source \"$full_xshrc_path\"' to ~/.bashrc."
else
    echo "🌟 Source line already in ~/.bashrc. No changes made."
fi

# Try sourcing xshrc.sh (limited by subshell)
. "$full_xshrc_path"
if [ $? -eq 0 ]; then
    echo "✨ Attempted to source '$full_xshrc_path' (PATH updated in subshell: $PATH)."
else
    echo "❌ Failed to source '$full_xshrc_path'."
fi

echo "🔔 To apply PATH changes now, run: source ~/.bashrc"
echo "    Or source this script: source $0 \"$1\" \"$2\""
echo "🎉 Done! Test with 'hi.+x' or check PATH with 'echo \$PATH'."
echo "🧙‍♂️ New sessions will automatically include '$full_exec_path' via /etc/profile.d/d2p.sh."
