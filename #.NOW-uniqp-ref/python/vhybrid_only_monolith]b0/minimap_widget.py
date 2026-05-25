import sys
import math
from PySide6.QtWidgets import QWidget, QLabel, QVBoxLayout
from PySide6.QtCore import Qt
from PySide6.QtGui import QPainter, QColor, QFont, QPen, QBrush


class MinimapWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.grid_size = 12
        self.entities = {}
        self.selector_pos = [0, 0, 0]  # [z, y, x] - using same format as 3D viewer
        self.camera_direction = 0  # Direction in radians (0 = facing +X axis)
        self.is_selected = False
        self.pov_mode = 0  # 0=third person, 1=first person, 2=free camera
        self.pov_modes = ["Third Person", "First Person", "Free Camera"]
        
        # Size of the minimap
        self.minimap_size = 200
        self.margin = 10
        
        # Set fixed size
        self.setFixedSize(self.minimap_size + 2*self.margin, self.minimap_size + 40)  # Extra space for label
        
        # Make transparent background possible
        self.setAttribute(Qt.WA_TranslucentBackground)
        
    def update_data(self, entities, selector_pos, pov_mode, camera_direction=None):
        """Update the minimap data"""
        self.entities = entities
        self.selector_pos = selector_pos
        self.pov_mode = pov_mode
        if camera_direction is not None:
            self.camera_direction = camera_direction
        self.update()  # Trigger repaint
        
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
            # Vertical lines
            x_pos = self.margin + i * cell_size
            painter.drawLine(
                int(x_pos), int(self.margin), 
                int(x_pos), int(self.margin + self.minimap_size)
            )
            # Horizontal lines
            y_pos = self.margin + i * cell_size
            painter.drawLine(
                int(self.margin), int(y_pos), 
                int(self.margin + self.minimap_size), int(y_pos)
            )
        
        # Draw entities
        painter.setPen(Qt.NoPen)  # No outline for entities
        for pos_key, entity in self.entities.items():
            x = entity['x']
            y = entity['y']  # Changed from z to y
            
            # Convert to minimap coordinates
            minimap_x = self.margin + x * cell_size + cell_size / 2
            minimap_y = self.margin + y * cell_size + cell_size / 2  # Changed from z to y
            
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
                int(minimap_y - entity_size / 2),
                int(entity_size), int(entity_size)
            )
        
        # Draw selector
        sel_x = self.selector_pos[2]  # x index (3rd element: x coordinate)
        sel_y = self.selector_pos[1]  # y index (2nd element: y coordinate, used as y in the 2D layout)  
        minimap_sel_x = self.margin + sel_x * cell_size + cell_size / 2
        minimap_sel_y = self.margin + sel_y * cell_size + cell_size / 2
        
        # Draw selector as yellow circle
        painter.setBrush(QColor(255, 255, 0))  # Yellow
        selector_radius = cell_size * 0.3
        painter.drawEllipse(
            int(minimap_sel_x - selector_radius),
            int(minimap_sel_y - selector_radius),
            int(2 * selector_radius), int(2 * selector_radius)
        )
        
        # Draw field of view (cone/sight) from selector if in first person mode
        if self.pov_mode == 1:  # First person mode
            # Draw sight cone/fan to represent field of view
            painter.setPen(QPen(QColor(0, 255, 255, 150), 2))  # Semi-transparent cyan
            
            look_distance = 4 * cell_size  # Length of sight lines
            fov_angle = math.pi / 4  # 45 degrees field of view (approx 90 degree total FOV)
            
            # Calculate directions based on camera direction
            center_angle = self.camera_direction  # Direction camera is facing
            left_angle = self.camera_direction - fov_angle  # 45 degrees to the left of center
            right_angle = self.camera_direction + fov_angle  # 45 degrees to the right of center
            
            # Calculate end points based on camera direction
            center_end_x = minimap_sel_x + look_distance * math.cos(center_angle)
            center_end_y = minimap_sel_y + look_distance * math.sin(center_angle)
            left_end_x = minimap_sel_x + look_distance * math.cos(left_angle)
            left_end_y = minimap_sel_y + look_distance * math.sin(left_angle)
            right_end_x = minimap_sel_x + look_distance * math.cos(right_angle)
            right_end_y = minimap_sel_y + look_distance * math.sin(right_angle)
            
            # Draw the field of view as a filled triangle/cone
            painter.setBrush(QColor(0, 255, 255, 50))  # Translucent cyan fill
            fov_polygon = [
                (int(minimap_sel_x), int(minimap_sel_y)),
                (int(left_end_x), int(left_end_y)),
                (int(right_end_x), int(right_end_y))
            ]
            painter.drawPolygon(fov_polygon)
            
            # Draw outline and center line
            painter.setBrush(Qt.NoBrush)  # No fill for outline
            painter.setPen(QPen(QColor(0, 255, 255), 2))  # Brighter cyan for outline
            painter.drawLine(
                int(minimap_sel_x), int(minimap_sel_y),
                int(left_end_x), int(left_end_y)
            )
            painter.drawLine(
                int(minimap_sel_x), int(minimap_sel_y),
                int(right_end_x), int(right_end_y)
            )
            
            # Draw center line
            painter.setPen(QPen(QColor(0, 255, 255), 2))  # Brighter cyan for center line
            painter.drawLine(
                int(minimap_sel_x), int(minimap_sel_y),
                int(center_end_x), int(center_end_y)
            )
        
        # Draw POV mode indicator
        painter.setPen(QColor(255, 255, 255))
        painter.setFont(QFont("Arial", 10))
        painter.drawText(
            self.margin, 
            self.margin + self.minimap_size + 15, 
            f"POV: {self.pov_modes[self.pov_mode]}"
        )