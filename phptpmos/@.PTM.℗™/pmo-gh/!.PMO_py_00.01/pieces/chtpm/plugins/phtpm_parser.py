#!/usr/bin/env python3
"""
PHTPM Parser - Python Sovereign Theater
Standardized for PMO v1.4.0 (Option Z & Single Pulse).
MIRROR IMPLEMENTATION: Exact Python translation of chtpm_parser.c
- Tokenizer: tokenize() function mirrors C exactly
- Parser: parse_layout_elements() mirrors parse_chtm() exactly  
- Renderer: render_element() recursive function mirrors C exactly
- Input: process_key() mirrors C process_key() exactly
"""

import os
import re
import sys
import time
import subprocess
import signal
from datetime import datetime

# Import TPM Utils
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../locations'))
import path_utils
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../master_ledger/plugins'))
from piece_manager import piece_manager

class UIElement:
    def __init__(self, el_type, label="", href="", on_click="", interactive_idx=0):
        self.type = el_type
        self.label = label
        self.href = href
        self.on_click = on_click
        self.input_buffer = ""
        self.interactive_idx = interactive_idx
        self.parent_index = -1
        self.children = []  # C: children[MAX_CHILDREN]
        self.num_children = 0  # C: num_children
        self.visibility = True  # C: visibility
        self.current_child_focus = 0  # C: current_child_focus

