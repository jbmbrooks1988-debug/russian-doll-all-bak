# This piece is responsible for capturing keyboard input and logging it.
# Standardized for PMO v1.4.0.

import sys
import tty
import termios
import os
import time
from datetime import datetime

# Import TPM Utils
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../locations'))
import path_utils

# Constants
CTRL_KEY = lambda k: ord(k) & 0x1f
ARROW_LEFT = 1000
ARROW_RIGHT = 1001
ARROW_UP = 1002
ARROW_DOWN = 1003

def setup_raw_mode():
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    new_settings = old_settings[:]
    new_settings[0] &= ~(termios.BRKINT | termios.ICRNL | termios.INPCK | termios.ISTRIP | termios.IXON)
    new_settings[2] |= termios.CS8
    new_settings[3] &= ~(termios.ECHO | termios.ICANON | termios.IEXTEN | termios.ISIG)
    new_settings[6][termios.VMIN] = 0
    new_settings[6][termios.VTIME] = 1
    termios.tcsetattr(fd, termios.TCSAFLUSH, new_settings)
    return old_settings

def restore_terminal(settings):
    fd = sys.stdin.fileno()
    termios.tcsetattr(fd, termios.TCSAFLUSH, settings)

def read_key():
    ch = sys.stdin.read(1)
    if not ch: return 0
    if ord(ch) == 27:
        seq1 = sys.stdin.read(1)
        if not seq1: return 27
        seq2 = sys.stdin.read(1)
        if not seq2: return 27
        if seq1 == '[':
            if seq2 == 'A': return ARROW_UP
            elif seq2 == 'B': return ARROW_DOWN
            elif seq2 == 'C': return ARROW_RIGHT
            elif seq2 == 'D': return ARROW_LEFT
        return 27
    return ord(ch)

def main():
    path_utils.init_paths()
    hist_path = path_utils.get_piece_path('keyboard', 'history.txt')
    if not hist_path: return
    
    os.makedirs(os.path.dirname(hist_path), exist_ok=True)
    
    is_tty = sys.stdin.isatty()
    orig = None
    if is_tty: orig = setup_raw_mode()
    
    try:
        while True:
            if is_tty:
                key = read_key()
                if key == CTRL_KEY('c'): break
                if key > 0:
                    with open(hist_path, 'a') as f:
                        f.write(f"{key}\n")
            time.sleep(0.016)
    finally:
        if orig: restore_terminal(orig)

if __name__ == "__main__":
    main()
