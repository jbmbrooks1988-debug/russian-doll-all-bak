# This piece monitors the master ledger for events.

import os
import sys
import time

MASTER_LEDGER_PATH = os.path.join(os.path.dirname(__file__), '../../master_ledger/ledger.txt')
MONITOR_LOG_PATH = os.path.join(os.path.dirname(__file__), '../../master_ledger/monitor_log.txt')

def tail_file(filepath):
    """Generator that yields new lines appended to a file."""
    if not os.path.exists(filepath):
        # Log to file instead of terminal to avoid interfering with renderer
        os.makedirs(os.path.dirname(MONITOR_LOG_PATH), exist_ok=True)
        with open(MONITOR_LOG_PATH, 'a') as log:
            log.write(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] Monitor: Ledger file not found at {filepath}. Waiting...\n")
        while not os.path.exists(filepath):
            time.sleep(1)
    
    with open(filepath, 'r') as f:
        f.seek(0, 2) # Go to the end of the file
        while True:
            line = f.readline()
            if not line:
                time.sleep(0.1) # Sleep briefly
                continue
            yield line

def main():
    """Main loop to monitor the master ledger."""
    # Log startup to file to avoid interfering with renderer
    os.makedirs(os.path.dirname(MONITOR_LOG_PATH), exist_ok=True)
    with open(MONITOR_LOG_PATH, 'a') as log:
        log.write(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] Starting master_ledger_monitor piece.\n")
    
    try:
        for line in tail_file(MASTER_LEDGER_PATH):
            # Parse and process the event
            # Log to dedicated file to avoid interfering with renderer
            with open(MONITOR_LOG_PATH, 'a') as log:
                log.write(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] Monitor: Processing event: {line.strip()}\n")
            # In a real system, this would trigger actions based on the event_type
            # For example, if 'StateUpdate', it might update a piece's persisted state file.
            
    except KeyboardInterrupt:
        with open(MONITOR_LOG_PATH, 'a') as log:
            log.write(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] Master ledger monitor piece stopped.\n")
    except Exception as e:
        with open(MONITOR_LOG_PATH, 'a') as log:
            log.write(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] Error in master_ledger_monitor: {e}\n")

if __name__ == "__main__":
    main()
