#!/usr/bin/env python3
"""
FuzzPet Module - Sovereign Stage (Python Implementation)
Standardized for PMO v1.4.0 (Option Z & Single Pulse).
Produces view.txt and syncs state.txt mirror for the OS Theater.
"""

import os
import sys
import time
import subprocess
import threading
from datetime import datetime

# Import TPM Utils
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../../locations'))
import path_utils
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../../master_ledger/plugins'))
from piece_manager import piece_manager

# Constants
MAP_ROWS = 10
MAP_COLS = 20

class FuzzPetStage:
    def __init__(self):
        self.state = {
            'name': 'Fuzzball',
            'hunger': 50,
            'happiness': 75,
            'energy': 100,
            'level': 1,
            'pos_x': 5,
            'pos_y': 5,
            'status': 'active'
        }
        self.map = []
        self.current_response_key = "default"
        self.last_key_display = "None"
        self.load_initial_data()

    def load_initial_data(self):
        """MIRROR C VERSION: Load state and map, place pet at correct position."""
        # Load state from DNA (.pdl mirror)
        dna = piece_manager.read_piece_state('fuzzpet')
        if dna: self.state.update(dna)
        
        # Load map - MIRROR C: load_map() clears old '@' and places at initial position
        self.map = piece_manager.load_map()
        
        # Ensure pet '@' is at the correct position from state
        # C version does this in load_map() by reading pos_x/pos_y from state
        pos_x = self.state.get('pos_x', 5)
        pos_y = self.state.get('pos_y', 5)
        
        # Clear any existing '@' from map
        for y in range(len(self.map)):
            if isinstance(self.map[y], list):
                for x in range(len(self.map[y])):
                    if self.map[y][x] == '@':
                        self.map[y][x] = '.'
            else:
                row = list(self.map[y])
                if '@' in row:
                    row[row.index('@')] = '.'
                    self.map[y] = "".join(row)
        
        # Place '@' at current position
        if 0 <= pos_y < len(self.map):
            if isinstance(self.map[pos_y], list):
                if 0 <= pos_x < len(self.map[pos_y]):
                    self.map[pos_y][pos_x] = '@'
            else:
                row = list(self.map[pos_y])
                if 0 <= pos_x < len(row):
                    row[pos_x] = '@'
                self.map[pos_y] = "".join(row)

    def sync_mirror(self):
        """DNA SYNC: Update piece.txt (Audit) and state.txt (High-speed Render)."""
        # 1. Update DNA (.pdl mirror via piece_manager)
        piece_manager.write_piece_state('fuzzpet', self.state)
        
        # 2. Update High-Speed Mirror (state.txt) for Parser
        mirror_path = path_utils.get_piece_path('fuzzpet', 'state.txt')
        if mirror_path:
            with open(mirror_path, 'w') as f:
                for k, v in self.state.items():
                    f.write(f"{k}={v}\n")
                f.write(f"response_key={self.current_response_key}\n")

    def write_sovereign_view(self):
        """MIRROR C VERSION EXACTLY."""
        # 1. Sync the mirror first so others see the data (Parser relies on this!)
        self.sync_mirror()

        # 2. Get Last Response (Audit Reflection) - C uses popen to piece_manager
        response = "..."
        responses = piece_manager.get_piece_responses('fuzzpet')
        if self.current_response_key in responses:
            response = responses[self.current_response_key]

        # 3. Compose Sovereign View (The "Stage") - EXACTLY LIKE C
        view_path = path_utils.get_piece_path('fuzzpet', 'view.txt')
        if not view_path: return
        
        with open(view_path, 'w') as fp:
            for y in range(MAP_ROWS):
                # C: fprintf(fp, "║  %-20s                                     ║\n", map[y]);
                # Handle both list of chars and string
                if y < len(self.map):
                    if isinstance(self.map[y], list):
                        map_line = "".join(self.map[y])
                    else:
                        map_line = self.map[y]
                else:
                    map_line = ""
                fp.write(f"║  {map_line:<57} ║\n")
            
            fp.write("╠═══════════════════════════════════════════════════════════╣\n")
            fp.write(f"║  [FUZZPET]: {response[:45]:<45} ║\n")
            fp.write(f"║  DEBUG [Last Key]: {self.last_key_display:<38} ║\n")
        
        # 4. SIGNALING NODE: Append to local marker to wake up the Parser (Option Z)
        pulse_path = path_utils.get_piece_path('fuzzpet', 'view_changed.txt')
        if pulse_path:
            with open(pulse_path, 'a') as f:
                f.write('X\n')

    def move(self, direction):
        """MIRROR C VERSION: Update map array directly."""
        dx, dy = 0, 0
        if direction == 'w': dy = -1
        elif direction == 's': dy = 1
        elif direction == 'a': dx = -1
        elif direction == 'd': dx = 1
        
        pos_x = self.state['pos_x']
        pos_y = self.state['pos_y']
        nx, ny = pos_x + dx, pos_y + dy
        
        if 0 <= nx < MAP_COLS and 0 <= ny < MAP_ROWS:
            # Check for walls (mirror C)
            if ny < len(self.map) and nx < len(self.map[ny]):
                if self.map[ny][nx] in ['#', 'R']:
                    return
            
            # Check for treasure (mirror C)
            if ny < len(self.map) and nx < len(self.map[ny]):
                if self.map[ny][nx] == 'T':
                    self.state['happiness'] = min(100, self.state['happiness'] + 10)
                    piece_manager.append_to_master_ledger("EventFire", "treasure_found", "fuzzpet")
            
            # Update map - MIRROR C: map[pos_y][pos_x] = '.'; map[pos_y][pos_x] = '@';
            if pos_y < len(self.map):
                row = list(self.map[pos_y])
                if 0 <= pos_x < len(row):
                    row[pos_x] = '.'
                self.map[pos_y] = "".join(row)
            
            self.state['pos_x'] = nx
            self.state['pos_y'] = ny
            
            if ny < len(self.map):
                row = list(self.map[ny])
                if 0 <= nx < len(row):
                    row[nx] = '@'
                self.map[ny] = "".join(row)
            
            # Update stats (mirror C)
            self.state['hunger'] = min(100, self.state['hunger'] + 1)
            self.state['energy'] = max(0, self.state['energy'] - 1)
            self.current_response_key = "moved"
            
            # Save map (mirror C)
            piece_manager.save_map(self.map)
            
            # Tick Clock (via plugins)
            clock_inc_time = path_utils.get_piece_path('clock', 'plugins/increment_time.py')
            clock_inc_turn = path_utils.get_piece_path('clock', 'plugins/increment_turn.py')
            if clock_inc_time:
                subprocess.run([sys.executable, clock_inc_time], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if clock_inc_turn:
                subprocess.run([sys.executable, clock_inc_turn], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            
            piece_manager.append_to_master_ledger("EventFire", "moved", "fuzzpet")

    def process_key(self, key):
        # Debug display
        if 32 <= key <= 126: self.last_key_display = f"{key} ('{chr(key)}')"
        else: self.last_key_display = f"CODE {key}"

        char = chr(key) if 32 <= key <= 126 else ""
        
        if char in ['w', 'W']: self.move('w')
        elif char in ['s', 'S']: self.move('s')
        elif char in ['a', 'A']: self.move('a')
        elif char in ['d', 'D']: self.move('d')
        elif char == '2': # FEED (UI Index 2)
            self.state['hunger'] = max(0, self.state['hunger'] - 20)
            self.current_response_key = "fed"
            piece_manager.append_to_master_ledger("ResponseRequest", "fed", "fuzzpet")
        elif char == '3': # PLAY (UI Index 3)
            self.state['happiness'] = min(100, self.state['happiness'] + 10)
            self.current_response_key = "played"
            piece_manager.append_to_master_ledger("ResponseRequest", "played", "fuzzpet")
        elif char == '4': # SLEEP (UI Index 4)
            self.state['energy'] = min(100, self.state['energy'] + 30)
            self.current_response_key = "slept"
            piece_manager.append_to_master_ledger("ResponseRequest", "slept", "fuzzpet")
        elif char == '5': # EVOLVE
            self.state['level'] += 1
            self.current_response_key = "evolved"
            piece_manager.append_to_master_ledger("ResponseRequest", "evolved", "fuzzpet")
        elif char == '6': # END TURN
            clock_end = path_utils.get_piece_path('clock', 'plugins/end_turn.py')
            if clock_end:
                subprocess.run([sys.executable, clock_end], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.current_response_key = "default"
            piece_manager.append_to_master_ledger("EventFire", "turn_end", "clock")
        else:
            self.current_response_key = "default"

        self.write_sovereign_view()

def watch_input(stage):
    history_path = path_utils.get_piece_path('fuzzpet', 'history.txt')
    if not history_path: return
    
    # Ensure exists
    os.makedirs(os.path.dirname(history_path), exist_ok=True)
    if not os.path.exists(history_path): open(history_path, 'w').close()
    
    last_pos = os.path.getsize(history_path)
    while True:
        try:
            curr_size = os.path.getsize(history_path)
            if curr_size < last_pos: last_pos = 0 # Truncated
            
            if curr_size > last_pos:
                with open(history_path, 'r') as f:
                    f.seek(last_pos)
                    for line in f:
                        if line.strip().isdigit():
                            stage.process_key(int(line.strip()))
                    last_pos = f.tell()
            time.sleep(0.016) # 60 FPS Polling
        except:
            time.sleep(0.1)

if __name__ == "__main__":
    stage = FuzzPetStage()
    # Initial Frame - C calls write_sovereign_view() at startup
    stage.write_sovereign_view()
    print("[fuzzpet] Sovereign Stage active. Monitoring history.txt...", file=sys.stderr)
    watch_input(stage)
