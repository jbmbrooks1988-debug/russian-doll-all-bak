"""
Time Plugin
Implements the hybrid time system with manual and automatic advancement modes.
"""

import time
import threading
from datetime import datetime, timedelta


class Plugin:
    def __init__(self, main_app):
        self.main_app = main_app
        self.enabled = True
        self.game_time = {
            'year': 2026,
            'month': 1,
            'day': 1,
            'hour': 0,
            'minute': 0,
            'second': 0
        }
        # Load settings from file at initialization if available
        self.settings = self.load_settings()
        # Ensure defaults are set if settings file doesn't exist
        self.ensure_defaults()
        self.timer_thread = None
        self.running = False
        self.last_player_action_time = time.time()
        self.player_moved_threshold = 1.0  # seconds between player moves that trigger time
        self.time_multiplier = 1.0  # How much game time advances per real second
        self.last_update_time = time.time()

    def ensure_defaults(self):
        """Ensure default settings are present."""
        defaults = {
            "time_advancement_mode": "manual",  # Either "manual" or "automatic"
            "timer_speed": 1.0,               # Speed multiplier for automatic mode
            "reality_field_radius": 10,       # Radius for player-based entity movement
            "show_time_display": True,        # Whether to show time on screen
            "show_reality_field": False       # Whether to visualize reality field
        }
        
        for key, default_value in defaults.items():
            if key not in self.settings:
                self.settings[key] = default_value

    def load_settings(self):
        """Load settings from file."""
        import os
        import json
        settings_file = "data/settings.json"
        if os.path.exists(settings_file):
            try:
                with open(settings_file, 'r') as f:
                    return json.load(f)
            except:
                return {}
        return {}

    def initialize(self, voxel_grid_widget=None):
        """Initialize the time plugin."""
        self.voxel_grid_widget = voxel_grid_widget
        # Load initial settings and start timer based on mode
        if self.settings.get("time_advancement_mode", "manual") == "automatic":
            # Reset time tracking before starting timer
            self.last_update_time = time.time()
            self.start_timer()
            print("Time plugin initialized with AUTOMATIC mode - Timer started")
        else:
            print("Time plugin initialized with MANUAL mode - Timer inactive")
            # Even in manual mode, make sure to initialize the last update time
            self.last_update_time = time.time()
        
    def start_timer(self):
        """Start the timer thread."""
        if self.timer_thread is None or not self.timer_thread.is_alive():
            self.running = True
            self.timer_thread = threading.Thread(target=self._timer_loop, daemon=False)  # Changed to non-daemon
            self.timer_thread.start()
        else:
            # Timer is already running, just ensure running flag is true
            self.running = True

    def stop_timer(self):
        """Stop the timer thread."""
        self.running = False
        # Wait briefly for the thread to stop cleanly, if it exists
        if self.timer_thread and self.timer_thread.is_alive():
            self.timer_thread.join(timeout=0.2)  # Wait up to 200ms for clean shutdown

    def _timer_loop(self):
        """Main timer loop that advances game time."""
        print("[TIMER] Automatic timer loop started")  # Initial debug message
        while self.running:
            try:
                # Double check mode every iteration - make sure we're in automatic mode
                current_mode = self.settings.get("time_advancement_mode", "manual")
                
                if current_mode == "automatic":
                    current_time = time.time()
                    
                    # Calculate time elapsed since last update
                    time_delta = current_time - self.last_update_time
                    
                    # Only proceed if we have real time that has passed
                    if time_delta > 0:
                        # Advance game time based on real time elapsed multiplied by speed
                        game_seconds_to_add = time_delta * self.settings.get("timer_speed", 1.0)
                        
                        # Only advance if we have a meaningful time delta
                        # Scale up the time advancement to make it more visible in-game
                        game_seconds_scaled = game_seconds_to_add * 3600  # 1 real second = 1 hour in game
                        if abs(game_seconds_scaled) > 0.01:  # Only if more than 0.01 game seconds to add
                            old_time = self.get_formatted_time()
                            self.advance_game_time(game_seconds_scaled)
                            new_time = self.get_formatted_time()
                            print(f"[TIMER] Time advanced in automatic mode from {old_time} to {new_time}")  # Debug showing time change
                            
                    # Update the time reference for next calculation
                    self.last_update_time = current_time
                else:
                    # If not in automatic mode, wait longer before checking again
                    time.sleep(0.1)
            except Exception as e:
                print(f"Error in timer loop: {e}")
                time.sleep(0.1)  # Brief pause before continuing
            
            # Small sleep to prevent excessive CPU usage
            time.sleep(0.01)  # Sleep 10ms between iterations

    def on_settings_changed(self, settings):
        """Handle changes to time settings."""
        # Store old mode to detect changes
        old_mode = self.settings.get("time_advancement_mode", "manual")
        self.settings = settings.copy()
        # Update time multiplier based on settings
        self.time_multiplier = self.settings.get("timer_speed", 1.0)
        
        # Determine new mode
        new_mode = self.settings.get("time_advancement_mode", "manual")
        
        print(f"[DEBUG] Settings changed - old_mode: {old_mode}, new_mode: {new_mode}")
        
        # Start/stop timer based on mode change
        if new_mode == "automatic":
            if old_mode != new_mode:  # Mode actually changed
                print("[DEBUG] Stopping timer for mode switch...")
                self.stop_timer()  # Stop any existing timer first
                # Reset the time tracking for fresh start
                self.last_update_time = time.time()
            print("[DEBUG] Starting timer in automatic mode...")
            self.start_timer()
            print("Time advancement mode: AUTOMATIC - Timer started")
        else:  # manual mode
            if old_mode != new_mode:  # Mode actually changed
                print("[DEBUG] Stopping timer for manual mode...")
                self.stop_timer()
            print("Time advancement mode: MANUAL - Timer stopped")

    def player_moved(self):
        """Called when the player moves - advances time in manual mode."""
        current_time = time.time()
        
        # Only trigger time advancement if enough time has passed since last action
        if (current_time - self.last_player_action_time) > self.player_moved_threshold:
            if self.settings.get("time_advancement_mode", "manual") == "manual":
                # Advance time by a fixed amount when player moves in manual mode
                self.advance_game_time(3600)  # Advance by 1 hour (3600 seconds) per entity move in manual mode
            self.last_player_action_time = current_time

    def advance_game_time(self, seconds_to_add):
        """Advance game time by the specified number of seconds."""
        # Add the seconds
        total_seconds = self.game_time['second'] + seconds_to_add
        
        # Handle overflow for seconds
        while total_seconds >= 60:
            total_seconds -= 60
            self.game_time['minute'] += 1
            
        self.game_time['second'] = int(total_seconds)
        
        # Handle overflow for minutes
        while self.game_time['minute'] >= 60:
            self.game_time['minute'] -= 60
            self.game_time['hour'] += 1
            
        # Handle overflow for hours
        while self.game_time['hour'] >= 24:
            self.game_time['hour'] -= 24
            self.game_time['day'] += 1
            
        # Handle overflow for days (simple 30-day months)
        days_in_month = self.get_days_in_month(self.game_time['month'], self.game_time['year'])
        while self.game_time['day'] > days_in_month:
            self.game_time['day'] -= days_in_month
            self.game_time['month'] += 1
            
            if self.game_time['month'] > 12:
                self.game_time['month'] = 1
                self.game_time['year'] += 1
                
            days_in_month = self.get_days_in_month(self.game_time['month'], self.game_time['year'])
        
        # Update the time display to reflect changes
        self.update_time_display()

    def get_days_in_month(self, month, year):
        """Get number of days in a given month and year."""
        if month in [1, 3, 5, 7, 8, 10, 12]:
            return 31
        elif month in [4, 6, 9, 11]:
            return 30
        elif month == 2:
            # Simple leap year check
            if year % 4 == 0 and (year % 100 != 0 or year % 400 == 0):
                return 29
            else:
                return 28
        return 30  # Fallback

    def get_formatted_time(self):
        """Get the current game time in a formatted string."""
        return f"{self.game_time['year']}-{self.game_time['month']:02d}-{self.game_time['day']:02d} {self.game_time['hour']:02d}:{self.game_time['minute']:02d}"

    def activate(self):
        """Activate the plugin."""
        self.enabled = True

    def deactivate(self):
        """Deactivate the plugin."""
        self.enabled = False
        self.stop_timer()
        print("Time plugin deactivated - Timer stopped")

    def get_widget(self):
        """Return the widget for UI integration if applicable."""
        return None

    def handle_event(self, event):
        """Handle events - for capturing player/entity moves."""
        # Check if this is a key event that indicates player/entity movement
        from PySide6.QtCore import QEvent
        
        # Listen for any events that might indicate player movement
        # For now, we'll detect any keyboard input as potential player movement in manual mode
        if hasattr(event, 'type') and event.type() == QEvent.KeyPress:
            # In manual mode, any key press (movement or otherwise) should advance time
            if self.settings.get("time_advancement_mode", "manual") == "manual":
                self.player_moved()
        
        # We don't consume the event, just listen for it
        return False

    def update(self):
        """Update method called regularly by the main app."""
        # Check for connections to other plugins to handle entity movements
        if hasattr(self.main_app, 'plugin_manager') and hasattr(self.main_app.plugin_manager, 'plugin_instances'):
            # Check if voxel grid has entities moved since last check
            if self.voxel_grid_widget and self.settings.get("time_advancement_mode", "manual") == "manual":
                # Check for entity movement by monitoring changes in the grid entities
                # This is a simple approach - in more complex implementation, 
                # other plugins would directly notify the time plugin
                pass

    def get_current_time(self):
        """Return the current game time dictionary."""
        return self.game_time

    def update_time_display(self):
        """Update the time display widget when time changes."""
        # Find and update the menu plugin's time display widget
        if hasattr(self.main_app, 'plugin_manager') and hasattr(self.main_app.plugin_manager, 'plugin_instances'):
            for plugin in self.main_app.plugin_manager.plugin_instances:
                # Check if this is the menu plugin specifically (by checking for known menu plugin attributes)
                # The menu plugin has specific attributes like settings_dialog, time_display_widget, etc.
                if (hasattr(plugin, '__class__') and 
                    plugin.__class__.__name__ == 'Plugin' and  # The menu plugin class name
                    hasattr(plugin, 'update_time_display') and  # Has the method we need
                    hasattr(plugin, 'settings_file') and      # Specific to menu plugin
                    plugin is not self):                      # Exclude self
                    try:
                        time_str = self.get_formatted_time()
                        plugin.update_time_display(time_str)
                        print(f"[DISPLAY] Time display updated to: {time_str}")  # Debug showing GUI update
                    except Exception as e:
                        # Silently handle errors to avoid spam
                        pass

    def entity_moved(self):
        """Called whenever any entity is moved in the world - advances time in manual mode."""
        if self.settings.get("time_advancement_mode", "manual") == "manual":
            print(f"Entity moved in manual mode - Advancing time by 1 hour")  # Debug message
            old_time = self.get_formatted_time()
            self.player_moved()
            new_time = self.get_formatted_time()
            print(f"Time advanced from {old_time} to {new_time}")  # Debug message showing time change


# Alias for plugin system
PluginClass = Plugin