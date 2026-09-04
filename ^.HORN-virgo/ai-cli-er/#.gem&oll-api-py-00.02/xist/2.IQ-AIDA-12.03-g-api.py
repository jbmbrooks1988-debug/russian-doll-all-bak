#!/usr/bin/env python3
import os, subprocess, threading, time, sys, fnmatch
import warnings
from google import genai
from google.genai import types
from google.api_core import exceptions as google_exceptions
from prompt_toolkit import PromptSession
from prompt_toolkit.completion import PathCompleter, Completer
from prompt_toolkit.key_binding import KeyBindings
from prompt_toolkit.document import Document
from prompt_toolkit.formatted_text import HTML
from prompt_toolkit.patch_stdout import patch_stdout
from duckduckgo_search import DDGS

# Suppress non-text streaming warnings from google.genai
warnings.filterwarnings("ignore", category=UserWarning, module="google.genai")

# --- CONFIG ---
MODEL = 'gemini-2.5-flash'
YOLO_MODE = False
CONTEXT_LIMIT = 1048576
client = genai.Client()

# Token Tracking Globals
REMAINING_TOKENS = "Unknown"
REMAINING_REQUESTS = "Unknown"
RESET_TOKENS = "N/A"

# --- AIDA SYSTEM PROMPT ---
SYSTEM_PROMPT = (
    "You are Aida, an expert technical coding agent. "
    "When asked about code, architecture, or documentation: "
    "1. ALWAYS start by scanning the current directory (./) using `list_dir`, `search_in_files`, or `read_file`. "
    "2. Base your answers on the local codebase first. "
    "3. When editing, use `edit_file` with exact matches. "
    "4. Keep responses concise, proactive, and tool-efficient."
)
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

# FEATURE 1: Explicit Command Permission Safeguard
def run_command(command: str) -> str:
    """Run shell command"""
    global YOLO_MODE
    if not YOLO_MODE:
        # Prompt the developer explicitly before executing a modified shell state
        print(f"\n\033[95m⚠️ [PERMISSION REQUEST] The agent wants to execute a system command.\033[0m")
        confirm = input(f"\033[93mCommand: '{command}'? (y/n): \033[0m")
        if confirm.lower() != 'y':
            return "Permission Denied: Command execution aborted by user."
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

AVAILABLE_TOOLS = {
    'web_search': web_search, 'write_file': write_file, 'read_file': read_file,
    'edit_file': edit_file, 'search_in_files': search_in_files, 'run_command': run_command,
    'change_directory': change_directory, 'list_dir': list_dir
}

# --- UI UTILS ---
kb = KeyBindings()

@kb.add('enter')
def _(event):
    """If the completion menu is open, select the current completion. Otherwise, submit."""
    if event.app.current_buffer.complete_state:
        event.app.current_buffer.complete_state = None
    else:
        event.app.current_buffer.validate_and_handle()

@kb.add(' ')
def _(event):
    """Select completion on space if menu is open."""
    if event.app.current_buffer.complete_state:
        event.app.current_buffer.complete_state = None
    event.app.current_buffer.insert_text(' ')

def get_context_pct():
    text = "".join([str(m) for m in messages])
    return int((len(text.split()) * 1.3 / CONTEXT_LIMIT) * 100)

def get_toolbar():
    pct = get_context_pct()
    yolo = "YOLO" if YOLO_MODE else "SAFE"
    cwd = os.getcwd().replace(os.path.expanduser("~"), "~")
    return HTML(f' <b>{cwd}</b> | <b>Tokens: {REMAINING_TOKENS}</b> | <b>Reqs: {REMAINING_REQUESTS}</b> | <b>Reset: {RESET_TOKENS}</b> | <b>Context: {pct}%</b> | <b>{yolo}</b>')

class AtFileCompleter(Completer):
    def __init__(self):
        self.path_completer = PathCompleter(expanduser=True)

    def get_completions(self, document, complete_event):
        word_before_cursor = document.get_word_before_cursor()

        if word_before_cursor.startswith('@'):
            path_part = word_before_cursor[1:]
            
            # The PathCompleter needs a document that *only* contains the path part
            # it's supposed to complete.
            dummy_document = Document(path_part, len(path_part))
            
            yield from self.path_completer.get_completions(dummy_document, complete_event)

