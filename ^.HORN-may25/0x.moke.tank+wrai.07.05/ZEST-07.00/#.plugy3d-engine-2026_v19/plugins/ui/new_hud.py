"""
New HUD Plugin
A completely different approach using an independent overlay widget
"""

from PySide6.QtWidgets import QWidget, QLabel, QVBoxLayout, QHBoxLayout, QTextEdit
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QColor, QFont, QPainter, QPalette, QPen


class HUDDisplayWidget(QWidget):
    """Independent HUD widget that overlays on top of the 3D view"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(Qt.WA_TransparentForMouseEvents, True)  # Allow mouse events to pass through
        self.setStyleSheet("background-color: rgba(30, 30, 40, 180); border: 1px solid rgb(200, 200, 100); border-radius: 5px;")  # Dark translucent background with golden border
        
        # Create layout and labels
        layout = QVBoxLayout()
        layout.setContentsMargins(10, 10, 10, 10)  # Add some padding
        layout.setSpacing(2)  # Reduce spacing between elements
        
        # Create labels for different HUD elements
        self.position_label = QLabel("Position: (0, 0, 0)")
        self.position_label.setStyleSheet("color: cyan; font-size: 16px; font-weight: bold; qproperty-alignment: AlignLeft;")
        self.position_label.setAlignment(Qt.AlignLeft)
        
        self.pov_label = QLabel("POV: Free Camera")
        self.pov_label.setStyleSheet("color: yellow; font-size: 16px; font-weight: bold; qproperty-alignment: AlignLeft;")
        self.pov_label.setAlignment(Qt.AlignLeft)
        
        self.controls_label1 = QLabel("WASD/QE: Move Camera | Arrows: Selector")
        self.controls_label1.setStyleSheet("color: white; font-size: 14px; qproperty-alignment: AlignLeft;")
        self.controls_label1.setAlignment(Qt.AlignLeft)
        
        self.controls_label2 = QLabel("Z/X: Selector Up/Down | 0: Cycle POV")
        self.controls_label2.setStyleSheet("color: white; font-size: 14px; qproperty-alignment: AlignLeft;")
        self.controls_label2.setAlignment(Qt.AlignLeft)
        
        # Add widgets to layout
        layout.addWidget(self.position_label)
        layout.addWidget(self.pov_label) 
        layout.addWidget(self.controls_label1)
        layout.addWidget(self.controls_label2)
        
        self.setLayout(layout)
        
        # Set fixed size for the HUD
        self.setFixedWidth(320)
        self.setFixedHeight(140)  # Explicit height to ensure it fits all content
        
    def update_data(self, selector_pos, pov_mode, pov_modes):
        """Update the HUD with new data"""
        self.position_label.setText(f"Position: X:{selector_pos[0]}, Y:{selector_pos[1]}, Z:{selector_pos[2]}")
        self.pov_label.setText(f"POV: {pov_modes[pov_mode] if pov_mode < len(pov_modes) else 'Unknown'}")


class NewHUDPlugin:
    """New HUD plugin using overlay widget approach"""
    
    def __init__(self, main_app):
        self.main_app = main_app
        self.grid_widget = None
        self.hud_widget = None
        self.is_initialized = False
        
        # Store references to grid data
        self.selector_pos = [0, 0, 0]
        self.pov_mode = 0
        self.pov_modes = ["Third Person", "First Person", "Free Camera"]
        
        # Timer for updating HUD
        self.update_timer = None
        
    def initialize(self, voxel_grid_widget):
        """Initialize the new HUD plugin"""
        print("Initializing NEW HUD Plugin with independent overlay widget...")
        
        self.grid_widget = voxel_grid_widget
        if self.grid_widget:
            # Create the HUD widget as child of main app
            self.hud_widget = HUDDisplayWidget(self.main_app)
            self.hud_widget.show()
            
            # Position it in the top-left corner of the main window
            self.hud_widget.move(10, 10)
            
            # Set up update timer to periodically update HUD data
            self.update_timer = QTimer()
            self.update_timer.timeout.connect(self._periodic_update)
            self.update_timer.start(100)  # Update every 100ms (10 times per second)
            
            self.is_initialized = True
            print("NEW HUD Plugin initialized successfully with independent widget")
            return True
        else:
            print("NEW HUD Plugin: Could not find voxel_grid_widget")
            return False
    
    def _periodic_update(self):
        """Periodically update HUD data from grid widget"""
        if self.grid_widget and self.hud_widget:
            # Update from grid widget data
            if hasattr(self.grid_widget, 'selector_pos'):
                self.selector_pos = self.grid_widget.selector_pos[:]
            
            if hasattr(self.grid_widget, 'camera') and self.grid_widget.camera:
                self.pov_mode = self.grid_widget.camera.pov_mode
                self.pov_modes = self.grid_widget.camera.pov_modes
            
            # Trigger HUD display update
            self.hud_widget.update_data(self.selector_pos, self.pov_mode, self.pov_modes)
    
    def handle_event(self, event):
        """Handle events if needed"""
        # This is a basic implementation
        return False
    
    def update(self):
        """Update method for plugin system"""
        # Handled by timer in _periodic_update
        pass

# Export the plugin class
Plugin = NewHUDPlugin