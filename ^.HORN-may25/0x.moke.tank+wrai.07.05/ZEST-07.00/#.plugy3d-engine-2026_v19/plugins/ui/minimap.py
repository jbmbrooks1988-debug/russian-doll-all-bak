"""
Minimap UI Plugin
Handles the minimap widget functionality.
"""

import sys
import math
from PySide6.QtWidgets import QWidget
from PySide6.QtCore import Qt, QPoint, QTimer
from PySide6.QtGui import QPainter, QColor, QFont, QPen, QBrush, QPolygon


class EnhancedMinimapWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.grid_size = 12
        self.entities = {}
        self.selector_pos = [0, 0, 0]  # [x, y, z] - using same format as 3D viewer
        self.camera_direction = 0  # Direction in radians
        self.pov_mode = 0  # 0=third person, 1=first person, 2=free camera
        self.pov_modes = ["Third Person", "First Person", "Free Camera"]
        
        # Size of the minimap
        self.minimap_size = 200
        self.margin = 10
        
        # Set fixed size
        self.setFixedSize(self.minimap_size + 2*self.margin, self.minimap_size + 40)  # Extra space for label
        
        # Make transparent background possible
        self.setAttribute(Qt.WA_TranslucentBackground)
        
    def update_data(self, entities, selector_pos, pov_mode, camera_direction):
        """Update the minimap data"""
        self.entities = entities
        self.selector_pos = selector_pos
        self.pov_mode = pov_mode
        self.camera_direction = camera_direction
        self.update()  # Immediately trigger repaint
        
    def paintEvent(self, event):
        """Draw the minimap"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        # Draw background with semi-transparent dark color
        painter.fillRect(
            self.margin, 
            self.margin, 
            self.minimap_size, 
            self.minimap_size, 
            QColor(40, 40, 40, 180)
        )
        
        # Set up for drawing the grid
        cell_size = self.minimap_size / self.grid_size
        
        # Draw grid lines
        painter.setPen(QPen(QColor(100, 100, 100, 150), 1))
        for i in range(self.grid_size + 1):
            # Vertical lines (for X axis)
            x_pos = self.margin + i * cell_size
            painter.drawLine(
                int(x_pos), int(self.margin), 
                int(x_pos), int(self.margin + self.minimap_size)
            )
            # Horizontal lines (for Z axis)
            z_pos = self.margin + i * cell_size
            painter.drawLine(
                int(self.margin), int(z_pos), 
                int(self.margin + self.minimap_size), int(z_pos)
            )
        
        # Draw entities
        painter.setPen(Qt.NoPen)  # No outline for entities
        for pos_key, entity in self.entities.items():
            x = entity['x']
            z = entity['z']
            
            # Convert to minimap coordinates
            minimap_x = self.margin + x * cell_size + cell_size / 2
            minimap_z = self.margin + z * cell_size + cell_size / 2
            
            # Determine entity color
            if entity['color'] == 'red':
                painter.setBrush(QColor(255, 77, 77))
            elif entity['color'] == 'blue':
                painter.setBrush(QColor(0, 128, 255))
            else:
                painter.setBrush(QColor(128, 128, 128))
            
            # Draw entity as a small rectangle
            entity_size = cell_size * 0.6
            painter.drawRect(
                int(minimap_x - entity_size / 2),
                int(minimap_z - entity_size / 2),
                int(entity_size), int(entity_size)
            )
        
        # Draw selector
        sel_x = self.selector_pos[0]  # x coordinate
        sel_z = self.selector_pos[2]  # z coordinate
        minimap_sel_x = self.margin + sel_x * cell_size + cell_size / 2
        minimap_sel_z = self.margin + sel_z * cell_size + cell_size / 2
        
        # Draw selector as yellow circle
        painter.setBrush(QColor(255, 255, 0))  # Yellow
        selector_radius = cell_size * 0.3
        painter.drawEllipse(
            int(minimap_sel_x - selector_radius),
            int(minimap_sel_z - selector_radius),
            int(2 * selector_radius), int(2 * selector_radius)
        )
        
        # Draw field of view (cone/sight) from selector if in first person mode
        if self.pov_mode == 1:  # First person mode
            # Draw sight cone/fan to represent field of view
            painter.setPen(QPen(QColor(0, 255, 255, 150), 2))  # Semi-transparent cyan
            
            look_distance = 4 * cell_size  # Length of sight lines
            fov_angle = math.pi / 4  # 45 degrees field of view (approx 90 degree total FOV)
            
            # In the 3D view, camera_direction is angle around Y-axis.
            # In minimap (X-Z plane), it should map directly.
            # X+ is right, Z+ is back.
            # Angle is 0 towards X+.
            # My camera_direction is set to -math.pi / 2 as default (towards Z-).
            # The reference had camera_direction 0 facing +Y.
            # So a -math.pi/2 for my camera_direction in the minimap means facing Z- (forward).

            center_angle = self.camera_direction
            left_angle = self.camera_direction - fov_angle  # 45 degrees to the left of center
            right_angle = self.camera_direction + fov_angle  # 45 degrees to the right of center
            
            # Calculate end points based on camera direction (X and Z components)
            center_end_x = minimap_sel_x + look_distance * math.cos(center_angle)
            center_end_z = minimap_sel_z + look_distance * math.sin(center_angle)
            left_end_x = minimap_sel_x + look_distance * math.cos(left_angle)
            left_end_z = minimap_sel_z + look_distance * math.sin(left_angle)
            right_end_x = minimap_sel_x + look_distance * math.cos(right_angle)
            right_end_z = minimap_sel_z + look_distance * math.sin(right_angle)
            
            # Draw the field of view as a filled triangle/cone
            painter.setBrush(QColor(0, 255, 255, 50))  # Translucent cyan fill
            fov_points = [
                QPoint(int(minimap_sel_x), int(minimap_sel_z)),
                QPoint(int(left_end_x), int(left_end_z)),
                QPoint(int(right_end_x), int(right_end_z))
            ]
            polygon = QPolygon(fov_points)
            painter.drawPolygon(polygon)
            
            # Draw outline and center line
            painter.setBrush(Qt.NoBrush)  # No fill for outline
            painter.setPen(QPen(QColor(0, 255, 255), 2))  # Brighter cyan for outline
            painter.drawLine(
                int(minimap_sel_x), int(minimap_sel_z),
                int(left_end_x), int(left_end_z)
            )
            painter.drawLine(
                int(minimap_sel_x), int(minimap_sel_z),
                int(right_end_x), int(right_end_z)
            )
            
            # Draw center line
            painter.setPen(QPen(QColor(0, 255, 255), 2))  # Brighter cyan for center line
            painter.drawLine(
                int(minimap_sel_x), int(minimap_sel_z),
                int(center_end_x), int(center_end_z)
            )
        
        # Draw POV mode indicator
        painter.setPen(QColor(255, 255, 255))
        painter.setFont(QFont("Arial", 10))
        painter.drawText(
            self.margin, 
            self.margin + self.minimap_size + 15, 
            f"POV: {self.pov_modes[self.pov_mode]}"
        )


class MinimapPlugin:
    """Plugin for handling minimap functionality."""
    def __init__(self, main_app):
        self.main_app = main_app
        self.grid_widget = None
        self.minimap_widget = None
        self.last_selector_pos = None
        
    def initialize(self, voxel_grid_widget):
        """Initialize the minimap plugin."""
        self.grid_widget = voxel_grid_widget
        if self.grid_widget:
            # The minimap widget is parented to the main window to be an overlay
            self.minimap_widget = EnhancedMinimapWidget(self.main_app)
            self.minimap_widget.show()
            
            # Patch resizeEvent to handle repositioning
            self.original_resizeEvent = self.main_app.resizeEvent
            self.main_app.resizeEvent = self.patched_resizeEvent
            
            # Initial positioning
            self.reposition_minimap()
            
            # Initialize minimap data to show initial state
            self.update()
            print("Minimap Plugin initialized")
            return True
        return False

    def patched_resizeEvent(self, event):
        """Called when the main window is resized."""
        self.original_resizeEvent(event)
        self.reposition_minimap()

    def reposition_minimap(self):
        """Positions the minimap in the top-right corner."""
        if self.minimap_widget:
            self.minimap_widget.move(self.main_app.width() - self.minimap_widget.width() - 10, 40)
    
    def update(self):
        """Update the minimap data."""
        if not self.grid_widget or not self.minimap_widget:
            return
            
        # Only update if selector position has changed (optimization)
        current_pos = tuple(self.grid_widget.selector_pos)
        if self.last_selector_pos != current_pos:
            self.last_selector_pos = current_pos
            
            # Get entity data in the format expected by the minimap
            entities_for_minimap = {}
            for key, value in self.grid_widget.entities.items():
                # Handle both entities (with color) and terrain blocks (with block_type)
                if 'color' in value:
                    # This is an entity with a specific color
                    color = value['color']
                elif 'block_type' in value:
                    # This is a terrain block, map the block type to a color
                    block_type = value['block_type']
                    # Map block types to recognizable colors for the minimap
                    color_map = {
                        'bedrock': 'black',
                        'stone': 'darkgray',
                        'dirt': 'brown',
                        'grass': 'green',
                        'sand': 'yellow',
                        'gravel': 'lightgray',
                        'planks': 'orange',
                        'bricks': 'red'
                    }
                    color = color_map.get(block_type, 'gray')  # Default to gray if block type not mapped
                else:
                    # Fallback for any other case
                    color = 'gray'
                
                entities_for_minimap[key] = {
                    'x': value['x'],
                    'y': value['y'],
                    'z': value['z'],
                    'color': color
                }
            
            # Get camera direction from camera controller plugin
            camera_direction = self.grid_widget.camera.camera_direction if self.grid_widget.camera else 0
            
            self.minimap_widget.update_data(
                entities_for_minimap, 
                self.grid_widget.selector_pos, 
                self.grid_widget.camera.pov_mode if self.grid_widget.camera else 0,
                camera_direction
            )

# Export the plugin class
Plugin = MinimapPlugin
