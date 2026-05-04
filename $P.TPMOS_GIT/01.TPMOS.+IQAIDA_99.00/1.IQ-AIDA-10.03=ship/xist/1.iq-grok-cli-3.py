#!/usr/bin/env python3
import ollama
import os
import subprocess
import threading
import time
import sys

# --- CONFIG ---
MODEL = 'llama3-groq-tool-use'
CONTEXT_LIMIT = 8192
YOLO_MODE = False

# --- THE HANDS (ACTUAL FUNCTIONS) ---
def write_file(filename, content):
    with open(filename, 'w') as f: f.write(content)
    return f"Done: Wrote to {filename}"

def read_file(filename):
    try:
        with open(filename, 'r') as f: return f.read()
    except Exception as e: return f"Error: {str(e)}"

def edit_file(filename, search_block, replace_block):
    """Edits a file by replacing a specific block of text."""
    try:
        with open(filename, 'r') as f: content = f.read()
        if search_block not in content:
            return "Error: Search block not found. Make sure the search text is exact."
        new_content = content.replace(search_block, replace_block, 1)
        with open(filename, 'w') as f: f.write(new_content)
        return f"Successfully edited {filename}."
    except Exception as e: return f"Error: {str(e)}"

def run_command(command):
    global YOLO_MODE
    if not YOLO_MODE:
        confirm = input(f"\n\033[93m[SAFEGUARD] Run command: '{command}'? (y/n): \033[0m")
        if confirm.lower() != 'y': return "Command aborted by user."
    try:
        result = subprocess.run(command, shell=True, capture_output=True, text=True)
        return f"STDOUT: {result.stdout}\nSTDERR: {result.stderr}"
    except Exception as e: return str(e)

def change_directory(path):
    try:
        os.chdir(path); return f"Changed to: {os.getcwd()}"
    except Exception as e: return f"Error: {str(e)}"

def list_dir(path="."):
    try:
        files = os.listdir(path); return "\n".join(files) if files else "Empty."
    except Exception as e: return f"Error: {str(e)}"

# --- UI ANIMATION ---
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
    sys.stdout.write("\r" + " " * 30 + "\r") # Clear line

# --- UTILS ---
def get_context_stats(messages):
    text = "".join([str(m.get('content', '')) for m in messages])
    estimated_tokens = len(text.split()) * 1.3
    percent = (estimated_tokens / CONTEXT_LIMIT) * 100
    return int(percent)

# --- BRAIN ---
tools = [
    {'type': 'function', 'function': {'name': 'write_file', 'description': 'Write file', 'parameters': {'type': 'object', 'properties': {'filename': {'type': 'string'}, 'content': {'type': 'string'}}, 'required': ['filename', 'content']}}},
    {'type': 'function', 'function': {'name': 'read_file', 'description': 'Read file', 'parameters': {'type': 'object', 'properties': {'filename': {'type': 'string'}}, 'required': ['filename']}}},
    {'type': 'function', 'function': {'name': 'edit_file', 'description': 'Replace a block of text in a file.', 'parameters': {'type': 'object', 'properties': {'filename': {'type': 'string'}, 'search_block': {'type': 'string', 'description': 'The exact text to find'}, 'replace_block': {'type': 'string', 'description': 'The text to replace it with'}}, 'required': ['filename', 'search_block', 'replace_block']}}},
    {'type': 'function', 'function': {'name': 'run_command', 'description': 'Run shell command', 'parameters': {'type': 'object', 'properties': {'command': {'type': 'string'}}, 'required': ['command']}}},
    {'type': 'function', 'function': {'name': 'change_directory', 'description': 'CD', 'parameters': {'type': 'object', 'properties': {'path': {'type': 'string'}}, 'required': ['path']}}},
    {'type': 'function', 'function': {'name': 'list_dir', 'description': 'LS', 'parameters': {'type': 'object', 'properties': {'path': {'type': 'string'}}}}}
]

messages = [{'role': 'system', 'content': 'You are an agentic coding assistant. To edit files, use edit_file by providing the EXACT block of text to replace. Use run_command for terminal actions.'}]

print("\033[94mGrok-CLI Pro Active.\033[0m (/yolo, /clear, exit)")

while True:
    pct = get_context_stats(messages)
    color = "\033[92m" if pct < 75 else "\033[91m"
    user_input = input(f"[{color}{pct}%\033[0m] \033[92m>> \033[0m").strip()
    
    if user_input.lower() in ['exit', 'quit']: break
    if user_input.lower() == '/clear':
        messages = [messages[0]]
        print("Memory cleared."); continue
    if user_input.lower() == '/yolo':
        YOLO_MODE = not YOLO_MODE
        print(f"YOLO: {'ON' if YOLO_MODE else 'OFF'}"); continue

    messages.append({'role': 'user', 'content': user_input})
    
    while True:
        stop_animation = False
        t = threading.Thread(target=thinking_animation)
        t.start()
        
        try:
            response = ollama.chat(model=MODEL, messages=messages, tools=tools)
        finally:
            stop_animation = True
            t.join()
        
        if not response.message.tool_calls:
            print(f"\n{response.message.content}\n")
            messages.append(response.message); break
            
        messages.append(response.message)
        for tool in response.message.tool_calls:
            name, args = tool.function.name, tool.function.arguments
            print(f"\033[90m[Action: {name}]\033[0m")
            
            if name == 'write_file': result = write_file(**args)
            elif name == 'read_file': result = read_file(**args)
            elif name == 'edit_file': result = edit_file(**args)
            elif name == 'run_command': result = run_command(**args)
            elif name == 'change_directory': result = change_directory(**args)
            elif name == 'list_dir': result = list_dir(**args)
            
            messages.append({'role': 'tool', 'content': result, 'name': name})