class PHTPMParser:
    def __init__(self, layout_path):
        self.layout_path = layout_path
        self.focus_index = 0
        self.active_index = -1
        self.elements = []
        self.variables = {}
        
        # GHOST HAND FIX: Ignore all history created before birth
        hist_path = path_utils.get_piece_path('keyboard', 'history.txt')
        self.last_history_position = os.path.getsize(hist_path) if hist_path and os.path.exists(hist_path) else 0
        
        self.last_pulse_size = 0
        self.last_view_pulse_size = 0
        self.current_module_proc = None
        self.nav_buffer = ""
        self.clear_nav_on_next = False
        
        # Only load state if it exists and has content (fresh start otherwise)
        state_path = path_utils.get_piece_path('chtpm', 'state.txt')
        if state_path and os.path.exists(state_path) and os.path.getsize(state_path) > 0:
            self.load_state()
        
        self.parse_layout_elements()

    def cleanup_module(self):
        if self.current_module_proc:
            self.current_module_proc.terminate()
            try: self.current_module_proc.wait(timeout=0.2)
            except: self.current_module_proc.kill()
            self.current_module_proc = None

    def launch_module(self, path):
        self.cleanup_module()
        if not path: return
        self.current_module_proc = subprocess.Popen([sys.executable, "-u", path])

    def load_state(self):
        state_path = path_utils.get_piece_path('chtpm', 'state.txt')
        if state_path and os.path.exists(state_path):
            with open(state_path, 'r') as f:
                for line in f:
                    if line.startswith("focus_index="): self.focus_index = int(line.split("=")[1])
                    elif line.startswith("active_index="): self.active_index = int(line.split("=")[1])
                    elif line.startswith("current_layout="): self.layout_path = line.split("=")[1].strip()

    def save_state(self):
        state_path = path_utils.get_piece_path('chtpm', 'state.txt')
        if state_path:
            os.makedirs(os.path.dirname(state_path), exist_ok=True)
            with open(state_path, 'w') as f:
                f.write(f"focus_index={self.focus_index}\nactive_index={self.active_index}\ncurrent_layout={self.layout_path}\n")
        active_flag_path = path_utils.get_piece_path('fuzzpet', 'active.txt')
        if active_flag_path:
            with open(active_flag_path, 'w') as f:
                f.write("1" if (self.active_index != -1 and self.elements[self.active_index].on_click == "INTERACT") else "0")

    def get_attr(self, attr_str, name):
        match = re.search(fr'{name}\s*=\s*["\'](.*?)["\']', attr_str, re.DOTALL)
        return match.group(1) if match else ""

    def parse_layout_elements(self):
        """TOKENIZER: Mirror C implementation exactly (chtpm_parser.c parse_chtm())."""
        self.elements = []
        if not os.path.exists(self.layout_path): return
        with open(self.layout_path, 'r') as f: content = f.read()
        
        # Remove comments (C: remove_comments)
        content = re.sub(r'<!--.*?-->', '', content, flags=re.DOTALL)
        
        # 1. Module Logic (C: parse_chtm module handling)
        module_match = re.search(r'<module>(.*?)</module>', content, re.DOTALL)
        if module_match and self.current_module_proc is None:
            module_path = module_match.group(1).strip()
            # Trim whitespace like C does
            module_path = ' '.join(module_path.split())
            self.launch_module(module_path)
        content = re.sub(r'<module>.*?</module>', '', content, flags=re.DOTALL)
        
        # 2. TOKENIZATION (C: tokenize function)
        # Find all tags and text between them
        tokens = []
        cursor = 0
        while cursor < len(content):
            tag_start = content.find('<', cursor)
            if tag_start == -1:
                # Rest is text
                text_content = content[cursor:]
                if text_content:
                    tokens.append(('TEXT', text_content, '', ''))
                break
            
            # Text before tag
            if tag_start > cursor:
                text_content = content[cursor:tag_start]
                tokens.append(('TEXT', text_content, '', ''))
            
            # Find tag end
            tag_end = content.find('>', tag_start)
            if tag_end == -1:
                break
            
            tag_body = content[tag_start + 1:tag_end]
            
            # Determine token type (C: tokenize logic)
            if tag_body.startswith('/'):
                # Close tag
                tag_name = tag_body[1:].strip()
                tokens.append(('CLOSE', '', tag_name, ''))
            else:
                # Open or self-close tag
                self_closing = tag_body.rstrip().endswith('/')
                if self_closing:
                    tag_body = tag_body.rstrip()[:-1]  # Remove trailing /
                
                # Split tag name and attributes (C: tokenize logic)
                space_pos = tag_body.find(' ')
                if space_pos == -1:
                    tag_name = tag_body.strip()
                    attributes = ''
                else:
                    tag_name = tag_body[:space_pos].strip()
                    attributes = tag_body[space_pos + 1:]
                
                token_type = 'SELFCLOSE' if self_closing else 'OPEN'
                tokens.append((token_type, '', tag_name, attributes))
            
            cursor = tag_end + 1
        
        # 3. PROCESS TOKENS (C: parse_chtm main loop)
        stack = []
        interactive_count = 0
        
        for token_type, text_content, tag_name, attributes in tokens:
            tag_name = tag_name.lower()
            
            if token_type == 'TEXT':
                # Trim whitespace (C: parse_chtm TEXT handling)
                trimmed = text_content.strip()
                if trimmed:
                    el = UIElement('text', trimmed, '', '', 0)
                    el.parent_index = stack[-1] if stack else -1
                    self.elements.append(el)
            
            elif token_type in ('OPEN', 'SELFCLOSE'):
                # Handle module tag (C: parse_chtm module handling)
                if tag_name == 'module':
                    continue  # Already handled above
                
                # Handle panel tag (C: parse_chtm panel handling)
                if tag_name == 'panel':
                    if token_type == 'OPEN':
                        stack.append(-1)  # Panel root
                    continue
                
                # Create element (C: parse_chtm element creation)
                if tag_name not in ['panel', '/panel']:
                    idx = 0
                    if tag_name in ['button', 'link', 'cli_io', 'menu']:
                        interactive_count += 1
                        idx = interactive_count
                    
                    el = UIElement(
                        tag_name,
                        self.get_attr(attributes, 'label'),
                        self.get_attr(attributes, 'href'),
                        self.get_attr(attributes, 'onClick'),
                        idx
                    )
                    el.parent_index = stack[-1] if stack else -1
                    
                    # Add to parent's children (C: parent-child linking)
                    if el.parent_index != -1:
                        parent = self.elements[el.parent_index]
                        if len(parent.children) < 20:
                            parent.children.append(len(self.elements))
                            parent.num_children = len(parent.children)
                    
                    self.elements.append(el)
                    
                    if token_type == 'OPEN' and tag_name in ['menu', 'panel']:
                        stack.append(len(self.elements) - 1)
            
            elif token_type == 'CLOSE':
                if tag_name == 'panel' and stack:
                    stack.pop()
                elif stack:
                    stack.pop()

    def load_vars(self):
        self.variables = {}
        # Mirrors
        fstate_path = path_utils.get_piece_path('fuzzpet', 'state.txt')
        if fstate_path and os.path.exists(fstate_path):
            with open(fstate_path, 'r') as f:
                for line in f:
                    if '=' in line:
                        k, v = line.strip().split('=', 1)
                        self.variables[f"fuzzpet_{k}"] = v
        # Clock
        cstate_path = path_utils.get_piece_path('clock', 'state.txt')
        if cstate_path and os.path.exists(cstate_path):
            with open(cstate_path, 'r') as f:
                for line in f:
                    if line.startswith("turn "): self.variables["clock_turn"] = line.split()[1]
                    elif line.startswith("time "): self.variables["clock_time"] = line.split()[1]
        # Option Z - SOVEREIGN COMPOSITE: Read Module's Stage (view.txt) - MIRROR C EXACTLY
        view_path = path_utils.get_piece_path('fuzzpet', 'view.txt')
        if view_path and os.path.exists(view_path):
            with open(view_path, 'r') as f:
                self.variables["game_map"] = f.read()

    def is_interactive(self, el):
        """Mirror C: is_interactive() function."""
        return el.type in ['button', 'link', 'cli_io', 'menu']

    def render_element(self, idx, frame_buffer):
        """Mirror C: render_element() - recursive element rendering."""
        if idx < 0 or idx >= len(self.elements):
            return
        
        el = self.elements[idx]
        
        # Check parent visibility - hide menu children when menu is not active (C: render_element parent check)
        if el.parent_index != -1:
            parent = self.elements[el.parent_index]
            if parent.type == 'menu' and self.active_index != el.parent_index:
                return  # Hide children of inactive menu
        
        # Variable substitution (C: render_element substitution loop)
        label = el.label
        substituted = ""
        i = 0
        while i < len(label):
            if label[i] == '$' and i + 1 < len(label) and label[i + 1] == '{':
                end = label.find('}', i + 2)
                if end != -1:
                    var_name = label[i + 2:end]
                    val = self.variables.get(var_name, "")
                    substituted += str(val)
                    i = end + 1
                    continue
            substituted += label[i]
            i += 1
        
        # Render based on type (C: render_element type handling)
        if el.type == 'br':
            frame_buffer.append("\n")
        elif el.type == 'text':
            frame_buffer.append(substituted)
        elif self.is_interactive(el):
            # Determine focus indicator (C: render_element interactive handling)
            is_focused = (self.active_index == -1 and idx == self.focus_index)
            
            # Check menu child focus (C: menu child focus logic)
            if el.parent_index != -1:
                parent = self.elements[el.parent_index]
                if parent.type == 'menu' and self.active_index == el.parent_index:
                    is_focused = False
                    for c_idx, child_idx in enumerate(parent.children):
                        if child_idx == idx and parent.current_child_focus == c_idx:
                            is_focused = True
                            break
            
            if idx == self.active_index:
                indicator = "[^]"
            elif is_focused:
                indicator = "[>]"
            else:
                indicator = "[ ]"
            
            # Format output (C: render_element formatting)
            if el.type == 'cli_io':
                line = f"{indicator} {el.interactive_idx}. {substituted}{el.input_buffer}_ "
            else:
                line = f"{indicator} {el.interactive_idx}. [{substituted}] "
            
            # Indent if has parent (C: parent indentation)
            if el.parent_index != -1:
                frame_buffer.append("  ")
            frame_buffer.append(line)
        
        # Recursively render children (C: render_element children loop)
        if el.type in ['menu', 'panel']:
            if el.parent_index == -1 or el.visibility or self.active_index == idx:
                for child_idx in el.children:
                    self.render_element(child_idx, frame_buffer)

    def compose_frame(self):
        """Mirror C: compose_frame() - recursive composition."""
        self.load_vars()
        frame_buffer = []
        
        # Render root elements (C: compose_frame root loop)
        for i in range(len(self.elements)):
            if self.elements[i].parent_index == -1:
                self.render_element(i, frame_buffer)
        
        # Add footer only (C adds only footer, header comes from layout)
        frame_buffer.append("\n╚═══════════════════════════════════════════════════════════╝\n")
        nav_msg = f"Nav > {self.nav_buffer}_" if self.active_index == -1 else f"Active [^]: {self.nav_buffer} (ESC to exit)"
        frame_buffer.append(nav_msg + "\n")
        
        final_frame = "".join(frame_buffer)
        frame_path = path_utils.get_piece_path('display', 'current_frame.txt')
        if frame_path:
            with open(frame_path, 'w') as f: f.write(final_frame)
            pulse_path = path_utils.get_piece_path('display', 'frame_changed.txt')
            if pulse_path:
                with open(pulse_path, 'a') as pf: pf.write("P\n")
        
        if self.clear_nav_on_next:
            self.nav_buffer = ""
            self.clear_nav_on_next = False

    def inject_raw_key(self, key_code):
        # CRITICAL FIX: Module history, NOT keyboard history
        # Keyboard history is for HARDWARE capture only
        hist_path = path_utils.get_piece_path('fuzzpet', 'history.txt')
        if hist_path:
            with open(hist_path, 'a') as f: f.write(f"{key_code}\n")

    def handle_launch_command(self, cmd):
        """Mirror C: handle_launch_command() - lookup app and launch."""
        app_name = cmd[7:]  # Remove "LAUNCH:" prefix
        
        # Log the request
        ledger_path = path_utils.get_piece_path('master_ledger', 'master_ledger.txt')
        if ledger_path:
            from datetime import datetime
            ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            with open(ledger_path, 'a') as f:
                f.write(f"[{ts}] LaunchRequest: {app_name} | Source: phtpm_parser\n")
        
        # Lookup in app_list.txt (C: handle_launch_command)
        app_list_path = path_utils.get_piece_path('os', 'app_list.txt')
        if app_list_path and os.path.exists(app_list_path):
            with open(app_list_path, 'r') as f:
                for line in f:
                    line = line.strip()
                    if '=' in line:
                        name, path = line.split('=', 1)
                        if name.lower() == app_name.lower():
                            self.launch_module(path)
                            break

    def process_key(self, key):
        """Mirror C: process_key() exactly."""
        is_navigable = lambda idx: self.elements[idx].type in ["button", "link", "cli_io"]
        
        if self.active_index == -1:
            # Navigation mode (C: process_key navigation)
            if key in [1002, ord('w'), ord('W')]:
                prev = self.focus_index
                while True:
                    self.focus_index -= 1
                    if self.focus_index < 0:
                        self.focus_index = prev
                        break
                    if is_navigable(self.focus_index):
                        break
                self.nav_buffer = "w" if key in [1002, ord('w')] else "W"
                self.clear_nav_on_next = True
                
            elif key in [1003, ord('s'), ord('S')]:
                prev = self.focus_index
                while True:
                    self.focus_index += 1
                    if self.focus_index >= len(self.elements):
                        self.focus_index = prev
                        break
                    if is_navigable(self.focus_index):
                        break
                self.nav_buffer = "s" if key in [1003, ord('s')] else "S"
                self.clear_nav_on_next = True
                
            elif key in [1000, ord('a'), ord('A')]:
                self.nav_buffer = "a" if key in [1000, ord('a')] else "A"
                self.clear_nav_on_next = True
                
            elif key in [1001, ord('d'), ord('D')]:
                self.nav_buffer = "d" if key in [1001, ord('d')] else "D"
                self.clear_nav_on_next = True
                
            elif ord('1') <= key <= ord('9'):
                # Digit jump (C: do_jump)
                target = key - ord('0')
                for i, el in enumerate(self.elements):
                    if el.interactive_idx == target:
                        self.focus_index = i
                        break
                        
            elif key in [10, 13]:
                self.nav_buffer = ""
                if 0 <= self.focus_index < len(self.elements):
                    el = self.elements[self.focus_index]
                    if el.href:
                        # Navigate to new layout (C: href handling)
                        self.cleanup_module()
                        self.layout_path = el.href
                        self.parse_layout_elements()
                        self.focus_index = 0
                        self.active_index = -1
                        # Initialize focus to first navigable
                        for i in range(len(self.elements)):
                            if is_navigable(i):
                                self.focus_index = i
                                break
                    elif el.type == 'menu':
                        # Open menu (C: menu activation)
                        self.active_index = self.focus_index
                        if el.num_children > 0:
                            el.current_child_focus = 0
                    else:
                        # Activate element (C: element activation)
                        self.active_index = self.focus_index
                        if el.type != 'cli_io' and el.on_click != 'INTERACT':
                            if el.on_click.startswith('LAUNCH:'):
                                self.handle_launch_command(el.on_click)
                                self.active_index = -1
                            elif el.on_click == 'FEED':
                                self.inject_raw_key(ord('2'))
                                self.active_index = -1
                            elif el.on_click == 'PLAY':
                                self.inject_raw_key(ord('3'))
                                self.active_index = -1
                            elif el.on_click == 'SLEEP':
                                self.inject_raw_key(ord('4'))
                                self.active_index = -1
                            elif el.on_click == 'EVOLVE':
                                self.inject_raw_key(ord('5'))
                                self.active_index = -1
                            elif el.on_click == 'END_TURN':
                                self.inject_raw_key(ord('6'))
                                self.active_index = -1
                            else:
                                self.active_index = -1
                                
            elif key in [127, 8]:
                if len(self.nav_buffer) > 0:
                    self.nav_buffer = self.nav_buffer[:-1]
                    
            elif key in [ord('q'), ord('Q')]:
                self.cleanup_module()
                sys.exit(0)
                
        else:
            # Active mode (C: process_key active)
            el = self.elements[self.active_index]
            
            if key == 27:  # ESC
                self.active_index = -1
                
            elif el.type == 'cli_io':
                if key in [10, 13]:
                    # Submit CLI input
                    for ch in el.input_buffer:
                        self.inject_raw_key(ord(ch))
                    self.inject_raw_key(10)
                    el.input_buffer = ""
                    self.active_index = -1
                elif key in [127, 8]:
                    if len(el.input_buffer) > 0:
                        el.input_buffer = el.input_buffer[:-1]
                elif 32 <= key <= 126:
                    if len(el.input_buffer) < 255:
                        el.input_buffer += chr(key)
                        
            elif el.on_click == 'INTERACT':
                self.inject_raw_key(key)
                if 32 <= key <= 126:
                    self.nav_buffer = chr(key)
                else:
                    self.nav_buffer = "[Key]"
                self.clear_nav_on_next = True
        
        self.save_state()