# --- MAIN LOOP ---
def main():
    global YOLO_MODE, messages, REMAINING_TOKENS, REMAINING_REQUESTS, RESET_TOKENS
    session = PromptSession(completer=AtFileCompleter(), bottom_toolbar=get_toolbar, key_bindings=kb)
    print("\033[94mAida Streaming Terminal Active. [Ctrl+C] halts generation. [exit] to quit.\033[0m")
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
                messages.append(types.Content(role="user", parts=[types.Part.from_text(text=f"System Notification: {snapshot_str}")]))
                print(f"\033[90m{snapshot_str}\033[0m")
                continue
            
            messages.append(types.Content(role="user", parts=[types.Part.from_text(text=user_input)]))
            
            # Continuous processing loop to catch tool calls and text returns
            while True:
                function_calls = []
                accumulated_text = ""
                last_printed_candidate = None
                try:
                    # FEATURE 2 & 3: Run the streaming channel with explicit tool schemas
                    response_stream = client.models.with_raw_response.generate_content_stream(
                        model=MODEL,
                        contents=messages,
                        config=types.GenerateContentConfig(
                            system_instruction=SYSTEM_PROMPT,
                            tools=[types.Tool(function_declarations=[
                                types.FunctionDeclaration(
                                    name='write_file',
                                    description='Write/create a file',
                                    parameters=types.Schema(
                                        type="OBJECT",
                                        properties={
                                            "filename": types.Schema(type="STRING", description="The name of the file to write"),
                                            "content": types.Schema(type="STRING", description="The file contents")
                                        },
                                        required=["filename", "content"]
                                    )
                                ),
                                types.FunctionDeclaration(
                                    name='read_file',
                                    description='Read a file',
                                    parameters=types.Schema(
                                        type="OBJECT",
                                        properties={"filename": types.Schema(type="STRING")},
                                        required=["filename"]
                                    )
                                ),
                                types.FunctionDeclaration(
                                    name='edit_file',
                                    description='Search and replace text in a file',
                                    parameters=types.Schema(
                                        type="OBJECT",
                                        properties={
                                            "filename": types.Schema(type="STRING"),
                                            "search_block": types.Schema(type="STRING"),
                                            "replace_block": types.Schema(type="STRING")
                                        },
                                        required=["filename", "search_block", "replace_block"]
                                    )
                                ),
                                types.FunctionDeclaration(
                                    name='list_dir',
                                    description='List directory contents (ls)',
                                    parameters=types.Schema(
                                        type="OBJECT",
                                        properties={"path": types.Schema(type="STRING")},
                                    )
                                ),
                                types.FunctionDeclaration(
                                    name='run_command',
                                    description='Run shell command',
                                    parameters=types.Schema(
                                        type="OBJECT",
                                        properties={"command": types.Schema(type="STRING")},
                                        required=["command"]
                                    )
                                ),
                                types.FunctionDeclaration(
                                    name='web_search',
                                    description='Search the internet for current info',
                                    parameters=types.Schema(
                                        type="OBJECT",
                                        properties={"query": types.Schema(type="STRING")},
                                        required=["query"]
                                    )
                                ),
                                types.FunctionDeclaration(
                                    name='change_directory',
                                    description='Change working directory (cd)',
                                    parameters=types.Schema(
                                        type="OBJECT",
                                        properties={"path": types.Schema(type="STRING")},
                                        required=["path"]
                                    )
                                ),
                                types.FunctionDeclaration(
                                    name='search_in_files',
                                    description='Search local files for patterns',
                                    parameters=types.Schema(
                                        type="OBJECT",
                                        properties={
                                            "query": types.Schema(type="STRING"),
                                            "file_pattern": types.Schema(type="STRING")
                                        },
                                        required=["query"]
                                    )
                                ),
                            ])]
                        )
                    )
                    
                    # Update rate limits from headers
                    headers = response_stream.http_response.headers
                    REMAINING_TOKENS = headers.get('x-ratelimit-remaining-tokens', "N/A")
                    REMAINING_REQUESTS = headers.get('x-ratelimit-remaining-requests', "N/A")
                    RESET_TOKENS = headers.get('x-ratelimit-reset-tokens', "N/A")
                    
                    for chunk in response_stream:
                        # Process inline streaming text
                        if chunk.text:
                            accumulated_text += chunk.text
                            sys.stdout.write(chunk.text)
                            sys.stdout.flush()
                        # Accumulate structural tool demands if requested
                        if chunk.function_calls:
                            function_calls.extend(chunk.function_calls)
                        if chunk.candidates:
                            last_printed_candidate = chunk.candidates[0].content
                            
                except KeyboardInterrupt:
                    print("\n\033[91mStream execution halted by user.\033[0m")
                    # Clear streaming states to gracefully return back to the primary prompt loop
                    break
                    
                # Synchronize the history buffer using the last structural content configuration
                if last_printed_candidate:
                    messages.append(last_printed_candidate)
                elif accumulated_text:
                    messages.append(types.Content(role="model", parts=[types.Part.from_text(text=accumulated_text)]))
                    
                # Handle downstream execution steps
                if function_calls:
                    print() # Clear line after text content
                    tool_parts = []
                    for call in function_calls:
                        tool_name = call.name
                        tool_args = dict(call.args) if call.args else {}
                        
                        # Cleaner, bulleted tool output
                        print(f"  \033[92m• Executing: \033[1m{tool_name}\033[0m")
                        for arg, val in tool_args.items():
                            # Truncate long content for readability
                            display_val = (str(val)[:50] + '...') if len(str(val)) > 53 else str(val)
                            print(f"    \033[90m└ {arg}: {display_val}\033[0m")
                        
                        if tool_name in AVAILABLE_TOOLS:
                            result_text = AVAILABLE_TOOLS[tool_name](**tool_args) if tool_args else AVAILABLE_TOOLS[tool_name]()
                        else:
                            result_text = f"Error: Tool '{tool_name}' unknown."
                            
                        tool_parts.append(types.Part.from_function_response(
                            name=tool_name,
                            response={"result": str(result_text)}
                        ))
                    messages.append(types.Content(role="tool", parts=tool_parts))
                    continue # Re-evaluate downstream workflows
                if accumulated_text:
                    print() # Complete trailing line formatting
                    break
                    
        except KeyboardInterrupt:
            print("\n\033[91mSession interrupted.\033[0m")
        except google_exceptions.ResourceExhausted as e:
            print(f"\n\033[91mRate limit exceeded: {str(e)}\033[0m")
            print(f"\033[93mCheck the 'Reset' field in the toolbar for an estimated reset time.\033[0m")
        except Exception as e:
            print(f"\n\033[91mError: {str(e)}\033[0m")

if __name__ == '__main__':
    main()
