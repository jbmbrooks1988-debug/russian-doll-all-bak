#!/usr/bin/env python3
import asyncio, ollama, os, subprocess, sys, fnmatch, time
from prompt_toolkit import PromptSession
from prompt_toolkit.completion import PathCompleter, Completer
from prompt_toolkit.document import Document
from prompt_toolkit.formatted_text import HTML
from prompt_toolkit.patch_stdout import patch_stdout
from duckduckgo_search import DDGS

# --- CONFIG ---
MODEL = 'llama3-groq-tool-use'
YOLO_MODE = False
CONTEXT_LIMIT = 8192

# --- TOOLS DEFINITION ---
TOOLS_LIST = [
    {'type': 'function', 'function': {'name': 'web_search', 'description': 'Search web', 'parameters': {'type': 'object', 'properties': {'query': {'type': 'string'}}, 'required': ['query']}}},
    {'type': 'function', 'function': {'name': 'write_file', 'description': 'New file', 'parameters': {'type': 'object', 'properties': {'filename': {'type': 'string'}, 'content': {'type': 'string'}}, 'required': ['filename', 'content']}}},
    {'type': 'function', 'function': {'name': 'read_file', 'description': 'Read file', 'parameters': {'type': 'object', 'properties': {'filename': {'type': 'string'}}, 'required': ['filename']}}},
    {'type': 'function', 'function': {'name': 'edit_file', 'description': 'Edit file', 'parameters': {'type': 'object', 'properties': {'filename': {'type': 'string'}, 'search_block': {'type': 'string'}, 'replace_block': {'type': 'string'}}, 'required': ['filename', 'search_block', 'replace_block']}}},
    {'type': 'function', 'function': {'name': 'run_command', 'description': 'Shell', 'parameters': {'type': 'object', 'properties': {'command': {'type': 'string'}}, 'required': ['command']}}}
]

# --- THE HANDS (TOOLS) ---
def web_search(query):
    try:
        with DDGS() as ddgs:
            results = [r for r in ddgs.text(query, max_results=5)]
            return "\n".join([f"Title: {r['title']}\nSnippet: {r['body']}" for r in results])
    except Exception as e: 
        return f"Error: {str(e)}"

def write_file(filename, content):
    with open(filename, 'w') as f: f.write(content)
    return f"Created {filename}"

def read_file(filename):
    try:
        with open(filename, 'r') as f: return f.read()
    except Exception as e: return f"Error: {str(e)}"

def edit_file(filename, search_block, replace_block):
    try:
        with open(filename, 'r') as f: content = f.read()
        search_block = search_block.strip()
        if search_block not in content:
            return "Error: Exact search block not found. Use read_file to check indentation."
        new_content = content.replace(search_block, replace_block.strip(), 1)
        with open(filename, 'w') as f: f.write(new_content)
        return f"Edited {filename}"
    except Exception as e: return f"Error: {str(e)}"

def run_command(command):
    global YOLO_MODE
    if not YOLO_MODE:
        confirm = input(f"\n\033[93m[SAFEGUARD] Run '{command}'? (y/n): \033[0m")
        if confirm.lower() != 'y': return "Aborted."
    res = subprocess.run(command, shell=True, capture_output=True, text=True)
    return f"OUT: {res.stdout}\nERR: {res.stderr}"

# --- UI UTILS ---
messages = [{'role': 'system', 'content': 'You are a technical coding agent. Use edit_file for existing files and web_search for info.'}]

def get_context_pct():
    text = "".join([str(m.get('content', '')) for m in messages])
    return int((len(text.split()) * 1.3 / CONTEXT_LIMIT) * 100)

def get_toolbar():
    pct = get_context_pct()
    yolo = "YOLO" if YOLO_MODE else "SAFE"
    cwd = os.getcwd().replace(os.path.expanduser("~"), "~")
    return HTML(f' <b>{cwd}</b> | <b>{MODEL}</b> | <b>Quota: {pct}%</b> | <b>{yolo}</b>')

# --- CUSTOM @ COMPLETER ---
class AtFileCompleter(Completer):
    def __init__(self):
        self.path_completer = PathCompleter(expanduser=True)

    def get_completions(self, document, complete_event):
        word = document.get_word_before_cursor()
        if word.startswith('@'):
            # Strip @ and create a new document for path completion
            new_text = document.text_before_cursor[:-len(word)] + word[1:]
            new_cursor_pos = len(new_text)
            sub_doc = Document(new_text, new_cursor_pos)
            for completion in self.path_completer.get_completions(sub_doc, complete_event):
                yield completion

# --- ASYNC THINKING ANIMATION ---
async def thinking_animation():
    chars = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"]
    start = time.time()
    idx = 0
    try:
        while True:
            elapsed = time.time() - start
            sys.stdout.write(f"\r\033[90m{chars[idx % 10]} Thinking... ({elapsed:.1f}s)\033[0m")
            sys.stdout.flush()
            idx += 1
            await asyncio.sleep(0.1)
    except asyncio.CancelledError:
        sys.stdout.write("\r" + " " * 60 + "\r")
        sys.stdout.flush()

# --- MAIN ASYNC LOOP ---
async def main():
    global YOLO_MODE, messages
    session = PromptSession(completer=AtFileCompleter(), bottom_toolbar=get_toolbar)
    print("\033[94mGrok-TUI v2.1 Active (Async). [Ctrl+C] to cancel thinking. [exit] to quit.\033[0m")
    
    while True:
        try:
            with patch_stdout():
                user_input = await session.prompt_async(">> ")
            if not user_input: continue
            if user_input.lower() in ['exit', 'quit']: break
            if user_input.lower() == '/clear':
                messages = [messages[0]]
                print("\033[90mMemory cleared.\033[0m")
                continue
            if user_input.lower() == '/yolo':
                YOLO_MODE = not YOLO_MODE
                print(f"\033[90mYOLO Mode: {'ON' if YOLO_MODE else 'OFF'}\033[0m")
                continue
                
            messages.append({'role': 'user', 'content': user_input})
            
            while True:
                anim_task = asyncio.create_task(thinking_animation())
                try:
                    # Run blocking Ollama call in a thread
                    response = await asyncio.to_thread(
                        ollama.chat, 
                        model=MODEL, 
                        messages=messages, 
                        tools=TOOLS_LIST
                    )
                    anim_task.cancel()
                except KeyboardInterrupt:
                    anim_task.cancel()
                    print("\n\033[91m[REQUEST CANCELLED]\033[0m")
                    break
                except Exception as e:
                    anim_task.cancel()
                    print(f"\n\033[91mError: {e}\033[0m")
                    break

                if not response.message.tool_calls:
                    print(f"\n{response.message.content}\n")
                    messages.append(response.message)
                    break

                messages.append(response.message)
                for tool in response.message.tool_calls:
                    name = tool.function.name
                    args = tool.function.arguments
                    print(f"\033[90m[Action: {name}]\033[0m")
                    
                    # Execute tools (synchronous, but fast enough for CLI)
                    if name == 'web_search': res = web_search(**args)
                    elif name == 'write_file': res = write_file(**args)
                    elif name == 'read_file': res = read_file(**args)
                    elif name == 'edit_file': res = edit_file(**args)
                    elif name == 'run_command': res = run_command(**args)
                    
                    messages.append({'role': 'tool', 'content': res, 'name': name})
                    
        except KeyboardInterrupt:
            print("\n\033[90mInterrupted. Type 'exit' to quit.\033[0m")
            continue
        except EOFError:
            break
        except Exception as e:
            print(f"\n\033[91mUnexpected Error: {e}\033[0m")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nGoodbye!")