#!/usr/bin/env python3
"""
Module for managing file-based inter-process communication (IPC) between pieces.
This centralizes file operations for piece state management and communication.
Standardized for PMO v1.4.0 (Single Pulse Standard).
"""

import os
import sys
import time

# Import path_utils
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../locations'))
import path_utils


class PieceManager:
    """Manages file-based communication between different pieces."""
    
    def __init__(self):
        """Initialize the PieceManager."""
        path_utils.init_paths()
    
    def get_path_for_piece(self, piece_name, filename):
        """Generate the appropriate file path for a given piece (TPM-WISE)."""
        return path_utils.get_piece_path(piece_name, filename)
    
    def trigger_pulse(self, type_char='S'):
        """SIGNALING NODE: Append to universal pulse (KISS Protocol)."""
        pulse_path = self.get_path_for_piece('display', 'frame_changed.txt')
        if not pulse_path: return
            
        os.makedirs(os.path.dirname(pulse_path), exist_ok=True)
        try:
            with open(pulse_path, 'a') as f:
                f.write(f"{type_char}\n")
        except: pass

    def get_piece_responses(self, piece_name):
        """Get responses dictionary from a piece's .pdl file (DNA Source of Truth).
        MIRROR C VERSION: C reads responses from fuzzpet.pdl, not piece.txt."""
        # Try PDL first (DNA source of truth, like C version)
        pdl_path = self.get_path_for_piece(piece_name, f'{piece_name}.pdl')
        if pdl_path and os.path.exists(pdl_path):
            responses = {}
            try:
                with open(pdl_path, 'r') as f:
                    for line in f:
                        if line.startswith('RESPONSE'):
                            # Parse: RESPONSE     | fed                | Yum yum~...
                            parts = line.split('|')
                            if len(parts) >= 3:
                                key = parts[1].strip()
                                value = parts[2].strip()
                                responses[key] = value
                return responses
            except Exception as e:
                print(f"get_piece_responses PDL error: {e}", file=sys.stderr)
        
        # Fallback to piece.txt if PDL doesn't exist
        state_path = self.get_path_for_piece(piece_name, 'piece.txt')
        if not state_path or not os.path.exists(state_path): return {}
        
        responses = {}
        try:
            with open(state_path, 'r') as f:
                in_responses = False
                for line in f:
                    line_stripped = line.strip()
                    if line_stripped.startswith('responses:'):
                        in_responses = True
                        continue
                    if in_responses:
                        if line_stripped and not line.startswith(' ') and not line.startswith('\t'):
                            break
                        if ':' in line_stripped and not line_stripped.startswith('#'):
                            colon_pos = line_stripped.find(':')
                            key = line_stripped[:colon_pos].strip()
                            value = line_stripped[colon_pos + 1:].strip()
                            if key and value:
                                responses[key] = value
        except Exception as e:
            print(f"get_piece_responses piece.txt error: {e}", file=sys.stderr)
        return responses

    def read_piece_state(self, piece_name):
        """Read state from a piece's state file (DNA Mirror)."""
        state_path = self.get_path_for_piece(piece_name, 'piece.txt')
        if not state_path or not os.path.exists(state_path): return {}
        
        state_data = {}
        try:
            with open(state_path, 'r') as f:
                for line in f:
                    line = line.strip()
                    if '=' in line:
                        key, value = line.split('=', 1)
                        key, value = key.strip(), value.strip()
                        if key in ['pos_x', 'pos_y', 'level', 'hunger', 'happiness', 'energy']:
                            try: state_data[key] = int(value)
                            except: state_data[key] = value
                        else: state_data[key] = value
        except: pass
        return state_data
    
    def write_piece_state(self, piece_name, state_data):
        """Write state to piece.txt, preserving responses and other sections (mirror C behavior)."""
        state_path = self.get_path_for_piece(piece_name, 'piece.txt')
        pdl_path = self.get_path_for_piece(piece_name, f'{piece_name}.pdl')
        if not state_path: return False
        
        # Build responses section from PDL if available, else preserve existing
        responses_section = ""
        if pdl_path and os.path.exists(pdl_path):
            # Read responses from PDL (DNA is source of truth)
            responses = {}
            with open(pdl_path, 'r') as f:
                for line in f:
                    if line.startswith('RESPONSE'):
                        parts = line.split('|')
                        if len(parts) >= 3:
                            key = parts[1].strip()
                            value = parts[2].strip()
                            responses[key] = value
            # Format responses section
            responses_section = "responses:\n"
            for key, value in responses.items():
                responses_section += f"  {key}: {value}\n"
        elif os.path.exists(state_path):
            # Preserve existing responses from piece.txt
            in_responses = False
            responses_lines = []
            with open(state_path, 'r') as f:
                for line in f:
                    if line.strip().startswith('responses:'):
                        in_responses = True
                        continue
                    if in_responses:
                        if line.strip() and not line.startswith(' '):
                            break
                        responses_lines.append(line)
            if responses_lines:
                responses_section = "responses:\n" + "".join(responses_lines)
        
        existing_lines = []
        in_state_section = False
        if os.path.exists(state_path):
            with open(state_path, 'r') as f: existing_lines = f.readlines()
        
        os.makedirs(os.path.dirname(state_path), exist_ok=True)
        try:
            with open(state_path, 'w') as f:
                wrote_state = False
                wrote_responses = False
                for line in existing_lines:
                    line_stripped = line.strip()
                    if line_stripped.startswith('state:'):
                        in_state_section = True
                        wrote_state = True
                        f.write('state:\n')
                        for key, value in state_data.items(): f.write(f'  {key}={value}\n')
                        continue
                    if in_state_section:
                        # Exit state section when we hit a non-indented non-empty line
                        if line_stripped and not line.startswith(' ') and not line.startswith('\t'):
                            in_state_section = False
                        else:
                            continue  # Skip indented lines (state contents)
                    if line_stripped.startswith('responses:'):
                        # Replace with our responses section
                        wrote_responses = True
                        f.write(responses_section)
                        # Skip existing response lines
                        continue
                    if line.startswith('  ') and 'responses:' in str(existing_lines[max(0,existing_lines.index(line)-1):]):
                        continue  # Skip old response lines
                    f.write(line)
                
                if not wrote_state:
                    f.write('state:\n')
                    for key, value in state_data.items(): f.write(f'  {key}={value}\n')
                if not wrote_responses and responses_section:
                    f.write(responses_section)
                    
            self.trigger_pulse('S')
            return True
        except Exception as e:
            print(f"write_piece_state error: {e}", file=sys.stderr)
            return False
    
    def load_map(self):
        map_path = self.get_path_for_piece('world', 'map.txt')
        if not map_path or not os.path.exists(map_path): self.create_default_map(); map_path = self.get_path_for_piece('world', 'map.txt')
        map_grid = []
        try:
            with open(map_path, 'r') as f: map_grid = [list(line.strip()) for line in f.readlines()]
        except: pass
        return map_grid
    
    def save_map(self, map_grid):
        map_path = self.get_path_for_piece('world', 'map.txt')
        if not map_path: return False
        os.makedirs(os.path.dirname(map_path), exist_ok=True)
        try:
            with open(map_path, 'w') as f:
                for row in map_grid: f.write("".join(row) + "\n")
            return True
        except: return False
    
    def create_default_map(self):
        map_path = self.get_path_for_piece('world', 'map.txt')
        if not map_path: return
        os.makedirs(os.path.dirname(map_path), exist_ok=True)
        default_map = ["##########", "#        #", "#        #", "#        #", "#   @    #", "#        #", "#        #", "#        #", "#        #", "##########"]
        with open(map_path, 'w') as f: f.write("\n".join(default_map))
    
    def save_frame(self, content):
        frame_path = self.get_path_for_piece('display', 'current_frame.txt')
        if not frame_path: return False
        os.makedirs(os.path.dirname(frame_path), exist_ok=True)
        try:
            with open(frame_path, 'w') as f: f.write(content)
            return True
        except: return False
    
    def append_to_master_ledger(self, event_type, description, source='piece_manager'):
        ledger_path = self.get_path_for_piece('master_ledger', 'ledger.txt')
        if not ledger_path: return False
        os.makedirs(os.path.dirname(ledger_path), exist_ok=True)
        timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
        log_entry = f"[{timestamp}] {event_type}: {description} | Source: {source}\n"
        try:
            with open(ledger_path, 'a') as ledger: ledger.write(log_entry)
            return True
        except: return False

# Global instance
piece_manager = PieceManager()
