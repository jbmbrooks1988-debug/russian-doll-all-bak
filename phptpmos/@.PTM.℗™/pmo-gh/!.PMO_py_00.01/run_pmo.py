#!/usr/bin/env python3

import subprocess
import os
import sys
import time

def main():
    """Main execution script for the Python TPM application."""
    print("Starting CHTPM+OS Pipeline (Python)...")
    
    # Ensure the current working directory is the base of the project
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)
    
    # Set up location_kvp
    os.makedirs(os.path.join(script_dir, 'pieces/locations'), exist_ok=True)
    with open(os.path.join(script_dir, 'pieces/locations/location_kvp'), 'w') as f:
        f.write(f"project_root={script_dir}\n")
        f.write(f"pieces_dir={script_dir}/pieces\n")
        f.write(f"fuzzpet_app_dir={script_dir}/pieces/apps/fuzzpet_app\n")
        f.write(f"fuzzpet_dir={script_dir}/pieces/apps/fuzzpet_app/fuzzpet\n")
        f.write(f"clock_dir={script_dir}/pieces/apps/fuzzpet_app/clock\n")
        f.write(f"world_dir={script_dir}/pieces/apps/fuzzpet_app/world\n")
        f.write(f"system_dir={script_dir}/pieces/system\n")

    # RESET SESSION STATE (Mandatory for OS start)
    state_path = os.path.join(script_dir, 'pieces/chtpm/state.txt')
    if os.path.exists(state_path): os.remove(state_path)
    
    # CLEAR PULSE AND HISTORY (Sovereignty Check)
    pulse_path = os.path.join(script_dir, 'pieces/display/frame_changed.txt')
    hist_path = os.path.join(script_dir, 'pieces/keyboard/history.txt')
    os.makedirs(os.path.dirname(pulse_path), exist_ok=True)
    os.makedirs(os.path.dirname(hist_path), exist_ok=True)
    open(pulse_path, 'w').close()
    open(hist_path, 'w').close()

    # Launch the orchestrator
    orchestrator_process = subprocess.Popen(
        [sys.executable, "-u", "pieces/system/plugins/orchestrator.py"],
        stdout=sys.stdout,
        stderr=sys.stderr
    )

    try:
        orchestrator_process.wait()
    except KeyboardInterrupt:
        orchestrator_process.terminate()
        orchestrator_process.wait()

if __name__ == "__main__":
    main()
