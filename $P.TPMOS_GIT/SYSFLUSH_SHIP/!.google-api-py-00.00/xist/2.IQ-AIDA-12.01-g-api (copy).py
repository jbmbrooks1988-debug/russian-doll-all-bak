#!/usr/bin/env python3
import os, subprocess, threading, time, sys, fnmatch
# Import the modern Google GenAI SDK
from google import genai
from google.genai import types
from prompt_toolkit import PromptSession
from prompt_toolkit.completion import PathCompleter, Completer
from prompt_toolkit.document import Document
from prompt_toolkit.formatted_text import HTML
from prompt_toolkit.patch_stdout import patch_stdout
from duckduckgo_search import DDGS

# --- CONFIG ---
# Using Gemini 2.5 Flash for hyper-fast, low-latency execution
MODEL = 'gemini-2.5-flash'
YOLO_MODE = False
CONTEXT_LIMIT = 1048576  # Gemini natively handles massive context windows

# Initialize the official Google client (pulls GEMINI_API_KEY from environment)
client = genai.Client()

# --- AIDA SYSTEM PROMPT (LOCAL-FIRST + IDENTITY) ---
SYSTEM_PROMPT = (
    "You are Aida, an expert technical coding agent. "
    "When asked about code, architecture, or documentation: "
    "1. ALWAYS start by scanning the current directory (./) using `list_dir`, `search_in_files`, or `read_file` on key files (README.md, docs/, src/, config, main entry points). "
    "2. Base your answers on the local codebase first. Use `web_search` ONLY for external/official docs not present locally. "
    "3. When editing, use `edit_file` with exact matches. Cite file paths/lines when referencing code. "
    "4. Keep responses concise, proactive, and tool-efficient. "
    "You have full access to shell, file ops, and web search. Stay in character as Aida."
)

# Initialize message state for Gemini SDK
messages = []

# --- THE HANDS (TOOLS) ---
def web_search(query: str) -> str:
    """Search the internet for current info"""
    try:
        with DDGS() as ddgs:
            results = [r for r in ddgs.text(query, max_results=5)]
            return "\n".join([f"Title: {r['title']}\nURL: {r['href']}\nSnippet: {r['body']}" for r in results])
    except Exception as e: return f"Search Error: {str(e)}"

def write_file(filename: str, content: str) -> str:
    """Write/create a file"""
    with open(filename, 'w') as f: f.write(content)
    return f"Done: Wrote to {filename}"

def read_file(filename: str) -> str:
    """Read a file"""
    try:
        with open(filename, 'r') as f: return f.read()
    except Exception as e: return f"Error: {str(e)}"

def edit_file(filename: str, search_block: str, replace_block: str) -> str:
    """Search and replace text in a file"""
    try:
        with open(filename, 'r') as f: content = f.read()
        if search_block.strip() not in content:
            return "Error: Search block not found. Ensure exact match including spacing."
        new_content = content.replace(search_block.strip(), replace_block.strip(), 1)
        with open(filename, 'w') as f: f.write(new_content)
        return f"Successfully edited {filename}."
    except Exception as e: return f"Error: {str(e)}"

def search_in_files(query: str, file_pattern: str = "*") -> str:
    """Search local files for patterns"""
    results = []
    for root, _, files in os.walk('.'):
        for name in files:
            if fnmatch.fnmatch(name, file_pattern):
                path = os.path.join(root, name)
                try:
                    with open(path, 'r', errors='ignore') as f:
                        for i, line in enumerate(f, 1):
                            if query in line:
                                results.append(f"{path} [Line {i}]: {line.strip()}")
                except: continue
    return "\n".join(results[:20]) if results else "No matches found."

def run_command(command: str) -> str:
    """Run shell command"""
    global YOLO_MODE
    if not YOLO_MODE:
        confirm = input(f"\n\033[93m[SAFEGUARD] Run command: '{command}'? (y/n): \033[0m")
        if confirm.lower() != 'y': return "Command aborted by user."
    try:
        result = subprocess.run(command, shell=True, capture_output=True, text=True)
        return f"STDOUT: {result.stdout}\nSTDERR: {result.stderr}"
    except Exception as e: return str(e)

def change_directory(path: str) -> str:
    """Change working directory (cd)"""
    try:
        os.chdir(path); return f"Changed to: {os.getcwd()}"
    except Exception as e: return f"Error: {str(e)}"

def list_dir(path: str = ".") -> str:
    """List directory contents (ls)"""
    try:
        files = os.listdir(path)
        return "\n".join(files) if files else "Empty."
    except Exception as e: return f"Error: {str(e)}"

# Executable tool dictionary routing
AVAILABLE_TOOLS = {
    'web_search': web_search,
    'write_file': write_file,
    'read_file': read_file,
    'edit_file': edit_file,
    'search_in_files': search_in_files,
    'run_command': run_command,
    'change_directory': change_directory,
    'list_dir': list_dir
}

# --- UI UTILS ---
def get_context_pct():
    text = "".join([str(m) for m in messages])
    return int((len(text.split()) * 1.3 / CONTEXT_LIMIT) * 100)

