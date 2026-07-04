"""
Core Selector Plugin
Handles selector movement and entity selection with correct coordinate system
"""

from PySide6.QtCore import Qt, QEvent
from PySide6.QtGui import QKeyEvent
import math


class SelectorPlugin:
    """Plugin for handling selector movement and entity selection with correct coordinates"""
    def __init__(self, main_app):
        self.main_app = main_app
        self.voxel_grid = None
        self.time_plugin = None  # Reference to time plugin for notifications
        self.selector_pos = [0, 0, 0]  # [x, y, z] using reference coordinate system
    
    def initialize(self, voxel_grid_widget):
        """Initialize the selector plugin with reference to voxel grid"""
        self.voxel_grid = voxel_grid_widget
        if self.voxel_grid:
            self.selector_pos = self.voxel_grid.selector_pos[:]  # Sync with grid's selector
            # Ensure grid has selection attributes
            if not hasattr(self.voxel_grid, 'is_selected'):
                self.voxel_grid.is_selected = False
            if not hasattr(self.voxel_grid, 'selected_entity_data'):
                self.voxel_grid.selected_entity_data = None
            if not hasattr(self.voxel_grid, 'selected_entity_key'):
                self.voxel_grid.selected_entity_key = None

            print("Selector Plugin initialized with correct coordinate system")
            # Find and store reference to time plugin for notifications
            # Delay finding the plugin to ensure all plugins are loaded
            import threading
            threading.Timer(1.0, self.find_time_plugin).start()  # Check for time plugin after a delay
            
            return True
        return False

    def find_time_plugin(self):
        """Find and store a reference to the time plugin."""
        print(f"[SELECTOR] Looking for time plugin...")  # Debug
        if hasattr(self.main_app, 'plugin_manager') and hasattr(self.main_app.plugin_manager, 'plugin_instances'):
            for plugin in self.main_app.plugin_manager.plugin_instances:
                print(f"[SELECTOR] Checking plugin: {plugin.__class__.__name__}")  # Debug
                if hasattr(plugin, 'get_formatted_time'):  # Time plugin has this method
                    self.time_plugin = plugin
                    print(f"[SELECTOR] Found and stored reference to time plugin")  # Debug
                    break
            if not self.time_plugin:
                print(f"[SELECTOR] Could not find time plugin")  # Debug
    
    def handle_event(self, event):
        """Handle events, specifically key presses for the selector."""
        if not isinstance(event, QKeyEvent) or event.type() != QEvent.KeyPress:
            return False
        
        return self.handle_key_event(event.key())

    def handle_key_event(self, key):
        """Handle keyboard input for selector movement using proper coordinate system"""
        if not self.voxel_grid:
            return False
            
        old_pos = self.selector_pos[:]
        new_pos = self.selector_pos[:]
        
        # Use reference coordinate system: X+ is right, Y+ is up, Z+ is back
        if key == Qt.Key_Right:
            new_pos[0] = min(new_pos[0] + 1, self.voxel_grid.grid_size - 1)  # X+ (right)
        elif key == Qt.Key_Left:
            new_pos[0] = max(new_pos[0] - 1, 0)  # X- (left)
        elif key == ord('X') or key == ord('x'):  # X key moves Y+ (up)
            new_pos[1] = min(new_pos[1] + 1, self.voxel_grid.grid_size - 1)  # Y+ (up)
        elif key == ord('Z') or key == ord('z'):  # Z key moves Y- (down)
            new_pos[1] = max(new_pos[1] - 1, 0)  # Y- (down)
        elif key == Qt.Key_Up:  # Z- (forward, toward viewer)
            new_pos[2] = max(new_pos[2] - 1, 0)
        elif key == Qt.Key_Down:  # Z+ (back, away from viewer)
            new_pos[2] = min(new_pos[2] + 1, self.voxel_grid.grid_size - 1)
        elif key == Qt.Key_Return or key == Qt.Key_Enter:
            return self._handle_selection()
        else:
            return False  # Key not handled by selector
        
        if new_pos != old_pos:
            self.selector_pos = new_pos
            self.voxel_grid.set_selector_position(*new_pos)
            # Update UI elements to reflect new position
            if hasattr(self.voxel_grid, 'update_ui_elements'):
                self.voxel_grid.update_ui_elements()
            return True
        
        return False  # Position didn't change
    
    def _handle_selection(self):
        """Handle entity selection/deselection"""
        if not self.voxel_grid:
            return False
            
        # Create position key in format "x,y,z"
        pos_key = f"{self.selector_pos[0]},{self.selector_pos[1]},{self.selector_pos[2]}"
        
        if not self.voxel_grid.is_selected:
            # Attempt to select an entity at this position
            if pos_key in self.voxel_grid.entities:
                self.voxel_grid.is_selected = True
                self.voxel_grid.selected_entity_key = pos_key
                self.voxel_grid.selected_entity_data = self.voxel_grid.entities.pop(pos_key)
                print(f"Selected entity at {pos_key}")
        else:
            # Deselect entity - place it back or at new location
            if pos_key in self.voxel_grid.entities:
                # Position is occupied, put entity back to original position
                self.voxel_grid.entities[self.voxel_grid.selected_entity_key] = self.voxel_grid.selected_entity_data
            else:
                # Position is free, place entity there
                self.voxel_grid.entities[pos_key] = {
                    **self.voxel_grid.selected_entity_data,
                    'x': self.selector_pos[0],
                    'y': self.selector_pos[1],
                    'z': self.selector_pos[2]
                }
            
            self.voxel_grid.is_selected = False
            self.voxel_grid.selected_entity_key = None
            self.voxel_grid.selected_entity_data = None
            print(f"Deselected entity, placed at {pos_key}")
        
        # Update the grid and UI elements
        self.voxel_grid.update()
        if hasattr(self.voxel_grid, 'update_ui_elements'):
            self.voxel_grid.update_ui_elements()
        
        # Notify the time plugin that an entity has been moved (in manual mode)
        if self.time_plugin and hasattr(self.time_plugin, 'entity_moved'):
            print(f"[SELECTOR] Notifying time plugin of entity movement")  # Debug print
            self.time_plugin.entity_moved()
        
        return True
    
    def get_selector_position(self):
        """Get current selector position [x, y, z]"""
        return self.selector_pos[:]
    
    def set_selector_position(self, x, y, z):
        """Set selector position with validation"""
        self.selector_pos[0] = max(0, min(x, self.voxel_grid.grid_size - 1))
        self.selector_pos[1] = max(0, min(y, self.voxel_grid.grid_size - 1))
        self.selector_pos[2] = max(0, min(z, self.voxel_grid.grid_size - 1))
        
        if self.voxel_grid:
            self.voxel_grid.set_selector_position(*self.selector_pos)

Plugin = SelectorPlugin