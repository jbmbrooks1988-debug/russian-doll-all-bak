"""
NPC Plugin
Implements event-driven AI NPCs (cow and pig) that respond to the time system.
Follows the event-driven architecture as described in event_plugin_arch_idea_v0.0.txt.
"""

import os
import json
from datetime import datetime
from PySide6.QtCore import QTimer
import threading
import time


class NPCPlugin:
    def __init__(self, main_app):
        self.main_app = main_app
        self.enabled = True
        self.voxel_grid_widget = None
        self.npc_entities = {}  # Will hold cow and pig entities
        self.processed_events_file = "data/master_reader.txt"
        self.event_ledger_file = "data/master_ledger.txt"
        self.log_file = "logs/npc_log.txt"
        self.last_processed_line = 0
        
        # Ensure directories exist
        os.makedirs("data", exist_ok=True)
        os.makedirs("logs", exist_ok=True)
        
        # Timer for processing events
        self.event_timer = None
        self.time_plugin = None  # Will be set when connected to the time plugin

    def initialize(self, voxel_grid_widget):
        """Initialize the NPC plugin."""
        self.voxel_grid_widget = voxel_grid_widget
        
        # Load the last processed event line
        self.load_last_processed_event()
        
        # Find and connect to the time plugin
        self.find_time_plugin()
        
        # Start the event processing timer
        self.start_event_processing()
        
        print("NPC Plugin initialized with event-driven architecture")

    def find_time_plugin(self):
        """Find the time plugin to connect to the time system."""
        if hasattr(self.main_app, 'plugin_manager') and hasattr(self.main_app.plugin_manager, 'plugin_instances'):
            for plugin in self.main_app.plugin_manager.plugin_instances:
                if hasattr(plugin, 'get_formatted_time'):
                    self.time_plugin = plugin
                    print("Connected to time plugin")
                    break

    def load_last_processed_event(self):
        """Load the last processed event line number."""
        try:
            if os.path.exists(self.processed_events_file):
                with open(self.processed_events_file, 'r') as f:
                    content = f.read().strip()
                    if content:
                        self.last_processed_line = int(content)
                    else:
                        self.last_processed_line = 0
        except:
            self.last_processed_line = 0

    def save_last_processed_event(self, line_number):
        """Save the last processed event line number."""
        self.last_processed_line = line_number
        with open(self.processed_events_file, 'w') as f:
            f.write(str(line_number))

    def start_event_processing(self):
        """Start the event processing timer."""
        if self.event_timer is None:
            self.event_timer = QTimer()
            self.event_timer.timeout.connect(self.process_events)
            self.event_timer.start(1000)  # Check for new events every second

    def process_events(self):
        """Process new events from the event ledger."""
        try:
            if not os.path.exists(self.event_ledger_file):
                return
                
            with open(self.event_ledger_file, 'r') as f:
                lines = f.readlines()
                
            # Process events that haven't been processed yet
            for i in range(self.last_processed_line, len(lines)):
                line = lines[i].strip()
                if line:
                    self.process_single_event(line)
                
                # Update the last processed line
                self.save_last_processed_event(i + 1)
                
        except Exception as e:
            self.log_message(f"Error processing events: {e}")

    def process_single_event(self, event_line):
        """Process a single event from the ledger."""
        # Parse event line: "Time: YYYY-MM-DD HH:MM:SS | Type: EVENT_TYPE | Entity: ENTITY_ID | Params: x:5,y:0,z:5 | Status: PENDING"
        try:
            # Split by ' | '
            parts = event_line.split(' | ')
            if len(parts) < 4:
                return  # Invalid event format
                
            # Extract components
            time_part = parts[0].replace('Time: ', '')
            type_part = parts[1].replace('Type: ', '')
            entity_part = parts[2].replace('Entity: ', '')
            params_part = parts[3].replace('Params: ', '').split(' | ')[0]  # Extract just parameters
            status_part = parts[3].split(' | ')[1] if ' | ' in parts[3] else 'PENDING'  # Status might be in a separate part
            
            if status_part != 'PENDING':
                return  # Already processed
                
            # Extract entity ID and type
            entity_id = entity_part.strip()
            event_type = type_part.strip()
            
            # Log the event being processed
            self.log_message(f"Processing event: {event_type} for {entity_id}")
            
            # Handle different event types
            if event_type == 'NPC_MOVE':
                self.handle_npc_move(entity_id, params_part)
            elif event_type == 'NPC_INTERACT':
                self.handle_npc_interact(entity_id, params_part)
            elif event_type == 'NPC_STATE_CHANGE':
                self.handle_npc_state_change(entity_id, params_part)
            elif event_type == 'NPC_SPAWN':
                self.handle_npc_spawn(entity_id, params_part)
            else:
                self.log_message(f"Unknown event type: {event_type}")
                
            # Update event status to COMPLETED
            self.update_event_status(time_part, event_type, entity_id, params_part, 'COMPLETED')
            
        except Exception as e:
            self.log_message(f"Error processing event '{event_line}': {e}")
            # Update event status to FAILED
            try:
                parts = event_line.split(' | ')
                if len(parts) >= 4:
                    time_part = parts[0].replace('Time: ', '')
                    type_part = parts[1].replace('Type: ', '')
                    entity_part = parts[2].replace('Entity: ', '')
                    params_part = parts[3].replace('Params: ', '').split(' | ')[0]
                    self.update_event_status(time_part, type_part, entity_part, params_part, 'FAILED')
            except:
                pass

    def handle_npc_move(self, entity_id, params_str):
        """Handle NPC move event."""
        try:
            # Parse parameters: x:5,y:0,z:5
            params = {}
            param_pairs = params_str.split(',')
            for pair in param_pairs:
                if ':' in pair:
                    key, value = pair.split(':', 1)
                    params[key.strip()] = int(value.strip())
            
            new_x = params.get('x', 0)
            new_y = params.get('y', 0) 
            new_z = params.get('z', 0)
            
            # Find the entity in the grid and move it
            old_pos_key = None
            for pos_key, entity_data in self.voxel_grid_widget.entities.items():
                if entity_data.get('id') == entity_id:
                    old_pos_key = pos_key
                    break
            
            if old_pos_key:
                # Remove entity from old position
                entity_data = self.voxel_grid_widget.entities.pop(old_pos_key)
                
                # Update entity position
                entity_data['x'] = new_x
                entity_data['y'] = new_y
                entity_data['z'] = new_z
                
                # Add entity to new position
                new_pos_key = f"{new_x},{new_y},{new_z}"
                self.voxel_grid_widget.entities[new_pos_key] = entity_data
                
                self.log_message(f"{entity_id} moved from {old_pos_key} to {new_pos_key}")
                
                # Trigger grid update
                if hasattr(self.voxel_grid_widget, 'update'):
                    self.voxel_grid_widget.update()
            else:
                self.log_message(f"Entity {entity_id} not found in grid for move operation")
                
        except Exception as e:
            self.log_message(f"Error handling NPC move for {entity_id}: {e}")

    def handle_npc_interact(self, entity_id, params_str):
        """Handle NPC interaction event."""
        try:
            # Parse parameters - might include target, action type, etc.
            self.log_message(f"{entity_id} performing interaction with params: {params_str}")
            
            # For now, just log the interaction
            # In the future, this could trigger more complex interactions
        except Exception as e:
            self.log_message(f"Error handling NPC interaction for {entity_id}: {e}")

    def handle_npc_state_change(self, entity_id, params_str):
        """Handle NPC state change event."""
        try:
            # Parse the new state from parameters
            state = params_str  # Might be just the state name
            self.log_message(f"{entity_id} state changed to: {state}")
            
            # This could change the behavior pattern of the NPC
        except Exception as e:
            self.log_message(f"Error handling NPC state change for {entity_id}: {e}")

    def handle_npc_spawn(self, entity_id, params_str):
        """Handle NPC spawn event."""
        try:
            # Parse parameters to determine where to spawn
            params = {}
            if ',' in params_str:
                param_pairs = params_str.split(',')
                for pair in param_pairs:
                    if ':' in pair:
                        key, value = pair.split(':', 1)
                        params[key.strip()] = int(value.strip())
            else:
                # Default position if no params provided
                params = {'x': 0, 'y': 0, 'z': 0}
            
            x = params.get('x', 0)
            y = params.get('y', 0)
            z = params.get('z', 0)
            
            # Spawn the entity by adding it to the grid
            pos_key = f"{x},{y},{z}"
            if pos_key not in self.voxel_grid_widget.entities:
                # Create a new entity based on the entity_id (should be cow or pig)
                color = 'brown' if 'cow' in entity_id.lower() else 'pink'
                
                self.voxel_grid_widget.entities[pos_key] = {
                    'id': entity_id,
                    'obj_type': 'emoji_entity',
                    'x': x,
                    'y': y,
                    'z': z,
                    'color': color,
                    'voxel_model': []  # This would be loaded from the emoji system
                }
                
                self.log_message(f"Spawned {entity_id} at {pos_key}")
                
                # Trigger grid update
                if hasattr(self.voxel_grid_widget, 'update'):
                    self.voxel_grid_widget.update()
            else:
                self.log_message(f"Cannot spawn {entity_id} at {pos_key}, position occupied")
                
        except Exception as e:
            self.log_message(f"Error handling NPC spawn for {entity_id}: {e}")

    def update_event_status(self, time_str, event_type, entity_id, params, new_status):
        """Update the status of an event in the ledger."""
        try:
            # Read the ledger file
            if not os.path.exists(self.event_ledger_file):
                return
                
            with open(self.event_ledger_file, 'r') as f:
                lines = f.readlines()
            
            # Find the matching event and update its status
            for i, line in enumerate(lines):
                if (time_str in line and event_type in line and 
                    entity_id in line and params in line):
                    # Create the updated line with new status
                    updated_line = f"Time: {time_str} | Type: {event_type} | Entity: {entity_id} | Params: {params} | Status: {new_status}\n"
                    lines[i] = updated_line
                    break
            
            # Write the updated content back to file
            with open(self.event_ledger_file, 'w') as f:
                f.writelines(lines)
                
        except Exception as e:
            self.log_message(f"Error updating event status: {e}")

    def schedule_npc_action(self, entity_id, action_type, params, delay_minutes=0):
        """Schedule an NPC action by writing an event to the ledger."""
        try:
            # Get current time from time plugin if available, otherwise use system time
            if self.time_plugin:
                # Use the time plugin's format
                current_time = self.time_plugin.get_formatted_time()
                # Format it to include seconds
                # Assuming time_plugin gives us format like "2026-01-01 00:00"
                # Add seconds to make it consistent with expected format
                current_time += ":00"
            else:
                current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            
            # Create the event entry
            event_entry = f"Time: {current_time} | Type: {action_type} | Entity: {entity_id} | Params: {params} | Status: PENDING\n"
            
            # Append to the event ledger
            with open(self.event_ledger_file, 'a') as f:
                f.write(event_entry)
                
            self.log_message(f"Scheduled {action_type} for {entity_id} with params {params}")
            
        except Exception as e:
            self.log_message(f"Error scheduling NPC action: {e}")

    def log_message(self, message):
        """Log a message to the NPC log file."""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_entry = f"{timestamp} - {message}\n"
        
        with open(self.log_file, 'a') as f:
            f.write(log_entry)

    def daily_npc_decisions(self):
        """Make daily decisions for NPCs based on time system."""
        # This would be called daily by the time system
        # Generate random movements, interactions, etc.
        for entity_id in ['cow', 'pig']:
            # Check if entity exists in the game world
            entity_exists = False
            for pos_key, entity_data in self.voxel_grid_widget.entities.items():
                if entity_data.get('id') == entity_id:
                    entity_exists = True
                    current_x = entity_data['x']
                    current_y = entity_data['y']
                    current_z = entity_data['z']
                    break
            
            if entity_exists:
                # Generate a random movement nearby
                import random
                new_x = current_x + random.randint(-2, 2)
                new_z = current_z + random.randint(-2, 2)
                # Keep Y at ground level (0)
                new_y = 0
                
                # Schedule the move event
                params = f"x:{new_x},y:{new_y},z:{new_z}"
                self.schedule_npc_action(entity_id, 'NPC_MOVE', params)

    def hourly_npc_actions(self):
        """Perform hourly actions for NPCs."""
        # This would be called hourly by the time system
        # Could include simple movements or state checks
        for entity_id in ['cow', 'pig']:
            # Find the entity and possibly trigger a minor action
            for pos_key, entity_data in self.voxel_grid_widget.entities.items():
                if entity_data.get('id') == entity_id:
                    # Log the hourly check
                    self.log_message(f"Hourly check for {entity_id} at position {pos_key}")
                    break

    def handle_event(self, event):
        """Handle events such as time-based triggers."""
        # This method will be called by the main app when events occur
        return False  # Don't consume events, just listen for time-based triggers

    def on_settings_changed(self, settings):
        """Handle time system settings changes."""
        # Adjust behavior based on time advancement mode
        pass

    def activate(self):
        """Activate the NPC plugin."""
        self.enabled = True

    def deactivate(self):
        """Deactivate the NPC plugin."""
        self.enabled = False
        if self.event_timer:
            self.event_timer.stop()

    def get_widget(self):
        """Return the widget for UI integration if applicable."""
        return None


# Export the plugin class
Plugin = NPCPlugin