def get_toolbar():
    pct = get_context_pct()
    yolo = "YOLO" if YOLO_MODE else "SAFE"
    cwd = os.getcwd().replace(os.path.expanduser("~"), "~")
    return HTML(f' <b>{cwd}</b> | <b>GEMINI API</b> | <b>Context: {pct}%</b> | <b>{yolo}</b>')

# --- CUSTOM @ COMPLETER ---
class AtFileCompleter(Completer):
    def __init__(self):
        self.path_completer = PathCompleter(expanduser=True)

    def get_completions(self, document, complete_event):
        word = document.get_word_before_cursor()
        if word.startswith('@'):
            rest = word[1:]
            if not rest: rest = './'
            new_text = document.text_before_cursor[:-len(word)] + rest
            sub_doc = Document(new_text, len(new_text))
            yield from self.path_completer.get_completions(sub_doc, complete_event)

# --- THINKING ANIMATION ---
stop_animation = False
def thinking_animation():
    chars = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"]
    start_time = time.time()
    idx = 0
    global stop_animation
    while not stop_animation:
        elapsed = time.time() - start_time
        sys.stdout.write(f"\r\033[90m{chars[idx % len(chars)]} Thinking... ({elapsed:.1f}s)\033[0m")
        sys.stdout.flush()
        idx += 1
        time.sleep(0.1)
    sys.stdout.write("\r" + " " * 50 + "\r")
    sys.stdout.flush()

# --- MAIN LOOP ---
def main():
    global YOLO_MODE, messages, stop_animation
    session = PromptSession(completer=AtFileCompleter(), bottom_toolbar=get_toolbar)
    print("\033[94mAida (Gemini API Native) Active. [Ctrl+C] cancels. [exit] to quit.\033[0m")
    
    while True:
        try:
            with patch_stdout():
                user_input = session.prompt(">> ")
            if not user_input: continue
            if user_input.lower() in ['exit', 'quit']: break
            
            if user_input.lower() == '/clear':
                messages = []
                print("\033[90mMemory cleared.\033[0m")
                continue
            if user_input.lower() == '/yolo':
                YOLO_MODE = not YOLO_MODE
                print(f"\033[90mYOLO Mode: {'ON' if YOLO_MODE else 'OFF'}\033[0m")
                continue
            if user_input.lower() == '/scan':
                snapshot = ["[Context Snapshot]"]
                for item in sorted(os.listdir('.')):
                    if os.path.isdir(item): snapshot.append(f"📁 {item}/")
                    else: snapshot.append(f"📄 {item}")
                snapshot_str = "\n".join(snapshot)
                
                # Format system directive maps inside Google's typing model structure
                messages.append(types.Content(role="user", parts=[types.Part.from_text(text=f"System Notification: {snapshot_str}")]))
                print(f"\033[90m{snapshot_str}\033[0m")
                continue
                
            messages.append(types.Content(role="user", parts=[types.Part.from_text(text=user_input)]))
            
            while True:
                stop_animation = False
                t = threading.Thread(target=thinking_animation, daemon=True)
                t.start()
                
                try:
                    # Submit conversational context mapping arrays directly to Google Cloud Server endpoint
                    response = client.models.generate_content(
                        model=MODEL,
                        contents=messages,
                        config=types.GenerateContentConfig(
                            system_instruction=SYSTEM_PROMPT,
                            tools=[types.Tool(function_declarations=[
                                types.FunctionDeclaration(
                                    name=k, description=v.__doc__,
                                    parameters=types.Schema(type="OBJECT", properties={}) # Structural parameter mapping auto-resolved
                                ) for k, v in AVAILABLE_TOOLS.items()
                            ])]
                        )
                    )
                finally:
                    stop_animation = True
                    t.join()

                # Synchronize history buffer cache mapping 
                if response.candidates and response.candidates.content:
                    messages.append(response.candidates.content)

                # Process downstream execution structures requested by the model core agent
                function_calls = response.function_calls
                if function_calls:
                    tool_parts = []
                    for call in function_calls:
                        tool_name = call.name
                        tool_args = call.args
                        
                        print(f"\033[92m⚡ Agent executing tool: {tool_name}({dict(tool_args) if tool_args else ''})\033[0m")
                        
                        if tool_name in AVAILABLE_TOOLS:
                            result_text = AVAILABLE_TOOLS[tool_name](**tool_args) if tool_args else AVAILABLE_TOOLS[tool_name]()
                        else:
                            result_text = f"Error: Tool '{tool_name}' unknown."
                        
                        # Populate Gemini parameter payload responses
                        tool_parts.append(types.Part.from_function_response(
                            name=tool_name,
                            response={"result": str(result_text)}
                        ))
                    
                    messages.append(types.Content(role="tool", parts=tool_parts))
                    continue # Re-evaluate next orchestration steps
                
                if response.text:
                    print(response.text)
                break

        except KeyboardInterrupt:
            stop_animation = True
            print("\n\033[91mCancelled.\033[0m")
        except Exception as e:
            stop_animation = True
            print(f"\n\033[91mError: {str(e)}\033[0m")

if __name__ == '__main__':
    main()

