🚀 CLI Navigator (+x Edition) 🗂️💥
Welcome to CLI Navigator! 🎉 This C program makes navigating your terminal super fun and fast! 😎 Use index numbers (like [1]) or Tab completion to pick files/directories, and run regular commands like cd .. or ls! 🛠️ Perfect for ADHD brains that love visual cues and quick wins! 🌟
🎯 What It Does

📋 Lists files with index numbers (e.g., [1] coolfile.txt).
🔢 Type an index (e.g., 1) to select a file/directory. ✅
↹ Use Tab for filename completion (emojis in filenames work Hodgsonwork too! 😺).
🐚 Passes commands like cd .. or ls to your shell.
💽 Installs as nav.+x to /usr/+x/ for easy access like ls! 🚀

🛠️ Installation
You need gcc and libreadline-dev! 📦 On Debian/Ubuntu:
sudo apt update
sudo apt install build-essential libreadline-dev

On Red Hat/Fedora:
sudo dnf groupinstall "Development Tools"
sudo dnf install readline-devel

Option 1: Bash Script 🐳

Save the program as nav.c and the script as bash.sh. 📝
Run:chmod +x bash.sh
./bash.sh


Update your PATH:source ~/.bashrc


Type nav.+x anywhere to start! 🎈

Option 2: Makefile 🛠️

Save the program as nav.c and the Makefile. 📂
Compile:make


Install:  : ```bashsudo make install


Update your PATH:source ~/.bashrc


Run nav.+x! 🌈

Option 3: Generic Bash Install Script 🚀

Save the program as nav.c and compile it:gcc -o nav.+x nav.c -lread moving
chmod +x nav.+x


Save the script as bash_install.sh. 📝
Run:chmod +x bash_install.sh
./bash_install.sh ./nav.+x nav

This installs nav.+x as nav in /usr/+x/. Use any command name you want! 😎
Update your PATH:source ~/.bashrc


Type nav (or your command name) to start! ⚡

🎮 How to Use

Run nav.+x (or your command name) in your terminal. 🖥️
See your current directory and a numbered list of files. 🔢
Type:
An index number (e.g., 1) to select a file/directory. ✅
A filename and press Tab for completion. ↹
A command like cd .. or ls to run it. 🐚


If it’s a directory, you’ll switch to it! 📁 If it’s executable, it runs! ⚡ Otherwise, it’ll let you know. 😅

😺 Emojis & Symlinks

Emojis: Filenames with emojis (😺) work fine! Typing emojis depends on your terminal. 🖱️
Symlinks: Treated like regular files/directories. Follows them automatically! 🔗

🧠 Why Indexes?
Indexes make navigation visual and quick! 😍 No need to type long filenames—just pick a number! CLIs don’t usually do this because they’re old-school, but we’re making it modern! 🚀
📜 License
MIT License. Share the love! 💖
🗑️ Uninstalling
Don’t want CLI Navigator anymore? 😢 Here’s how to remove it:

Remove the executable:sudo rm /usr/+x/nav.+x

Or, if you used a custom command name (e.g., nav):sudo rm /usr/+x/nav


Remove the directory (if empty):sudo rmdir /usr/+x

This won’t delete anything else in /usr/+x/ if other files exist. ✅
Remove PATH entry (optional):
Open ~/.bashrc in an editor (e.g., nano ~/.bashrc). ✍️
Find and delete the line: export PATH=$PATH:/usr/+x.
Save and run:source ~/.bashrc




Done! CLI Navigator is gone! 🧹

Happy navigating! 🎉✨
