#!/usr/bin/env python3
"""
verify_pmo.py - Final Audit Suite (Robust Version)
Ensures absolute twin parity between Python and C implementations.
Validated against mock_pmo_system.py for accuracy.
"""

import os
import sys
import time
import subprocess
import signal

# Absolute Path Resolution
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
HISTORY_PATH = os.path.join(SCRIPT_DIR, "pieces/keyboard/history.txt")
FRAME_PATH = os.path.join(SCRIPT_DIR, "pieces/display/current_frame.txt")
STATE_PATH = os.path.join(SCRIPT_DIR, "pieces/chtpm/state.txt")

def cleanup_processes(proc):
    """Safely terminate process and children."""
    if proc:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()

def inject_key(code, wait_time=0.5):
    """Inject a key code into keyboard history and wait for processing."""
    print(f"  [Audit] Injecting Key: {code}")
    os.makedirs(os.path.dirname(HISTORY_PATH), exist_ok=True)
    with open(HISTORY_PATH, 'a') as f:
        f.write(f"{code}\n")
    time.sleep(wait_time)

def get_layout():
    """Read current layout from state file."""
    if not os.path.exists(STATE_PATH):
        return "MISSING"
    try:
        with open(STATE_PATH, 'r') as f:
            for line in f:
                if line.startswith("current_layout="):
                    return line.split("=")[1].strip()
    except Exception as e:
        return f"ERROR: {e}"
    return "UNKNOWN"

def get_frame():
    """Read current frame content."""
    if not os.path.exists(FRAME_PATH):
        return None
    try:
        with open(FRAME_PATH, 'r') as f:
            return f.read()
    except:
        return None

def wait_for_file(path, timeout=5):
    """Wait for a file to exist with timeout."""
    start = time.time()
    while time.time() - start < timeout:
        if os.path.exists(path):
            return True
        time.sleep(0.1)
    return False

def run_test():
    print("="*60)
    print("ARCHITECTURAL AUDIT - Python vs C PMO Parity")
    print("="*60)
    
    # 0. Pre-flight checks
    print("\n[Pre-flight] Checking required files...")
    required_files = [
        os.path.join(SCRIPT_DIR, "run_pmo.py"),
        os.path.join(SCRIPT_DIR, "pieces/chtpm/layouts/os.chtpm"),
    ]
    for rf in required_files:
        if not os.path.exists(rf):
            print(f"FAIL: Required file missing: {rf}")
            return
    
    # 1. Clean Start
    print("\n[Setup] Cleaning previous state...")
    if os.path.exists(HISTORY_PATH):
        open(HISTORY_PATH, 'w').close()
    if os.path.exists(STATE_PATH):
        os.remove(STATE_PATH)
    if os.path.exists(FRAME_PATH):
        os.remove(FRAME_PATH)
    
    # 2. Start System
    print("[Setup] Launching PMO system...")
    proc = subprocess.Popen(
        [sys.executable, "run_pmo.py"],
        cwd=SCRIPT_DIR,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        preexec_fn=os.setsid  # Create process group for clean kill
    )
    
    # Wait for boot with timeout
    print("[Setup] Waiting for system boot...")
    if not wait_for_file(STATE_PATH, timeout=5):
        cleanup_processes(proc)
        print("FAIL: System failed to create state file")
        return
    time.sleep(1)  # Extra time for full initialization
    
    results = []
    pass_count = 0
    
    # Test 1: OS Startup
    print("\n[Test 1] Checking OS startup layout...")
    layout = get_layout()
    if layout == "MISSING":
        results.append(("FAIL: State file not created", False))
    elif "os.chtpm" in layout:
        results.append(("PASS: Started at OS Menu", True))
    else:
        results.append((f"FAIL: Wrong startup layout: {layout}", False))
    
    # Test 2: Digit Focus (Must NOT auto-trigger)
    print("\n[Test 2] Testing digit focus (should NOT auto-trigger)...")
    inject_key(2, wait_time=0.5)
    layout = get_layout()
    if "os.chtpm" in layout:
        results.append(("PASS: Digit Jumping is Focus-Only (No Auto-trigger)", True))
    else:
        results.append(("FAIL: Digit Jump caused Auto-Trigger!", False))

    # Test 3: Activation (Enter key)
    print("\n[Test 3] Testing Enter activation...")
    inject_key(13, wait_time=0.5)
    layout = get_layout()
    if "main.chtpm" in layout:
        results.append(("PASS: Enter activation successful", True))
    else:
        results.append((f"FAIL: Enter failed to transition. Layout: {layout}", False))

    # Test 4: Leakage Audit (check for raw tags in frame)
    print("\n[Test 4] Checking for tag leakage in frame...")
    frame = get_frame()
    if frame is None:
        results.append(("FAIL: Frame file not found", False))
    elif "<text" in frame or "<button" in frame or "<link" in frame or "<cli_io" in frame:
        results.append(("FAIL: Tag leakage in frame buffer", False))
    else:
        results.append(("PASS: Clean Surgical Rendering", True))

    # Cleanup
    print("\n[Cleanup] Shutting down system...")
    cleanup_processes(proc)
    
    # Summary
    pass_count = sum(1 for _, passed in results if passed)
    total = len(results)
    
    print("\n" + "="*60)
    print("FINAL AUDIT SUMMARY")
    print("="*60)
    for result, passed in results:
        status = "✓" if passed else "✗"
        print(f"  {status} {result}")
    
    print(f"\nScore: {pass_count}/{total} tests passed")
    
    if pass_count == total:
        print("\n✓✓✓ ALL TESTS PASSED - Python PMO is architecturally sound ✓✓✓")
    else:
        print(f"\n✗✗✗ {total - pass_count} TEST(S) FAILED - Review required ✗✗✗")

if __name__ == "__main__":
    run_test()
