Here's a comprehensive README.md for your Grok-TUI tool:

```markdown
# Grok-TUI 

A powerful terminal-based AI coding agent powered by Ollama and Groq's LLaMA3 model with tool-use capabilities.

## Features ✨

- **Interactive Terminal UI** - Clean, responsive interface with real-time feedback
- **8 Built-in Tools** - File operations, web search, code execution, and more
- **Smart File Completion** - Type `@` + `Tab` for instant file path completion
- **Context Awareness** - Live token quota tracking to manage conversation limits
- **Safety First** - Command execution safeguard with YOLO mode option
- **Thinking Animation** - Visual feedback while AI processes your requests
- **Ctrl+C Support** - Cancel any request instantly without crashing

## Installation 📦

### Prerequisites
- Python 3.8+
- Ollama installed and running

### Setup

1. **Install dependencies:**
```bash
pip install ollama prompt_toolkit duckduckgo_search
```

2. **Pull the model:**
```bash
ollama pull llama3-groq-tool-use
```

3. **Run the script:**
```bash
python3 1.IQ-ROQ-10.04.py
```

## Usage 🚀

### Basic Commands

| Command | Description |
|---------|-------------|
| `/clear` | Clear conversation memory |
| `/yolo` | Toggle YOLO mode (skip command confirmations) |
| `exit` or `quit` | Exit the application |
| `@<Tab>` | Trigger file path completion |

### Interactive Features

**File Path Completion:**
```
>> Read @<Tab>           # Shows all files in current directory
>> Edit ./src/@<Tab>     # Navigate into subdirectories
```

**Context Quota:**
The bottom toolbar shows:
- Current working directory
- Active model name
- Context usage percentage (green < 75%, red ≥ 75%)
- YOLO/SAFE mode status

## Available Tools 🛠️

The AI can automatically use these tools:

### 1. **web_search**
Search the internet for current information.
```
>> "What's new in Python 3.12?"
```

### 2. **read_file**
Read contents of a file.
```
>> "Read config.json"
```

### 3. **write_file**
Create or overwrite a file.
```
>> "Create a file called test.py with a hello world script"
```

### 4. **edit_file**
Search and replace text in existing files.
```
>> "In main.py, replace 'print(hello)' with 'print(world)'"
```

### 5. **search_in_files**
Grep-like search across your codebase.
```
>> "Find all occurrences of 'TODO' in Python files"
```

### 6. **run_command**
Execute shell commands (requires confirmation unless YOLO mode).
```
>> "List all files in the current directory"
>> "Run git status"
```

### 7. **change_directory**
Change the working directory.
```
>> "cd into the src folder"
```

### 8. **list_dir**
List directory contents.
```
>> "What's in the current directory?"
```

## Configuration ⚙️

Edit these variables at the top of the script:

```python
MODEL = 'llama3-groq-tool-use'  # Ollama model to use
YOLO_MODE = False               # Skip command confirmations
CONTEXT_LIMIT = 8192            # Token limit for context window
```

## Safety Features 🛡️

### YOLO Mode
- **SAFE (default)**: Prompts for confirmation before executing shell commands
- **YOLO**: Executes commands immediately without prompts

Toggle with `/yolo` command.

### Safeguards
- All file operations are logged
- Command output captured (stdout/stderr)
- Context quota warnings prevent token overflow
- Graceful Ctrl+C handling prevents corruption

## Examples 💡

### Example 1: Create a Python Project
```
>> "Create a new Flask app with a basic API endpoint"
[Action: write_file]
[Action: write_file]
Done! Your Flask app is ready.
```

### Example 2: Debug Code
```
>> "Search for all 'print' statements in .py files"
[Action: search_in_files]
main.py [Line 15]: print("debug")
utils.py [Line 42]: print(error)

>> "Remove those debug prints"
[Action: edit_file]
[Action: edit_file]
```

### Example 3: Web Research + File Creation
```
>> "Research FastAPI best practices and create a guide.md"
[Action: web_search]
[Action: write_file]
Guide created with latest best practices!
```

## Troubleshooting 🔧

**"Ollama unreachable"**
- Ensure Ollama is running: `ollama list`
- Check model is downloaded: `ollama pull llama3-groq-tool-use`

**"Context quota exceeded"**
- Use `/clear` to reset conversation memory
- Increase `CONTEXT_LIMIT` in config

**Ctrl+C not working**
- Should cancel instantly; if frozen, use `Ctrl+\` (SIGQUIT)

## Performance Tips ⚡

1. **Use `/clear` regularly** to keep context fresh
2. **Enable YOLO mode** for rapid iteration (use with caution!)
3. **Be specific** with file paths using `@` completion
4. **Batch operations** - ask AI to perform multiple edits in one request

## Architecture 🏗️

```
┌─────────────────────────────────────────┐
│           Prompt Toolkit UI             │
│  (Input, Toolbar, @ Completion)         │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│         Main Loop (Threading)           │
│  - Animation Thread                     │
│  - Ollama API Call                      │
│  - Tool Execution                       │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│         Tool Handlers                   │
│  • web_search  • write_file             │
│  • read_file   • edit_file              │
│  • search_in_files • run_command        │
│  • change_directory • list_dir          │
└─────────────────────────────────────────┘
```

## Credits 👏

- **Model**: LLaMA3 with Groq tool-use via Ollama
- **Search**: DuckDuckGo Search API
- **UI**: Prompt Toolkit
- **Inspired by**: Interactive CLI coding assistants

## License 📄

MIT License - Feel free to modify and distribute!

---

**Happy Coding! 🎉**
```

This README covers all the features, tools, safety mechanisms, and usage patterns. You can save it as `README.md` in the same directory as your script!