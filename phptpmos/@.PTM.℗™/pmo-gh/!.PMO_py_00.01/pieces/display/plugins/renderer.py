#!/usr/bin/env python3
"""
ASCII Renderer - Console display
Standardized for PMO v1.4.0 (Single Pulse Standard).
Reads from pieces/display/current_frame.txt.
Monitors pieces/display/frame_changed.txt for updates.
"""

import os
import sys
import time

# Import TPM Utils
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../locations'))
import path_utils

def render_display():
    frame_path = path_utils.get_piece_path('display', 'current_frame.txt')
    if frame_path and os.path.exists(frame_path):
        with open(frame_path, 'r', encoding='utf-8') as f:
            print(f.read(), end='')
            sys.stdout.flush()

def main():
    path_utils.init_paths()
    pulse_path = path_utils.get_piece_path('display', 'frame_changed.txt')
    
    last_pulse_size = 0
    if pulse_path and os.path.exists(pulse_path):
        last_pulse_size = os.path.getsize(pulse_path)
    
    render_display()
    
    while True:
        try:
            if pulse_path and os.path.exists(pulse_path):
                curr_size = os.path.getsize(pulse_path)
                if curr_size > last_pulse_size:
                    render_display()
                    last_pulse_size = curr_size
            time.sleep(0.016)
        except KeyboardInterrupt:
            break
        except:
            time.sleep(0.1)

if __name__ == "__main__":
    main()
