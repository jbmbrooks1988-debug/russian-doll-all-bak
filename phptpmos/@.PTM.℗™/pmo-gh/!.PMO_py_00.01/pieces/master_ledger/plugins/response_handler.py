#!/usr/bin/env python3
"""
Module for handling responses and events between pieces.
Monitors for ResponseRequest events in the master ledger and triggers appropriate responses.
Mirrors the functionality of the C version response_handler.c.
"""

import os
import sys
import time
import subprocess
from piece_manager import piece_manager


class ResponseHandler:
    def __init__(self):
        """Initialize the ResponseHandler."""
        self.master_ledger_path = piece_manager.get_path_for_piece('master_ledger', 'master_ledger.txt')
        self.last_position = 0
        self.running = True
        
        # Ensure the master ledger directory and file exist
        os.makedirs(os.path.dirname(self.master_ledger_path), exist_ok=True)
        if not os.path.exists(self.master_ledger_path):
            with open(self.master_ledger_path, 'a') as f:
                f.close()  # Create file if it doesn't exist
    
    def monitor_responses(self):
        """Monitor for ResponseRequest events from pieces."""
        print("Response Handler Service Started")
        print("Monitoring pieces/master_ledger/master_ledger.txt for response requests...")
        print("Will trigger responses when ResponseRequest events are detected.")
        print("Press Ctrl+C to stop response handler.\n")
        
        print("Response handler is running and monitoring for events...\n")
        
        while self.running:
            try:
                # Read new lines from the master ledger
                with open(self.master_ledger_path, 'r') as fp:
                    fp.seek(self.last_position, 0)
                    
                    for line in fp:
                        # Update last position
                        self.last_position = fp.tell()
                        
                        # Check if this is a response request
                        if "ResponseRequest: " in line:
                            self.handle_response_request(line.strip())
                            
                # Small delay before checking again
                time.sleep(0.1)  # 100ms delay, matching the C version
                
            except KeyboardInterrupt:
                print("\nResponseHandler interrupted.", file=sys.stderr)
                self.running = False
            except Exception as e:
                print(f"Error in response handler: {e}", file=sys.stderr)
                time.sleep(0.1)  # Brief pause before continuing
    
    def handle_response_request(self, line):
        """Handle a ResponseRequest event from the master ledger."""
        # Extract the event info from the line
        # Line format: [timestamp] ResponseRequest: event_type on piece_name | Source: source_info
        original_request_part = line.find("ResponseRequest: ")
        if original_request_part != -1:
            request_start = line[original_request_part + 17:]  # Skip "ResponseRequest: " (17 chars: ResponseRequest: + space)
            
            # Find the event type (before " on ")
            on_pos = request_start.find(" on ")
            if on_pos != -1:
                event_type = request_start[:on_pos]
                
                # Extract piece name (after " on " until " |")
                pipe_pos = request_start.find(" |", on_pos)
                if pipe_pos != -1:
                    piece_name = request_start[on_pos + 4:pipe_pos]  # Skip " on " (4 chars)
                    
                    # Call the equivalent of piece manager to get the appropriate response
                    response = self.get_response_from_piece_manager(piece_name, event_type)
                    
                    if response:
                        # Print the response to the console for user feedback
                        print(f"\n[FUZZPET RESPONSE] {response}")
                        
                        # Log the response to the master ledger
                        piece_manager.append_to_master_ledger(
                            'ResponseTriggered', 
                            f'{event_type} on {piece_name} | Message: {response}', 
                            'response_handler'
                        )
    
    def get_response_from_piece_manager(self, piece_name, event_type):
        """Call the piece manager to get an appropriate response (simulated)."""
        # Since Python doesn't have the C piece_manager executable, we simulate this functionality
        # In a full implementation, this might call a Python-based piece manager or use direct module calls
        
        # Simulate getting a response based on piece and event type
        responses = {
            ('fuzzpet', 'hungry'): 'Pet is hungry, consider feeding.',
            ('fuzzpet', 'tired'): 'Pet wants to sleep.',
            ('fuzzpet', 'happy'): 'Pet is happy!',
            ('fuzzpet', 'sad'): 'Pet needs attention.',
            ('game_logic', 'error'): 'Game logic reported an error.',
            ('display', 'update'): 'Display updated successfully.',
        }
        
        response = responses.get((piece_name.lower(), event_type.lower()))
        if not response:
            response = f"Response for {event_type} on {piece_name}."
        
        return response

    def stop(self):
        """Stop the response handler."""
        self.running = False


def main():
    """Main function for the response handler piece."""
    print("Starting Response Handler piece.", file=sys.stderr)
    handler = ResponseHandler()
    
    try:
        handler.monitor_responses()
    except KeyboardInterrupt:
        print("\nResponse Handler piece stopped.", file=sys.stderr)
    finally:
        handler.stop()


if __name__ == "__main__":
    main()