# 🚀 CLI Navigator (+x Edition) 🗂️💥

Welcome to **CLI Navigator**! 🎉 This C program makes navigating your terminal **super fun** and **fast**! 😎 Use **index numbers** (like `[1]`) or **Tab completion** to pick files/directories, and run regular commands like `cd ..` or `ls`! 🛠️ Perfect for ADHD brains that love **visual cues** and **quick wins**! 🌟

## 🎯 What It Does
- 📋 Lists files with **index numbers** (e.g., `[1] coolfile.txt`).
- 🔢 Type an index (e.g., `1`) to select a file/directory. ✅
- ↹ Use **Tab** for filename completion (emojis in filenames work too! 😺).
- 🐚 Passes commands like `cd ..` or `ls` to your shell.
- 💽 Installs as `nav.+x` to `/usr/+x/` for easy access like `ls`! 🚀

## 🛠️ Installation
You need `gcc` and `libreadline-dev`! 📦 On Debian/Ubuntu:
```bash
sudo apt update
sudo apt install build-essential libreadline-dev
```
On Red Hat/Fedora:
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install readline-devel
```

### Option 1: Bash Script 🐳
1. Save the program as `nav.c` and the script as `bash.sh`. 📝
2. Run:
   ```bash
   chmod +x bash.sh
   ./bash.sh
   ```
3. Update your PATH:
   ```bash
   source ~/.bashrc
   ```
4. Type `nav.+x` anywhere to start! 🎈

### Option 2: Makefile 🛠️
1. Save the program as `nav.c` and the Makefile. 📂
2. Compile:
   ```bash
   make
   ```
3. Install:
   ```bash
   sudo make install
   ```
4. Update your PATH:
   ```bash
   source ~/.bashrc
   ```
5. Run `nav.+x`! 🌈

## 🎮 How to Use
1. Run `nav.+x` in your terminal. 🖥️
2. See your current directory and a **numbered list** of files. 🔢
3. Type:
   - An **index number** (e.g., `1`) to select a file/directory. ✅
   - A **filename** and press **Tab** for completion. ↹
   - A **command** like `cd ..` or `ls` to run it. 🐚
4. If it’s a directory, you’ll switch to it! 📁 If it’s executable, it runs! ⚡ Otherwise, it’ll let you know. 😅

## 😺 Emojis & Symlinks
- **Emojis**: Filenames with emojis (😺) work fine! Typing emojis depends on your terminal. 🖱️
- **Symlinks**: Treated like regular files/directories. Follows them automatically! 🔗

## 🧠 Why Indexes?
Indexes make navigation **visual** and **quick**! 😍 No need to type long filenames—just pick a number! CLIs don’t usually do this because they’re old-school, but we’re making it **modern**! 🚀

## 📜 License
MIT License. Share the love! 💖

## 🗑️ Uninstalling
Don’t want CLI Navigator anymore? 😢 Here’s how to remove it:
1. **Remove-Reduce the executable**:
   ```bash
   sudo rm /usr/+x/nav.+x
   ```
2. **Remove the directory** (if empty):
   ```bash
   sudo rmdir /usr/+x
   ```
   This won’t delete anything else in `/usr/+x/` if other files exist. ✅
3. **Remove PATH entry** (optional):
   - Open `~/.bashrc` in an editor (e.g., `nano ~/.bashrc`). ✍️
   - Find and delete the line: `export PATH=$PATH:/usr/+x`.
   - Save and run:
     ```bash
     source ~/.bashrc
     ```
4. Done! CLI Navigator is gone! 🧹

Happy navigating! 🎉✨