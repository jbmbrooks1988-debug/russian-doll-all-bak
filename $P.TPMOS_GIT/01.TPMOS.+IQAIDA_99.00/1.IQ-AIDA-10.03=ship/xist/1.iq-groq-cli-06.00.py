#!/usr/bin/env python3
import ollama, os, subprocess, threading, time, sys, fnmatch
from duckduckgo_search import DDGS

# --- CONFIG ---
MODEL = 'llama3-groq-tool-use'
CONTEXT_LIMIT = 8192
YOLO_MODE = False

# --- THE HANDS ---
def web_search(query):
    try:
        with DDGS() as ddgs:
            results = [r for r in ddgs.text(query, max_results=5)]
            return "\n\n".join([f"Title: {r['title']}\nURL: {r['href']}\nSnippet: {r['body']}" for r in results])
    except Exception as e: return f"Search Error: {str(e)}"

def write_file(filename, content):
    with open(filename, 'w') as f: f.write(content)
    return f"Done: Created {filename}"

def read_file(filename):
    try:
        with open(filename, 'r') as f: return f.read()
    except Exception as e: return f"Error: {str(e)}"

def edit_file(filename, search_block, replace_block):
    try:
        with open(filename, 'r') as f: content = f.read()
        if search_block.strip() not in content:
            return f"Error: Exact block not found in {filename}. Please use read_file to confirm the content first."
        new_content = content.replace(search_block.strip(), replace_block.strip(), 1)
        with open(filename, 'w') as f: f.write(new_content)
        return f"Successfully edited {filename}."
    except Exception as e: return f"Error: {str(e)}"

def run_command(command):
    global YOLO_MODE
    if not YOLO_MODE:
        confirm = input(f"\n\033[93m[SAFEGUARD] Run: '{command}'? (y/n): \033[0m")
        if confirm.lower() != 'y': return "Aborted."
    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    return f"STDOUT: {result.stdout}\nSTDERR: {result.stderr}"

# --- UI ---
stop_animation = False
def thinking_animation():
    chars = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"]
    start_time = time.time()
    idx = 0
    while not stop_animation:
        elapsed = time.time() - start_time
        sys.stdout.write(f"\r\033[90m{chars[idx % len(chars)]} Thinking... ({elapsed:.1f}s)\033[0m")
        sys.stdout.flush()
        idx += 1
        time.sleep(0.1)
    sys.stdout.write("\r" + " " * 50 + "\r")

def get_context_stats(messages):
    text = "".join([str(m.get('content', '')) for m in messages])
    tokens = len(text.split()) * 1.3
    return int((tokens / CONTEXT_LIMIT) * 100)

tools = [
    {'type': 'function', 'function': {'name': 'web_search', 'description': 'Search web', 'parameters': {'type': 'object', 'properties': {'query': {'type': 'string'}}, 'required': ['query']}}},
    {'type': 'function', 'function': {'name': 'write_file', 'description': 'New file only', 'parameters': {'type': 'object', 'properties': {'filename': {'type': 'string'}, 'content': {'type': 'string'}}, 'required': ['filename', 'content']}}},
    {'type': 'function', 'function': {'name': 'read_file', 'description': 'Read file', 'parameters': {'type': 'object', 'properties': {'filename': {'type': 'string'}}, 'required': ['filename']}}},
    {'type': 'function', 'function': {'name': 'edit_file', 'description': 'Edit existing file', 'parameters': {'type': 'object', 'properties': {'filename': {'type': 'string'}, 'search_block': {'type': 'string'}, 'replace_block': {'type': 'string'}}, 'required': ['filename', 'search_block', 'replace_block']}}},
    {'type': 'function', 'function': {'name': 'run_command', 'description': 'Shell cmd', 'parameters': {'type': 'object', 'properties': {'command': {'type': 'string'}}, 'required': ['command']}}}
]

messages = [{'role': 'system', 'content': 'You are a coding agent. PREFER edit_file for existing files. Always check what files exist in the current directory before asking the user.'}]

print("\033[94mGrok-CLI Pro Active.\033[0m")

while True:
    pct = get_context_stats(messages)
    files = ", ".join(os.listdir('.'))
    # Inject current directory state so it "sees" what you have
    messages[0]['content'] = f"System: You are in {os.getcwd()}. Files: [{files}]. Use edit_file for changes."
    
    user_input = input(f"[{pct}%] \033[92m>> \033[0m").strip()
    if user_input.lower() in ['exit', 'quit']: break
    if user_input.lower() == '/clear': messages = [messages[0]]; continue
    if user_input.lower() == '/yolo': YOLO_MODE = not YOLO_MODE; print("YOLO Toggle"); continue

    messages.append({'role': 'user', 'content': user_input})
    
    while True:
        stop_animation = False
        t = threading.Thread(target=thinking_animation); t.start()
        try:
            response = ollama.chat(model=MODEL, messages=messages, tools=tools)
        finally:
            stop_animation = True; t.join()
        
        if not response.message.tool_calls:
            print(f"\n{response.message.content}\n"); messages.append(response.message); break
            
        messages.append(response.message)
        for tool in response.message.tool_calls:
            name, args = tool.function.name, tool.function.arguments
            print(f"\033[90m[Action: {name}]\033[0m")
            if name == 'web_search': result = web_search(**args)
            elif name == 'write_file': result = write_file(**args)
            elif name == 'read_file': result = read_file(**args)
            elif name == 'edit_file': result = edit_file(**args)
            elif name == 'run_command': result = run_command(**args)
            messages.append({'role': 'tool', 'content': result, 'name': name})
