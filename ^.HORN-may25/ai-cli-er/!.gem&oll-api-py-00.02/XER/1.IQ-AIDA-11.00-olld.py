#!/usr/bin/env python3
import os, subprocess, threading, time, sys, fnmatch, re, json
from prompt_toolkit import PromptSession
from prompt_toolkit.completion import PathCompleter, Completer
from prompt_toolkit.document import Document
from prompt_toolkit.formatted_text import HTML
from prompt_toolkit.patch_stdout import patch_stdout
from duckduckgo_search import DDGS

# --- CONFIG ---
MODEL_PATH = '../models/llama3-groq.gguf'
YOLO_MODE = False
CONTEXT_LIMIT = 8192

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

messages = [{'role': 'system', 'content': SYSTEM_PROMPT}]

# --- THE HANDS (TOOLS) ---
def web_search(query):
    try:
        with DDGS() as ddgs:
            results = [r for r in ddgs.text(query, max_results=5)]
            return "\n".join([f"Title: {r['title']}\nURL: {r['href']}\nSnippet: {r['body']}" for r in results])
    except Exception as e: return f"Search Error: {str(e)}"

def write_file(filename, content):
    with open(filename, 'w') as f: f.write(content)
    return f"Done: Wrote to {filename}"

def read_file(filename):
    try:
        with open(filename, 'r') as f: return f.read()
    except Exception as e: return f"Error: {str(e)}"

def edit_file(filename, search_block, replace_block):
    try:
        with open(filename, 'r') as f: content = f.read()
        if search_block.strip() not in content:
            return "Error: Search block not found. Ensure exact match including spacing."
        new_content = content.replace(search_block.strip(), replace_block.strip(), 1)
        with open(filename, 'w') as f: f.write(new_content)
        return f"Successfully edited {filename}."
    except Exception as e: return f"Error: {str(e)}"

def search_in_files(query, file_pattern="*"):
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
        files = os.listdir(path)
        return "\n".join(files) if files else "Empty."
    except Exception as e: return f"Error: {str(e)}"

# --- LOCAL INFERENCE BRIDGE ---
def format_prompt(messages):
    prompt = ""
    for m in messages:
        role = m['role']
        content = m['content']
        prompt += f"<|start_header_id|>{role}<|end_header_id|>\n\n{content}<|eot_id|>"
    prompt += "<|start_header_id|>assistant<|end_header_id|>\n\n"
    return prompt

def call_local_model(prompt):
    # Using the compiled llama-cli in tools/
    cmd = [
        "./tools/llama-cli",
        "-m", MODEL_PATH,
        "-p", prompt,
        "-n", "1024",
        "--log-disable",
        "--no-display-prompt",
        "-no-cnv"
    ]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        return f"Error: Model call failed. {e.stderr}"

def parse_tool_calls(text):
    # Llama3-Groq tool call format: <tool_call>{...}</tool_call>
    pattern = r"<tool_call>(.*?)</tool_call>"
    matches = re.findall(pattern, text, re.DOTALL)
    calls = []
    for m in matches:
        try:
            calls.append(json.loads(m.strip()))
        except: continue
    return calls

# --- UI UTILS ---
def get_context_pct():
    text = "".join([str(m.get('content', '')) for m in messages])
    return int((len(text.split()) * 1.3 / CONTEXT_LIMIT) * 100)

def get_toolbar():
    pct = get_context_pct()
    yolo = "YOLO" if YOLO_MODE else "SAFE"
    cwd = os.getcwd().replace(os.path.expanduser("~"), "~")
    return HTML(f' <b>{cwd}</b> | <b>LOCAL-GGUF</b> | <b>Quota: {pct}%</b> | <b>{yolo}</b>')

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
    print("\033[94mAida v11.0 [NO-OLLAMA] Active. [Ctrl+C] cancels request. [/scan] context snapshot. [exit] to quit.\033[0m")
    
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
                print(f"\033[90mYOLO Mode: {'ON' if YOLO_MODE else 'OFF'}\033[0m")
                continue
            if user_input.lower() == '/scan':
                snapshot = ["[Context Snapshot]"]
                for item in sorted(os.listdir('.')):
                    if os.path.isdir(item): snapshot.append(f"📁 {item}/")
                    else: snapshot.append(f"📄 {item}")
                snapshot_str = "\n".join(snapshot)
                messages.append({'role': 'system', 'content': snapshot_str})
                print(f"\033[90m{snapshot_str}\033[0m")
                continue
                
            messages.append({'role': 'user', 'content': user_input})
            
            while True:
                stop_animation = False
                t = threading.Thread(target=thinking_animation, daemon=True)
                t.start()
                
                try:
                    prompt = format_prompt(messages)
                    response_text = call_local_model(prompt)
                except KeyboardInterrupt:
                    print("\n\033[91m[REQUEST CANCELLED]\033[0m")
                    break
                except Exception as e:
                    print(f"\n\033[91mError: {str(e)}\033[0m")
                    break
                finally:
                    stop_animation = True
                    t.join(timeout=0.5)

                tool_calls = parse_tool_calls(response_text)
                
                if not tool_calls:
                    print(f"\n{response_text}\n")
                    messages.append({'role': 'assistant', 'content': response_text})
                    break

                # Clean up response text by removing tool call XML for the history
                cleaned_text = re.sub(r"<tool_call>.*?</tool_call>", "", response_text, flags=re.DOTALL).strip()
                if cleaned_text: print(f"\n{cleaned_text}")
                
                messages.append({'role': 'assistant', 'content': response_text})
                
                for tool in tool_calls:
                    name = tool.get('name')
                    args = tool.get('arguments', {})
                    print(f"\n\033[90m[Action: {name}]\033[0m")
                    
                    if name == 'web_search': res = web_search(**args)
                    elif name in ['write_file', 'write']: res = write_file(**args)
                    elif name in ['read_file', 'read']: res = read_file(**args)
                    elif name == 'edit_file': res = edit_file(**args)
                    elif name == 'search_in_files': res = search_in_files(**args)
                    elif name in ['run_command', 'exec_cmd']: res = run_command(**args)
                    elif name == 'change_directory': res = change_directory(**args)
                    elif name == 'list_dir': res = list_dir(**args)
                    else: res = f"Unknown tool: {name}"
                    
                    print(f"\033[92m>> {res}\033[0m")
                    messages.append({'role': 'tool', 'content': res, 'name': name})
                            
        except KeyboardInterrupt:
            print("\n\033[90m[CANCELLED] Type 'exit' to quit.\033[0m")
            continue
        except EOFError:
            break
        except Exception as e:
            print(f"\n\033[91mUnexpected Error: {e}\033[0m")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\033[90mAida signing off...\033[0m")