def main():
    initial_layout = path_utils.build_path("pieces/chtpm/layouts/os.chtpm")
    if len(sys.argv) > 1: initial_layout = sys.argv[1]
    parser = PHTPMParser(initial_layout)
    signal.signal(signal.SIGINT, lambda s, f: (parser.cleanup_module(), sys.exit(0)))
    
    pulse_path = path_utils.get_piece_path('display', 'frame_changed.txt')
    view_pulse_path = path_utils.get_piece_path('fuzzpet', 'view_changed.txt')
    if pulse_path and os.path.exists(pulse_path): parser.last_pulse_size = os.path.getsize(pulse_path)
    if view_pulse_path and os.path.exists(view_pulse_path): parser.last_view_pulse_size = os.path.getsize(view_pulse_path)
    
    parser.compose_frame()
    
    keyboard_history = path_utils.get_piece_path('keyboard', 'history.txt')
    while True:
        dirty = False
        if keyboard_history and os.path.exists(keyboard_history):
            curr_size = os.path.getsize(keyboard_history)
            if curr_size < parser.last_history_position: parser.last_history_position = 0
            if curr_size > parser.last_history_position:
                with open(keyboard_history, 'r') as f:
                    f.seek(parser.last_history_position)
                    for line in f:
                        try:
                            key_code = int(line.strip())
                            parser.process_key(key_code)
                            dirty = True
                        except: pass
                    parser.last_history_position = f.tell()
        
        if pulse_path and os.path.exists(pulse_path):
            curr_size = os.path.getsize(pulse_path)
            if curr_size > parser.last_pulse_size:
                with open(pulse_path, 'r') as f:
                    f.seek(parser.last_pulse_size); new_data = f.read()
                parser.last_pulse_size = curr_size
                if 'P' not in new_data: dirty = True
        
        if view_pulse_path and os.path.exists(view_pulse_path):
            curr_size = os.path.getsize(view_pulse_path)
            if curr_size > parser.last_view_pulse_size: parser.last_view_pulse_size = curr_size; dirty = True

        if dirty or parser.clear_nav_on_next: 
            parser.compose_frame()
            if pulse_path and os.path.exists(pulse_path): parser.last_pulse_size = os.path.getsize(pulse_path)
        time.sleep(0.016)

if __name__ == "__main__":
    main()
