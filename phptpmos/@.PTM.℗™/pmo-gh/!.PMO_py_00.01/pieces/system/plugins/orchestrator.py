# This piece is the orchestrator.
# It launches and manages all other pieces as separate processes.

import sys
import subprocess
import time
import os

# Log file path
script_dir = os.path.dirname(__file__)


# --- Pieces to Launch ---
# The order can matter for initialization.
# phtpm_parser should run to create the first frame before renderer starts.
PIECES = [
    'pieces/keyboard/plugins/keyboard.py',
    'pieces/master_ledger/plugins/response_handler.py',
    'pieces/chtpm/plugins/phtpm_parser.py',
    'pieces/display/plugins/renderer.py',
    'pieces/display/plugins/gl_renderer.py',  # GL window renderer
]

def main():
    """Launches all pieces as subprocesses and manages them."""
    # Clear history and ledger files for new session
    try:
        open('pieces/keyboard/history.txt', 'w').close()
        open('pieces/master_ledger/master_ledger.txt', 'w').close()
        open('pieces/master_ledger/ledger.txt', 'w').close()
        open('pieces/display/frame_changed.txt', 'w').close()
        # CLEAR PARSER STATE (force fresh start with passed layout)
        open('pieces/chtpm/state.txt', 'w').close()
    except:
        pass
    
    print("Orchestrator starting...", file=sys.stderr)
    
    processes = []
    try:
        # Launch each piece
        for piece_script in PIECES:
            # We use sys.executable to ensure we use the same python interpreter
            # that is running the orchestrator. -u is for unbuffered output.
            # Allow renderer and keyboard to access terminal for display/input,
            # but redirect other processes
            
            # Launch phtpm_parser FIRST with layout argument
            if "phtpm_parser.py" in piece_script:
                proc = subprocess.Popen([sys.executable, "-u", piece_script, 
                                         "pieces/chtpm/layouts/os.chtpm"])
                processes.append(proc)
                print(f"  -> {piece_script} started (PID: {proc.pid})", file=sys.stderr)
                # Wait for initial frame
                time.sleep(0.5)
            elif "gl_renderer.py" in piece_script:
                # GL renderer needs DISPLAY environment variable
                proc = subprocess.Popen([sys.executable, "-u", piece_script])
                processes.append(proc)
                print(f"  -> {piece_script} started (PID: {proc.pid})", file=sys.stderr)
                time.sleep(0.3)
            elif "renderer.py" in piece_script or "keyboard.py" in piece_script:
                # Renderer and keyboard need to access terminal for display/input
                proc = subprocess.Popen([sys.executable, "-u", piece_script])
                processes.append(proc)
                print(f"  -> {piece_script} started (PID: {proc.pid})", file=sys.stderr)
                # A small delay to let pieces initialize in order
                time.sleep(0.5)
            else:
                # Other processes should not interfere with display/input
                proc = subprocess.Popen([sys.executable, "-u", piece_script],
                                      stdout=subprocess.DEVNULL,
                                      stderr=subprocess.DEVNULL)
                processes.append(proc)
                print(f"  -> {piece_script} started (PID: {proc.pid})", file=sys.stderr)
                # A small delay to let pieces initialize in order
                time.sleep(0.5)

        print("\nAll pieces launched. System is running.", file=sys.stderr)
        print("Press Ctrl+C to stop.\n", file=sys.stderr)

        # Monitor child processes
        while True:
            # Check if any process has terminated
            for proc in processes:
                if proc.poll() is not None:
                    print(f"\nPiece {proc.args} has terminated (exit code: {proc.returncode})", file=sys.stderr)
                    processes.remove(proc)
                    break
            
            if not processes:
                print("\nAll pieces have terminated. Exiting.", file=sys.stderr)
                break
            
            time.sleep(0.1)

    except KeyboardInterrupt:
        print("\n\nTermination signal received. Shutting down...", file=sys.stderr)
        for proc in processes:
            proc.terminate()
        for proc in processes:
            proc.wait()
        print("All pieces terminated. Goodbye!", file=sys.stderr)
    except Exception as e:
        print(f"\nAn error occurred: {e}", file=sys.stderr)
        for proc in processes:
            proc.terminate()
    finally:
        print("Orchestrator finished.", file=sys.stderr)

if __name__ == "__main__":
    main()
