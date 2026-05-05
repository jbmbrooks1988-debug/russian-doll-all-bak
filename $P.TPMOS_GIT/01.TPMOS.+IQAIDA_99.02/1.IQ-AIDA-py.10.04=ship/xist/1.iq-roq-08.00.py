#!/usr/bin/env python3
import ollama, os, subprocess, threading, time, sys, fnmatch
from prompt_toolkit import PromptSession
from prompt_toolkit.completion import PathCompleter
from prompt_toolkit.formatted_text import HTML
from prompt_toolkit.patch_stdout import patch_stdout
from duckduckgo_search import DDGS

# --- CONFIG ---
MODEL = 'llama3-groq-tool-use'
YOLO_MODE = False
CONTEXT_LIMIT = 8192

# --- THE HANDS (TOOLS) ---
def web_search(query):
    try:
        with DDGS() as ddgs:
            results = [r for r in ddgs.text(query, max_results=5)]
            return "\n\n".join([f"Title: {r['title']}\nSnippet: {r['body']}" for r in results])
    except Exception as e: return f"Error: {str(e)}"

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

stop_animation = False
def thinking_animation():
    chars, start = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"], time.time()
    idx = 0
    while not stop_animation:
        elapsed = time.time() - start
        sys.stdout.write(f"\r\033[90m{chars[idx % 10]} Thinking... ({elapsed:.1f}s)\033[0m")
        sys.stdout.flush()
        idx += 1; time.sleep(0.1)
    sys.stdout.write("\r" + " " * 50 + "\r")

# --- MAIN LOOP ---
def main():
    global YOLO_MODE, messages, stop_animation
    session = PromptSession(completer=PathCompleter(), bottom_toolbar=get_toolbar)
    print("\033[94mGrok-TUI v2.1 Active. [Ctrl+C] to cancel thinking. [exit] to quit.\033[0m")

    while True:
        try:
            with patch_stdout():
                user_input = session.prompt(">> ")
            
            if not user_input: continue
            if user_input.lower() in ['exit', 'quit']: break
            if user_input.lower() == '/clear': 
                messages = [messages[0]]
                print("\033[90mMemory cleared.\033[0m")
                continue
            if user_input.lower() == '/yolo': 
                YOLO_MODE = not YOLO_MODE
                continue

            messages.append({'role': 'user', 'content': user_input})
            
            while True:
                stop_animation = False
                t = threading.Thread(target=thinking_animation); t.start()
                try:
                    response = ollama.chat(model=MODEL, messages=messages, tools=[
                        {'type': 'function', 'function': {'name': 'web_search', 'description': 'Search web', 'parameters': {'type': 'object', 'properties': {'query': {'type': 'string'}}, 'required': ['query']}}},
                        {'type': 'function', 'function': {'name': 'write_file', 'description': 'New file', 'parameters': {'type': 'object', 'properties': {'filename': {'type': 'string'}, 'content': {'type': 'string'}}, 'required': ['filename', 'content']}}},
                        {'type': 'function', 'function': {'name': 'read_file', 'description': 'Read file', 'parameters': {'type': 'object', 'properties': {'filename': {'type': 'string'}}, 'required': ['filename']}}},
                        {'type': 'function', 'function': {'name': 'edit_file', 'description': 'Edit file', 'parameters': {'type': 'object', 'properties': {'filename': {'type': 'string'}, 'search_block': {'type': 'string'}, 'replace_block': {'type': 'string'}}, 'required': ['filename', 'search_block', 'replace_block']}}},
                        {'type': 'function', 'function': {'name': 'run_command', 'description': 'Shell', 'parameters': {'type': 'object', 'properties': {'command': {'type': 'string'}}, 'required': ['command']}}}
                    ])
                    stop_animation = True; t.join()
                except KeyboardInterrupt:
                    stop_animation = True; t.join()
                    print("\n\033[91m[REQUEST CANCELLED]\033[0m")
                    break

                if not response.message.tool_calls:
                    print(f"\n{response.message.content}\n")
                    messages.append(response.message); break
                
                messages.append(response.message)
                for tool in response.message.tool_calls:
                    name, args = tool.function.name, tool.function.arguments
                    print(f"\033[90m[Action: {name}]\033[0m")
                    if name == 'web_search': res = web_search(**args)
                    elif name == 'write_file': res = write_file(**args)
                    elif name == 'read_file': res = read_file(**args)
                    elif name == 'edit_file': res = edit_file(**args)
                    elif name == 'run_command': res = run_command(**args)
                    messages.append({'role': 'tool', 'content': res, 'name': name})

        except KeyboardInterrupt: continue
        except EOFError: break
        except Exception as e: print(f"Error: {e}")

if __name__ == "__main__":
    main()
