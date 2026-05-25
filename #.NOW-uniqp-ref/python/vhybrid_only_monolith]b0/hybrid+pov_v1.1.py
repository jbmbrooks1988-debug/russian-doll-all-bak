"""
Hybrid 2D3D Voxel Viewer
Allows switching between 2D and 3D views using '2' and '3' keys
"""
import sys
import math
import csv
import os
from PySide6.QtWidgets import QApplication, QMainWindow, QStackedWidget, QLabel, QWidget
from PySide6.QtCore import Qt
from PySide6.QtGui import QPainter, QColor, QFont, QPen, QPaintDevice
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from OpenGL.GL import *
from OpenGL.GLU import *
try:
    from OpenGL.GLUT import *
except ImportError:
    # If GLUT is not available, we'll skip text rendering in the minimap
    pass

# Import the minimap widget
from minimap_widget import MinimapWidget


class HybridViewer(QMainWindow):
    def __init__(self):
        super().__init__()
        
        # Create a stacked widget to switch between 2D and 3D views
        self.stacked_widget = QStackedWidget()
        
        # Create 2D view
        self.view_2d = VoxelViewer2DWidget()
        self.stacked_widget.addWidget(self.view_2d)
        
        # Create 3D view
        self.view_3d = VoxelViewer3DWidget(parent=self)
        self.stacked_widget.addWidget(self.view_3d)
        
        # Set stacked widget as central widget
        self.setCentralWidget(self.stacked_widget)
        
        # Initially show 2D view
        self.stacked_widget.setCurrentIndex(0)
        self.current_mode = "2d"
        
        self.setWindowTitle("Hybrid 2D/3D Voxel Viewer")
        self.setGeometry(100, 100, 1000, 800)
        
        # Mode indicator - will be positioned dynamically
        self.mode_label = QLabel("Mode: 2D (Press '3' for 3D, '2' for 2D)", self)
        self.mode_label.setStyleSheet("color: white; background-color: black; padding: 5px;")
        # Initial position at top, will be repositioned for 2D vs 3D mode
        self.mode_label.setGeometry(10, 10, 450, 30)
        
        # Create minimap widget
        self.minimap = MinimapWidget(self)
        self.minimap.hide()  # Only show when in 3D mode
        
        # Initialize minimap data right away to show initial state
        self._update_minimap_initial_state()
        
        # Make sure the main window can receive key events
        self.setFocusPolicy(Qt.StrongFocus)
        self.setFocus()
        
    def _update_minimap_initial_state(self):
        """Update the minimap with initial state"""
        # Get entity data in the format expected by the minimap
        entities_for_minimap = {}
        for key, value in self.view_3d.entities.items():
            entities_for_minimap[key] = {
                'x': value['x'],
                'y': value['y'],  # Added y coordinate
                'z': value['z'],
                'color': value['color']
            }
        self.minimap.update_data(entities_for_minimap, self.view_3d.selector_pos, self.view_3d.pov_mode, self.view_3d.camera_direction)
        
    def keyPressEvent(self, event):
        key = event.key()
        if key == ord('2') or key == ord('3'):
            if key == ord('2') and self.current_mode != "2d":
                self.stacked_widget.setCurrentIndex(0)
                self.current_mode = "2d"
                self.mode_label.setText("Mode: 2D (Press '3' for 3D, '2' for 2D)")
                
                # Position the mode label below the selector position in 2D mode
                # The selector position text is drawn in the 2D widget at a specific Y position
                # Calculate position to put mode label below the 2D widget content
                widget_height = self.view_2d.grid_size * self.view_2d.cell_size + 20  # Approximate height of 2D widget
                self.mode_label.setGeometry(10, widget_height + 15, 450, 30)  # Position below 2D view
                
                self.minimap.hide()  # Hide minimap in 2D mode
                event.accept()  # Accept the event to prevent it from propagating
            elif key == ord('3') and self.current_mode != "3d":
                self.stacked_widget.setCurrentIndex(1)
                self.current_mode = "3d"
                # Update the mode label to show current POV mode
                pov_text = self.view_3d.pov_modes[self.view_3d.pov_mode]
                self.mode_label.setText(f"Mode: 3D - POV: {pov_text} (Press '2' for 2D, '0' to cycle POV)")
                
                # Position the mode label back at the top for 3D mode
                self.mode_label.setGeometry(10, 10, 450, 30)
                
                self.minimap.show()  # Show minimap in 3D mode
                # Position the minimap in the top-right corner of the 3D view
                self.minimap.setGeometry(self.width() - self.minimap.width() - 10, 40, 
                                         self.minimap.width(), self.minimap.height())
                event.accept()  # Accept the event to prevent it from propagating
        else:
            # Pass other keys to current widget
            current_widget = self.stacked_widget.currentWidget()
            if current_widget:
                current_widget.keyPressEvent(event)
    
    def keyReleaseEvent(self, event):
        # Ensure the main window has focus to catch key events
        self.setFocus()
        super().keyReleaseEvent(event)

    def resizeEvent(self, event):
        # Handle window resizing - adjust minimap position and mode label position
        super().resizeEvent(event)
        if self.current_mode == "2d":
            # Reposition mode label below 2D widget when window is resized
            widget_height = self.view_2d.grid_size * self.view_2d.cell_size + 20  # Approximate height of 2D widget
            self.mode_label.setGeometry(10, widget_height + 15, 450, 30)  # Position below 2D view
        elif self.current_mode == "3d":
            # Position the minimap in the top-right corner of the 3D view
            self.minimap.setGeometry(self.width() - self.minimap.width() - 10, 40,
                                     self.minimap.width(), self.minimap.height())
            # Keep mode label at top in 3D mode
            self.mode_label.setGeometry(10, 10, 450, 30)


class VoxelViewer2DWidget(QWidget):
    def __init__(self):
        super().__init__()
        
        self.grid_size = 12
        self.cell_size = 50  # Size of each cell in pixels
        self.entities = {}
        self.selector_pos = [0, 0, 0]  # [z, row, col]
        self.is_selected = False
        self.selected_entity_key_when_picked_up = None
        self.selected_entity_data = None
        
        self.setFocusPolicy(Qt.StrongFocus)
        self.setMinimumSize(self.grid_size * self.cell_size + 35, self.grid_size * self.cell_size + 20)
        self.setMaximumSize(self.grid_size * self.cell_size + 35, self.grid_size * self.cell_size + 20)
        
        self.init_entities()

    def init_entities(self):
        """Initialize the entities on the grid"""
        self.entities = {}
        self.load_data()
        
        # Initialize selector position
        if self.entities:
            first_entity = next(iter(self.entities.values()))
            self.selector_pos = [first_entity['z'], first_entity['row'], first_entity['col']]
        else:
            self.selector_pos = [0, 0, 0]

    def load_data(self):
        """Load entity data from CSV"""
        script_dir = os.path.dirname(__file__)
        file_path = os.path.join(script_dir, 'data', 'aggregated_data.csv')
        try:
            import pandas as pd
            df = pd.read_csv(file_path)
            attributes_df = df[(df['source_file'].str.contains('entity_attributes.csv', na=False)) & (df['entity_id'].isin(['adam', 'eve']))]
            for entity_id in ['adam', 'eve']:
                entity_attrs = attributes_df[(attributes_df['entity_id'] == entity_id) & (attributes_df['attribute'].isin(['x', 'z']))]
                if not entity_attrs.empty:
                    x = float(entity_attrs[entity_attrs['attribute'] == 'x']['value'].iloc[0])
                    z = float(entity_attrs[entity_attrs['attribute'] == 'z']['value'].iloc[0])
                    pos_key = f"{int(z)},{int(x)}"
                    
                    # Load tilemap for 2D representation
                    tilemap_csv = self.load_tilemap_csv(entity_id)
                    
                    self.entities[pos_key] = {
                        'color': 'red' if entity_id == 'adam' else 'blue', 
                        'z': int(z), 
                        'row': int(z), 
                        'col': int(x), 
                        'id': entity_id, 
                        'tilemap': tilemap_csv
                    }
        except FileNotFoundError:
            print("aggregated_data.csv not found.")
            # Add example entities
            self.entities["1,2"] = {'color': 'red', 'z': 1, 'row': 1, 'col': 2, 'id': 'adam', 'tilemap': self.load_tilemap_csv('adam')}
            self.entities["3,4"] = {'color': 'blue', 'z': 3, 'row': 3, 'col': 4, 'id': 'eve', 'tilemap': self.load_tilemap_csv('eve')}
        except Exception as e:
            print(f"Error loading data: {e}")
            self.entities["1,2"] = {'color': 'red', 'z': 1, 'row': 1, 'col': 2, 'id': 'adam', 'tilemap': self.load_tilemap_csv('adam')}
            self.entities["3,4"] = {'color': 'blue', 'z': 3, 'row': 3, 'col': 4, 'id': 'eve', 'tilemap': self.load_tilemap_csv('eve')}

    def load_tilemap_csv(self, entity_id):
        """Load the 8x8 tilemap CSV file for an entity"""
        script_dir = os.path.dirname(__file__)
        entity_dir = os.path.join(script_dir, 'data', 'entities', entity_id, 'tile_8x8')
        
        tilemap_data = []
        try:
            for file_name in os.listdir(entity_dir):
                if file_name.endswith('.csv'):
                    csv_path = os.path.join(entity_dir, file_name)
                    with open(csv_path, 'r') as f:
                        reader = csv.reader(f)
                        for row in reader:
                            tilemap_data.append(row)
                    break
        except FileNotFoundError:
            print(f"Tilemap CSV not found for {entity_id}.")
            for i in range(8):
                row = []
                for j in range(8):
                    if i == 0 or i == 7 or j == 0 or j == 7:
                        row.append("128,128,128")
                    else:
                        row.append("255,255,255")
                tilemap_data.append(row)
        except Exception as e:
            print(f"Error loading tilemap for {entity_id}: {e}")
            for i in range(8):
                row = []
                for j in range(8):
                    if i == 0 or i == 7 or j == 0 or j == 7:
                        row.append("128,128,128")
                    else:
                        row.append("255,255,255")
                tilemap_data.append(row)
        
        return tilemap_data

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        self.draw_grid(painter)
        self.draw_entities(painter)
        self.draw_selector(painter)

    def draw_grid(self, painter):
        pen = QPen(QColor(128, 128, 128), 2)
        painter.setPen(pen)
        painter.drawRect(25, 15, self.grid_size * self.cell_size, self.grid_size * self.cell_size)
        
        # Vertical lines
        for i in range(self.grid_size + 1):
            painter.drawLine(25 + i * self.cell_size, 15, 
                             25 + i * self.cell_size, 15 + self.grid_size * self.cell_size)
        
        # Horizontal lines
        for i in range(self.grid_size + 1):
            painter.drawLine(25, 15 + i * self.cell_size, 
                             25 + self.grid_size * self.cell_size, 15 + i * self.cell_size)
        
        # Row/column labels
        painter.setFont(QFont("Arial", 10))
        painter.setPen(QColor(255, 255, 255))  # White text
        for i in range(self.grid_size):
            # X labels
            col_label = f"X{i}"
            painter.drawText(25 + i * self.cell_size + self.cell_size // 4, 12, col_label)
            # Y labels
            row_label = f"Y{i}"
            painter.drawText(5, 15 + i * self.cell_size + self.cell_size // 2 + 5, row_label)
        
        # Z level indicator
        painter.setFont(QFont("Arial", 12, QFont.Bold))
        painter.setPen(QColor(255, 255, 0))  # Yellow text
        z_label = f"Z-Level: {self.selector_pos[0]}"
        painter.drawText(25, 15 + self.grid_size * self.cell_size + 5, z_label)
        
        # Selector position readout - positioned to the far right
        painter.setPen(QColor(0, 255, 255))  # Cyan text
        pos_label = f"Selector X,Y,Z: {self.selector_pos[2]},{self.selector_pos[1]},{self.selector_pos[0]}"
        painter.drawText(320, 15 + self.grid_size * self.cell_size + 5, pos_label)
        
        # Mode instructions positioned below the selector position readout
        painter.setPen(QColor(255, 255, 255))  # White text
        mode_instructions = "Press '3' for 3D, '2' for 2D"
        painter.drawText(320, 15 + self.grid_size * self.cell_size + 25, mode_instructions)  # Below selector

    def draw_entities(self, painter):
        current_z_level = self.selector_pos[0]
        for pos_key, entity in self.entities.items():
            if entity['z'] == current_z_level:
                self.draw_single_entity(painter, entity['row'], entity['col'], entity['color'], entity.get('tilemap'))

        if self.is_selected and self.selected_entity_data:
            self.draw_single_entity(painter, self.selector_pos[1], self.selector_pos[2], self.selected_entity_data['color'], self.selected_entity_data.get('tilemap'))

    def draw_single_entity(self, painter, row, col, color, tilemap=None):
        x = 25 + col * self.cell_size
        y = 15 + row * self.cell_size
        
        if color == 'blue':
            painter.setBrush(QColor(0, 128, 255))  # Blue
        elif color == 'red':
            painter.setBrush(QColor(255, 77, 77))  # Red
        else:
            painter.setBrush(QColor(128, 128, 128))  # Gray as default
        
        painter.drawRect(x + 2, y + 2, self.cell_size - 4, self.cell_size - 4)
        
        if tilemap:
            self.draw_tilemap(painter, x + 2, y + 2, self.cell_size - 4, tilemap)

    def draw_tilemap(self, painter, x, y, size, tilemap):
        margin = size * 0.15
        tilemap_size = size - 2 * margin
        
        if tilemap_size <= 0:
            return
        
        tile_size = tilemap_size / 8
        start_x = x + margin
        start_y = y + margin
        
        for row_idx, row in enumerate(tilemap):
            for col_idx, rgb_str in enumerate(row):
                if row_idx < 8 and col_idx < 8:
                    tile_x = start_x + col_idx * tile_size
                    tile_y = start_y + row_idx * tile_size
                    
                    try:
                        r, g, b = map(int, rgb_str.split(','))
                        color = QColor(r, g, b)
                    except ValueError:
                        color = QColor(255, 255, 255)
                    
                    painter.fillRect(tile_x + 0.5, tile_y + 0.5, tile_size - 1, tile_size - 1, color)

    def draw_selector(self, painter):
        x = 25 + self.selector_pos[2] * self.cell_size  # Column (index 2)
        y = 15 + self.selector_pos[1] * self.cell_size  # Row (index 1)
        
        target_pos_key = f"{self.selector_pos[0]},{self.selector_pos[2]}"  # z and column
        has_entity = False
        
        if target_pos_key in self.entities:
            entity = self.entities[target_pos_key]
            if entity['row'] == self.selector_pos[1]:
                has_entity = True
        
        if not self.is_selected and has_entity:
            pen_color = QColor(204, 0, 204)  # Purple when over entity
        elif self.is_selected:
            target_occupied = False
            if target_pos_key in self.entities:
                entity = self.entities[target_pos_key]
                if entity['row'] == self.selector_pos[1]:
                    target_occupied = True
            
            if target_occupied:
                pen_color = QColor(255, 255, 0) # Yellow for invalid
            else:
                pen_color = QColor(0, 255, 0) # Green for valid
        else:
            pen_color = QColor(255, 255, 255) # White default
        
        painter.setPen(QPen(pen_color, 3))
        painter.setBrush(Qt.NoBrush)
        painter.drawRect(x, y, self.cell_size, self.cell_size)

    def keyPressEvent(self, event):
        key = event.key()
        
        if key == Qt.Key_Return or key == Qt.Key_Enter:
            current_pos_key = f"{self.selector_pos[0]},{self.selector_pos[2]}"
            
            if not self.is_selected:
                if current_pos_key in self.entities:
                    self.is_selected = True
                    self.selected_entity_key_when_picked_up = current_pos_key
                    self.selected_entity_data = self.entities.pop(current_pos_key)
            else:
                target_pos_key = f"{self.selector_pos[0]},{self.selector_pos[2]}"
                if target_pos_key in self.entities:
                    self.entities[self.selected_entity_key_when_picked_up] = self.selected_entity_data
                else:
                    self.selected_entity_data['z'] = self.selector_pos[0]
                    self.selected_entity_data['row'] = self.selector_pos[1]
                    self.selected_entity_data['col'] = self.selector_pos[2]
                    self.entities[target_pos_key] = self.selected_entity_data
                
                self.is_selected = False
                self.selected_entity_key_when_picked_up = None
                self.selected_entity_data = None
        elif key == Qt.Key_Space:
            current_pos_key = f"{self.selector_pos[0]},{self.selector_pos[2]}"
            if not self.is_selected:
                if current_pos_key in self.entities:
                    self.is_selected = True
                    self.selected_entity_key_when_picked_up = current_pos_key
                    self.selected_entity_data = self.entities.pop(current_pos_key)
            else:
                target_pos_key = f"{self.selector_pos[0]},{self.selector_pos[2]}"
                if target_pos_key in self.entities:
                    self.entities[self.selected_entity_key_when_picked_up] = self.selected_entity_data
                else:
                    self.selected_entity_data['z'] = self.selector_pos[0]
                    self.selected_entity_data['row'] = self.selector_pos[1]
                    self.selected_entity_data['col'] = self.selector_pos[2]
                    self.entities[target_pos_key] = self.selected_entity_data
                
                self.is_selected = False
                self.selected_entity_key_when_picked_up = None
                self.selected_entity_data = None
            self.update()
            return
        else:
            old_selector_pos = self.selector_pos[:]
            new_pos = old_selector_pos[:]
            
            if key == Qt.Key_Up and self.selector_pos[1] > 0:
                new_pos[1] -= 1  # Moving up decreases row
            elif key == Qt.Key_Down and self.selector_pos[1] < self.grid_size - 1:
                new_pos[1] += 1  # Moving down increases row
            elif key == Qt.Key_Left and self.selector_pos[2] > 0:
                new_pos[2] -= 1  # Moving left decreases column
            elif key == Qt.Key_Right and self.selector_pos[2] < self.grid_size - 1:
                new_pos[2] += 1  # Moving right increases column
            elif key == Qt.Key_Z and self.selector_pos[0] > 0:  # Z key to move down in Z-axis
                new_pos[0] -= 1
            elif key == Qt.Key_X and self.selector_pos[0] < self.grid_size - 1:  # X key to move up in Z-axis
                new_pos[0] += 1
            
            if new_pos != old_selector_pos:
                self.selector_pos = new_pos
        
        self.update()


class VoxelViewer3DWidget(QOpenGLWidget):
    def __init__(self, parent=None):
        super().__init__()
        
        self.parent = parent
        self.grid_size = 12
        self.cell_size = 1.0  # Size of each cell in world units
        self.entities = {}
        self.selector_pos = [0, 0, 0]  # [z, y, x]
        self.is_selected = False
        self.selected_entity_key_when_picked_up = None
        self.selected_entity_data = None
        
        # Camera controls
        self.camera_x = 0.0
        self.camera_y = -12.0
        self.camera_z = 12.0
        self.camera_angle_x = -45  # Looking down at a moderate angle
        self.camera_angle_y = 0  # Looking straight (no rotation)
        
        # Movement speed
        self.move_speed = 0.5
        
        # Toggle for entity rotation: True to rotate entities 90° around X-axis, False for original orientation
        self.rotate_entities = True
        
        # POV modes: 0=third_person, 1=first_person, 2=free_camera
        self.pov_mode = 2  # Start in free camera mode
        self.pov_modes = ["Third Person", "First Person", "Free Camera"]
        
        # Store selector position for camera following in first/third person modes
        self.last_selector_pos = [0, 0, 0]
        
        # Camera direction for first person POV (in radians)
        self.camera_direction = 0  # Start facing +Y direction (with -π/2 offset in update_camera gives -π/2 actual rotation -> +Y)

        self.setFocusPolicy(Qt.StrongFocus)
        self.init_entities()
        
    def draw_minimap_gl(self):
        """Draw a 2D minimap using OpenGL"""
        # Get viewport dimensions
        viewport = glGetIntegerv(GL_VIEWPORT)
        width, height = viewport[2], viewport[3]
        
        # Define minimap dimensions
        minimap_size = 200  # pixels
        minimap_margin = 20
        minimap_x = width - minimap_size - minimap_margin
        minimap_y = height - minimap_size - minimap_margin
        
        # Switch to orthographic projection for 2D drawing
        glMatrixMode(GL_PROJECTION)
        glPushMatrix()
        glLoadIdentity()
        gluOrtho2D(0, width, 0, height)
        
        glMatrixMode(GL_MODELVIEW)
        glPushMatrix()
        glLoadIdentity()
        
        # Disable depth testing for overlay
        glDisable(GL_DEPTH_TEST)
        
        # Draw minimap background
        glColor4f(0.2, 0.2, 0.2, 0.7)  # Semi-transparent dark gray
        glBegin(GL_QUADS)
        glVertex2f(minimap_x, minimap_y)
        glVertex2f(minimap_x + minimap_size, minimap_y)
        glVertex2f(minimap_x + minimap_size, minimap_y + minimap_size)
        glVertex2f(minimap_x, minimap_y + minimap_size)
        glEnd()
        
        # Draw grid lines on minimap
        glColor4f(0.4, 0.4, 0.4, 0.5)  # Semi-transparent gray
        glLineWidth(1.0)
        
        # Calculate grid spacing
        cell_size = minimap_size / self.grid_size
        
        # Draw vertical lines
        for i in range(self.grid_size + 1):
            x_pos = minimap_x + i * cell_size
            glBegin(GL_LINES)
            glVertex2f(x_pos, minimap_y)
            glVertex2f(x_pos, minimap_y + minimap_size)
            glEnd()
        
        # Draw horizontal lines
        for i in range(self.grid_size + 1):
            y_pos = minimap_y + i * cell_size
            glBegin(GL_LINES)
            glVertex2f(minimap_x, y_pos)
            glVertex2f(minimap_x + minimap_size, y_pos)
            glEnd()
        
        # Draw all entities on minimap
        for pos_key, entity in self.entities.items():
            x = entity['x']
            y = entity['y']  # Changed from z to y
            minimap_x_pos = minimap_x + x * cell_size + cell_size / 2
            minimap_y_pos = minimap_y + y * cell_size + cell_size / 2  # Changed from z to y
            
            # Determine entity color
            if entity['color'] == 'red':
                glColor3f(1.0, 0.3, 0.3)  # Red
            elif entity['color'] == 'blue':
                glColor3f(0.0, 0.5, 1.0)  # Blue
            else:
                glColor3f(0.5, 0.5, 0.5)  # Gray
            
            # Draw entity as a small square
            half_size = cell_size * 0.3
            glBegin(GL_QUADS)
            glVertex2f(minimap_x_pos - half_size, minimap_y_pos - half_size)
            glVertex2f(minimap_x_pos + half_size, minimap_y_pos - half_size)
            glVertex2f(minimap_x_pos + half_size, minimap_y_pos + half_size)
            glVertex2f(minimap_x_pos - half_size, minimap_y_pos + half_size)
            glEnd()
        
        # Draw selector position on minimap
        sel_x = self.selector_pos[2]  # x index
        sel_y = self.selector_pos[1]  # y index (changed from z to y)
        minimap_sel_x = minimap_x + sel_x * cell_size + cell_size / 2
        minimap_sel_y = minimap_y + sel_y * cell_size + cell_size / 2  # Changed from sel_z to sel_y
        
        # Draw selector as yellow circle
        glColor3f(1.0, 1.0, 0.0)  # Yellow
        glBegin(GL_TRIANGLE_FAN)
        glVertex2f(minimap_sel_x, minimap_sel_y)  # Center of circle
        for i in range(20):
            angle = 2 * math.pi * i / 20
            glVertex2f(minimap_sel_x + cell_size * 0.25 * math.cos(angle),
                      minimap_sel_y + cell_size * 0.25 * math.sin(angle))
        glEnd()
        
        # Draw line of sight from selector if in first person mode
        if self.pov_mode == 1:  # First person mode
            glColor3f(0.0, 1.0, 1.0)  # Cyan for line of sight
            glLineWidth(2.0)
            
            # Calculate direction vector based on camera direction
            look_distance = 3 * cell_size  # Length of sight line
            look_end_x = minimap_sel_x + look_distance * math.cos(self.camera_direction)
            look_end_y = minimap_sel_y + look_distance * math.sin(self.camera_direction)
            
            glBegin(GL_LINES)
            glVertex2f(minimap_sel_x, minimap_sel_y)  # Start from selector
            glVertex2f(look_end_x, look_end_y)       # End point of sight line
            glEnd()
        
        # Draw POV mode text if GLUT is available
        try:
            glRasterPos2f(minimap_x, minimap_y - 10)
            mode_text = f"POV: {self.pov_modes[self.pov_mode]}"
            for char in mode_text:
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, ord(char))
        except:
            # GLUT not available, skip text rendering
            pass
        
        # Re-enable depth testing
        glEnable(GL_DEPTH_TEST)
        
        # Restore previous matrices
        glPopMatrix()
        glMatrixMode(GL_PROJECTION)
        glPopMatrix()
        glMatrixMode(GL_MODELVIEW)
        
    def paintGL(self):
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()
        
        # Update camera based on POV mode
        self.update_camera()
        
        # Draw the grid
        self.draw_grid()
        
        # Draw entities (foundations)
        self.draw_entities()
        
        # Draw selector
        self.draw_selector()
        
        # Update minimap if it exists
        if hasattr(self, 'parent') and hasattr(self.parent, 'minimap'):
            # Get entity data in the format expected by the minimap
            entities_for_minimap = {}
            for key, value in self.entities.items():
                entities_for_minimap[key] = {
                    'x': value['x'],
                    'y': value['y'],  # Added y coordinate
                    'z': value['z'],
                    'color': value['color']
                }
            self.parent.minimap.update_data(entities_for_minimap, self.selector_pos, self.pov_mode, self.camera_direction)

    def init_entities(self):
        """Initialize the entities on the grid"""
        self.entities = {}
        self.load_data()

    def load_data(self):
        """Load entity data from CSV"""
        script_dir = os.path.dirname(__file__)
        file_path = os.path.join(script_dir, 'data', 'aggregated_data.csv')
        try:
            import pandas as pd
            df = pd.read_csv(file_path)
            attributes_df = df[(df['source_file'].str.contains('entity_attributes.csv', na=False)) & (df['entity_id'].isin(['adam', 'eve']))]
            for entity_id in ['adam', 'eve']:
                entity_attrs = attributes_df[(attributes_df['entity_id'] == entity_id) & (attributes_df['attribute'].isin(['x', 'z']))]
                if not entity_attrs.empty:
                    x = float(entity_attrs[entity_attrs['attribute'] == 'x']['value'].iloc[0])
                    z = float(entity_attrs[entity_attrs['attribute'] == 'z']['value'].iloc[0])
                    pos_key = f"{int(z)},{int(x)}"
                    
                    # Load 3D voxel model for the entity
                    voxel_model = self.load_voxel_model(entity_id)
                    
                    self.entities[pos_key] = {
                        'color': 'red' if entity_id == 'adam' else 'blue', 
                        'z': int(z), 
                        'x': int(x), 
                        'y': 0,  # Set y to 0 to represent ground level
                        'id': entity_id, 
                        'voxel_model': voxel_model
                    }
        except FileNotFoundError:
            print("aggregated_data.csv not found.")
            # Add example entities
            self.entities["1,2"] = {'color': 'red', 'z': 1, 'x': 2, 'y': 0, 'id': 'adam', 'voxel_model': self.load_voxel_model('adam')}
            self.entities["3,4"] = {'color': 'blue', 'z': 3, 'x': 4, 'y': 0, 'id': 'eve', 'voxel_model': self.load_voxel_model('eve')}
        except Exception as e:
            print(f"Error loading data: {e}")
            self.entities["1,2"] = {'color': 'red', 'z': 1, 'x': 2, 'y': 0, 'id': 'adam', 'voxel_model': self.load_voxel_model('adam')}
            self.entities["3,4"] = {'color': 'blue', 'z': 3, 'x': 4, 'y': 0, 'id': 'eve', 'voxel_model': self.load_voxel_model('eve')}

    def load_voxel_model(self, entity_id):
        """Load the 8x8x8 voxel CSV file for an entity and normalize coordinates"""
        script_dir = os.path.dirname(__file__)
        entity_dir = os.path.join(script_dir, 'data', 'entities', entity_id, 'voxel_8x8x8')
        
        raw_voxel_data = []
        try:
            for file_name in os.listdir(entity_dir):
                if file_name.endswith('.csv'):
                    csv_path = os.path.join(entity_dir, file_name)
                    with open(csv_path, 'r') as f:
                        reader = csv.DictReader(f)
                        for row in reader:
                            x = float(row['x'])
                            y = float(row['y'])
                            z = float(row['z'])
                            r = int(row['r']) / 255.0
                            g = int(row['g']) / 255.0
                            b = int(row['b']) / 255.0
                            # Skip black voxels (transparent)
                            if not (r == 0.0 and g == 0.0 and b == 0.0):
                                raw_voxel_data.append((x, y, z, r, g, b))
                    break  # Use the first CSV file found
        except FileNotFoundError:
            print(f"Voxel model CSV not found for {entity_id}.")
        except Exception as e:
            print(f"Error loading voxel model for {entity_id}: {e}")
        
        # Normalize coordinates to fit within 0-7 range for an 8x8x8 model
        if not raw_voxel_data:
            return raw_voxel_data
            
        # Find min and max for each axis
        min_x = min(v[0] for v in raw_voxel_data)
        max_x = max(v[0] for v in raw_voxel_data)
        min_y = min(v[1] for v in raw_voxel_data)
        max_y = max(v[1] for v in raw_voxel_data)
        min_z = min(v[2] for v in raw_voxel_data)
        max_z = max(v[2] for v in raw_voxel_data)
        
        # Calculate scale factors to fit within 0-7 range
        size_x = max_x - min_x if max_x != min_x else 1
        size_y = max_y - min_y if max_y != min_y else 1
        size_z = max_z - min_z if max_z != min_z else 1
        
        # Normalize the coordinates to 0-7 range
        normalized_voxel_data = []
        for x, y, z, r, g, b in raw_voxel_data:
            # Map from original range to 0-7 range
            norm_x = 7.0 * (x - min_x) / size_x
            norm_y = 7.0 * (y - min_y) / size_y
            norm_z = 7.0 * (z - min_z) / size_z
            # Ensure values are within bounds
            norm_x = max(0, min(7, norm_x))
            norm_y = max(0, min(7, norm_y))
            norm_z = max(0, min(7, norm_z))
            normalized_voxel_data.append((norm_x, norm_y, norm_z, r, g, b))
        
        return normalized_voxel_data

    def initializeGL(self):
        glClearColor(0.0, 0.0, 0.0, 1.0)
        glEnable(GL_DEPTH_TEST)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
        glEnable(GL_CULL_FACE)
        glCullFace(GL_BACK)
        glShadeModel(GL_SMOOTH)

    def resizeGL(self, width, height):
        glViewport(0, 0, width, height)
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        gluPerspective(45, width / height, 0.1, 100.0)
        glMatrixMode(GL_MODELVIEW)

    def paintEvent(self, event):
        # Call the parent paintGL to handle the 3D rendering
        super().paintEvent(event)
        
        # Now draw 2D overlays like the position readout using QPainter
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        # Draw selector position readout - positioned below the mode label to avoid overlap
        painter.setPen(QColor(0, 255, 255))  # Cyan text
        painter.setFont(QFont("Arial", 12, QFont.Bold))
        pos_label = f"Selector X,Y,Z: {self.selector_pos[2]},{self.selector_pos[1]},{self.selector_pos[0]}"
        painter.drawText(10, 60, pos_label)  # Positioned below the mode label at (10, 60)
        
        painter.end()

    def paintGL(self):
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()
        
        # Update camera based on POV mode
        self.update_camera()
        
        # Draw the grid
        self.draw_grid()
        
        # Draw entities (foundations)
        self.draw_entities()
        
        # Draw selector
        self.draw_selector()
        
        # Draw XYZ axis indicator
        self.draw_axis_indicator()
        
        # Note: Minimap will be drawn via paintEvent override since QPainter works better for 2D overlays

    def update_camera(self):
        """Update camera based on selected POV mode"""
        selector_x = self.selector_pos[2] * self.cell_size
        selector_y = (self.grid_size - 1 - self.selector_pos[1]) * self.cell_size
        selector_z = self.selector_pos[0] * self.cell_size
        
        if self.pov_mode == 0:  # Third person mode
            # New third person logic: behind, above, looking down
            center_x = selector_x + self.cell_size / 2
            center_y = selector_y + self.cell_size / 2
            center_z = selector_z + self.cell_size / 2

            distance = -9 #-7
            height = -2 #-4 #4
            pitch = -30 #-90
            
            # Reverse order of operations for OpenGL
            glTranslatef(0, 0, distance) # Step 5: move camera away from selector
            glRotatef(pitch, 1, 0, 0)  # Step 4: Pitch down to look at selector
            glRotatef(math.degrees(self.camera_direction), 0, 0, 1) # Step 3: Rotate around Z-axis
            glTranslatef(0, height, 0) # Step 2: Elevate camera
            glTranslatef(-center_x, -center_y, -center_z) # Step 1: Center on selector
        elif self.pov_mode == 1:  # First person mode
            # Camera positioned at selector position with appropriate eye-level height
            # For first person, position camera at selector with an eye-level height to see ground below
            eye_level = 2.0  # Higher eye level to see the ground beneath and have proper perspective
            
            # Set camera to be at the selector's position but slightly above
            # Apply rotation around Y-axis for left/right turning, but start facing +Y axis
            # To make +Y the default forward direction, we need to rotate from the default -Z facing
            # By -90 degrees around X-axis first, then apply the Y-axis rotation for direction
            glRotatef(-90, 1, 0, 0)  # Rotate to make Y-axis forward (from default -Z)
            glRotatef(math.degrees(self.camera_direction), 0, 0, 1)    # Then rotate around Z-axis for left/right
            # Offset camera to center it in the grid cell and at proper eye level
            # Move camera to center of the grid cube for proper viewing
            # Move camera forward by 1 grid unit in the Y direction (accounting for inverted Y rendering)
            glTranslatef(-selector_x - self.cell_size/2, -(selector_y + eye_level - 2*self.cell_size), -selector_z - self.cell_size/2)
        else:  # Free camera mode (pov_mode == 2)
            # Use the default camera controls
            glRotatef(self.camera_angle_x, 1, 0, 0)
            glRotatef(self.camera_angle_y, 0, 1, 0)
            glTranslatef(-self.camera_x, -self.camera_y, -self.camera_z)
    def draw_grid(self):
        glColor3f(0.5, 0.5, 0.5)
        glLineWidth(1.0)
        
        # Draw grid lines in X-Y plane at each Z level
        for z in range(self.grid_size):
            for i in range(self.grid_size + 1):
                # Lines along X axis
                glBegin(GL_LINES)
                glVertex3f(0, i * self.cell_size, z * self.cell_size)
                glVertex3f(self.grid_size * self.cell_size, i * self.cell_size, z * self.cell_size)
                glEnd()
                
                # Lines along Y axis
                glBegin(GL_LINES)
                glVertex3f(i * self.cell_size, 0, z * self.cell_size)
                glVertex3f(i * self.cell_size, self.grid_size * self.cell_size, z * self.cell_size)
                glEnd()

    def draw_entities(self):
        # Draw all entities from all Z levels
        for pos_key, entity in self.entities.items():
            self.draw_single_entity(
                entity['x'] * self.cell_size, 
                (self.grid_size - 1 - entity['y']) * self.cell_size, 
                entity['z'] * self.cell_size, 
                entity['color'],
                entity)  # Pass the entity data

        # If an entity is selected, draw it at the selector's current position
        if self.is_selected and self.selected_entity_data:
            x = self.selector_pos[2] * self.cell_size
            y = (self.grid_size - 1 - self.selector_pos[1]) * self.cell_size
            z = self.selector_pos[0] * self.cell_size
            self.draw_single_entity(x, y, z, self.selected_entity_data['color'], self.selected_entity_data)

    def draw_single_entity(self, x, y, z, color, entity_data=None):
        """Draw a single entity foundation and its 3D model"""
        # Draw the thin foundation with higher transparency
        if color == 'blue':
            glColor4f(0.0, 0.5, 1.0, 0.3)  # Blue with high transparency
        elif color == 'red':
            glColor4f(1.0, 0.3, 0.3, 0.3)  # Red with high transparency
        else:
            glColor4f(0.5, 0.5, 0.5, 0.3)  # Gray with high transparency

        # Draw the thin foundation cube at the bottom
        foundation_height = self.cell_size * 0.1  # Thin foundation
        self.draw_flat_cube(x, y, z, self.cell_size, foundation_height, is_entity=True)
        
        # Draw the 3D voxel model
        if entity_data and 'voxel_model' in entity_data and entity_data['voxel_model']:
            self.draw_3d_model(x, y, z, entity_data['voxel_model'], self.cell_size)

    def draw_flat_cube(self, x, y, z, size, height, is_entity=False):
        """Draw a thin flat cube (foundation)"""
        if is_entity:
            glBegin(GL_QUADS)
            # Top face
            glVertex3f(x, y, z + height)
            glVertex3f(x + size, y, z + height)
            glVertex3f(x + size, y + size, z + height)
            glVertex3f(x, y + size, z + height)
            # Bottom face
            glVertex3f(x, y, z)
            glVertex3f(x, y + size, z)
            glVertex3f(x + size, y + size, z)
            glVertex3f(x + size, y, z)
            # Front face
            glVertex3f(x, y, z)
            glVertex3f(x, y, z + height)
            glVertex3f(x + size, y, z + height)
            glVertex3f(x + size, y, z)
            # Back face
            glVertex3f(x, y + size, z)
            glVertex3f(x + size, y + size, z)
            glVertex3f(x + size, y + size, z + height)
            glVertex3f(x, y + size, z + height)
            # Right face
            glVertex3f(x + size, y, z)
            glVertex3f(x + size, y + size, z)
            glVertex3f(x + size, y + size, z + height)
            glVertex3f(x + size, y, z + height)
            # Left face
            glVertex3f(x, y, z)
            glVertex3f(x, y, z + height)
            glVertex3f(x, y + size, z + height)
            glVertex3f(x, y + size, z)
            glEnd()
        else:
            pass

    def draw_3d_model(self, base_x, base_y, base_z, voxel_model, cell_size):
        """Draw a 3D voxel model that fills the grid cube space"""
        voxel_size = cell_size / 8.0  # Each model voxel is 1/8 of the grid cube size
        
        for voxel in voxel_model:
            if len(voxel) >= 6:  # Ensure we have x,y,z,r,g,b
                orig_x, orig_y, orig_z, r, g, b = voxel[:6]
                
                if self.rotate_entities:
                    # Apply rotation transformation for entity models:
                    # Rotate 90 degrees around X-axis so top faces +Y and bottom faces -Y
                    # New X = orig_x (unchanged)
                    # New Y = orig_z (original Z becomes new Y)
                    # New Z = -orig_y (original Y becomes negative Z)
                    rot_x = orig_x
                    rot_y = orig_z
                    rot_z = -orig_y  # This moves the range from [0,7] to [-7,0]
                    
                    # Adjust the Z coordinate to be in the [0,7] range again
                    rot_z = 7 + rot_z  # Now rot_z is in range [0,7] where orig_y=0 becomes z=7, orig_y=7 becomes z=0
                else:
                    # Use original coordinates without rotation
                    rot_x = orig_x
                    rot_y = orig_y
                    rot_z = orig_z
                
                # Position each voxel in the center of its grid space after rotation
                center_x = base_x + rot_x * voxel_size + voxel_size/2
                center_y = base_y + (7 - rot_y) * voxel_size + voxel_size/2  # Invert Y coordinate after rotation
                center_z = base_z + rot_z * voxel_size + voxel_size/2
                
                # Draw the single voxel cube with the correct size at the correct center position
                glColor3f(r, g, b)
                self.draw_voxel_cube(center_x, center_y, center_z, voxel_size)

    def draw_voxel_cube(self, x, y, z, size):
        """Draw a single 3D voxel cube."""
        size_half = size / 2
        vertices = [
            [x - size_half, y - size_half, z - size_half],  # Vertex 0: back-bottom-left
            [x + size_half, y - size_half, z - size_half],  # Vertex 1: back-bottom-right
            [x + size_half, y + size_half, z - size_half],  # Vertex 2: back-top-right
            [x - size_half, y + size_half, z - size_half],  # Vertex 3: back-top-left
            [x - size_half, y - size_half, z + size_half],  # Vertex 4: front-bottom-left
            [x + size_half, y - size_half, z + size_half],  # Vertex 5: front-bottom-right
            [x + size_half, y + size_half, z + size_half],  # Vertex 6: front-top-right
            [x - size_half, y + size_half, z + size_half]   # Vertex 7: front-top-left
        ]
        
        faces = [
            [0, 1, 2, 3],  # Back face (-Z)
            [5, 4, 7, 6],  # Front face (+Z)
            [4, 0, 3, 7],  # Left face (-X)
            [1, 5, 6, 2],  # Right face (+X)
            [3, 2, 6, 7],  # Top face (+Y)
            [4, 5, 1, 0]   # Bottom face (-Y)
        ]
        
        glBegin(GL_QUADS)
        for face in faces:
            for vertex in face:
                glVertex3fv(vertices[vertex])
        glEnd()

    def draw_cube(self, x, y, z, size, is_entity=False):
        """Draw a full cube at the specified position"""
        cube_height = size  # Full height
        if is_entity:
            # Draw a full cube for entities
            glBegin(GL_QUADS)
            # Top face
            glVertex3f(x, y, z + cube_height)
            glVertex3f(x + size, y, z + cube_height)
            glVertex3f(x + size, y + size, z + cube_height)
            glVertex3f(x, y + size, z + cube_height)
            # Bottom face
            glVertex3f(x, y, z)
            glVertex3f(x, y + size, z)
            glVertex3f(x + size, y + size, z)
            glVertex3f(x + size, y, z)
            # Front face
            glVertex3f(x, y, z)
            glVertex3f(x, y, z + cube_height)
            glVertex3f(x + size, y, z + cube_height)
            glVertex3f(x + size, y, z)
            # Back face
            glVertex3f(x, y + size, z)
            glVertex3f(x + size, y + size, z)
            glVertex3f(x + size, y + size, z + cube_height)
            glVertex3f(x, y + size, z + cube_height)
            # Right face
            glVertex3f(x + size, y, z)
            glVertex3f(x + size, y + size, z)
            glVertex3f(x + size, y + size, z + cube_height)
            glVertex3f(x + size, y, z + cube_height)
            # Left face
            glVertex3f(x, y, z)
            glVertex3f(x, y, z + cube_height)
            glVertex3f(x, y + size, z + cube_height)
            glVertex3f(x, y + size, z)
            glEnd()
        else:
            # Draw wireframe cube for selector - make it slightly bigger to be more visible
            selector_size = size * 1.1  # Make slightly larger
            selector_offset = (selector_size - size) / 2
            selector_x = x - selector_offset
            selector_y = y - selector_offset
            selector_z = z
            
            glLineWidth(5.0)
            glBegin(GL_LINES)
            # Top face
            glVertex3f(selector_x, selector_y, selector_z + cube_height)
            glVertex3f(selector_x + selector_size, selector_y, selector_z + cube_height)
            
            glVertex3f(selector_x + selector_size, selector_y, selector_z + cube_height)
            glVertex3f(selector_x + selector_size, selector_y + selector_size, selector_z + cube_height)
            
            glVertex3f(selector_x + selector_size, selector_y + selector_size, selector_z + cube_height)
            glVertex3f(selector_x, selector_y + selector_size, selector_z + cube_height)
            
            glVertex3f(selector_x, selector_y + selector_size, selector_z + cube_height)
            glVertex3f(selector_x, selector_y, selector_z + cube_height)
            
            # Bottom face
            glVertex3f(selector_x, selector_y, selector_z)
            glVertex3f(selector_x + selector_size, selector_y, selector_z)
            
            glVertex3f(selector_x + selector_size, selector_y, selector_z)
            glVertex3f(selector_x + selector_size, selector_y + selector_size, selector_z)
            
            glVertex3f(selector_x + selector_size, selector_y + selector_size, selector_z)
            glVertex3f(selector_x, selector_y + selector_size, selector_z)
            
            glVertex3f(selector_x, selector_y + selector_size, selector_z)
            glVertex3f(selector_x, selector_y, selector_z)
            
            # Connect top and bottom faces
            glVertex3f(selector_x, selector_y, selector_z)
            glVertex3f(selector_x, selector_y, selector_z + cube_height)
            
            glVertex3f(selector_x + selector_size, selector_y, selector_z)
            glVertex3f(selector_x + selector_size, selector_y, selector_z + cube_height)
            
            glVertex3f(selector_x, selector_y + selector_size, selector_z)
            glVertex3f(selector_x, selector_y + selector_size, selector_z + cube_height)
            
            glVertex3f(selector_x + selector_size, selector_y + selector_size, selector_z)
            glVertex3f(selector_x + selector_size, selector_y + selector_size, selector_z + cube_height)
            
            glEnd()
            glLineWidth(1.0)

    def draw_selector(self):
        """Draw the selection indicator in 3D"""
        x = self.selector_pos[2] * self.cell_size  # X coordinate (index 2)
        y = (self.grid_size - 1 - self.selector_pos[1]) * self.cell_size  # Y coordinate inverted
        z = self.selector_pos[0] * self.cell_size  # Z coordinate
        
        # Set color based on selection state
        target_pos_key = f"{self.selector_pos[0]},{self.selector_pos[2]}"
        has_entity_at_exact_pos = False
        
        if target_pos_key in self.entities:
            entity = self.entities[target_pos_key]
            if entity['y'] == self.selector_pos[1]:  # Compare entity's y with selector's y
                has_entity_at_exact_pos = True
        
        if not self.is_selected and has_entity_at_exact_pos:
            # Purple when hovering over an entity
            glColor3f(0.8, 0.0, 0.8)
        elif self.is_selected:
            target_occupied = False
            if target_pos_key in self.entities:
                entity = self.entities[target_pos_key]
                if entity['y'] == self.selector_pos[1]:  # Compare entity's y with target y
                    target_occupied = True
            
            if target_occupied:
                glColor3f(1.0, 1.0, 0.0)  # Yellow for invalid
            else:
                glColor3f(0.0, 1.0, 0.0)  # Green for valid
        else:
            glColor3f(1.0, 1.0, 1.0)  # White for normal
        
        # Draw wireframe cube for selector
        self.draw_cube(x, y, z, self.cell_size, is_entity=False)
    


    def draw_circle_on_plane(self, cx, cy, cz, radius, axis_x, axis_y, axis_z):
        """Draw a circle at the center point on a plane defined by the normal vector"""
        slices = 16  # Number of segments to draw the circle
        glBegin(GL_TRIANGLE_FAN)
        glVertex3f(cx, cy, cz)  # Center of the circle
        
        for i in range(slices + 1):
            angle = 2.0 * math.pi * i / slices
            # Calculate the point on the circle in the appropriate plane
            if axis_x == 1 and axis_y == 0 and axis_z == 0:  # XZ plane (looking from Y axis)
                x = cx + radius * math.cos(angle)
                y = cy
                z = cz + radius * math.sin(angle)
            elif axis_x == 0 and axis_y == 1 and axis_z == 0:  # YZ plane (looking from X axis)
                x = cx
                y = cy + radius * math.cos(angle)
                z = cz + radius * math.sin(angle)
            else:  # Default to XY plane (looking from Z axis)
                x = cx + radius * math.cos(angle)
                y = cy + radius * math.sin(angle)
                z = cz
            
            glVertex3f(x, y, z)
        glEnd()

    def draw_axis_indicator(self):
        """Draw a large XYZ axis indicator in the corner of the 3D view"""
        # Set the size of the axis indicator
        axis_length = self.cell_size * 3.0  # Make it 3x the size of a cell
        
        # Save the current matrix state
        glPushMatrix()
        
        # Position the axis indicator at the actual origin with offset so it's not sitting directly on grid
        glTranslatef(-0.5, -0.5, -0.5)
        
        # Draw X-axis (Red)
        glColor3f(1.0, 0.0, 0.0)  # Red
        glLineWidth(3.0)
        glBegin(GL_LINES)
        glVertex3f(0, 0, 0)
        glVertex3f(axis_length, 0, 0)  # X-axis points right
        glEnd()
        
        # Draw Y-axis (Green)
        glColor3f(0.0, 1.0, 0.0)  # Green
        glBegin(GL_LINES)
        glVertex3f(0, 0, 0)
        glVertex3f(0, axis_length, 0)  # Y-axis points up
        glEnd()
        
        # Draw Z-axis (Blue)
        glColor3f(0.0, 0.0, 1.0)  # Blue
        glBegin(GL_LINES)
        glVertex3f(0, 0, 0)
        glVertex3f(0, 0, axis_length)  # Z-axis points towards viewer
        glEnd()
        
        # Restore the matrix state
        glPopMatrix()

    def keyPressEvent(self, event):
        """Handle keyboard input"""
        key = event.key()
        
        # Check for POV mode toggle first ('0' key)
        if key == ord('0'):
            self.pov_mode = (self.pov_mode + 1) % 3  # Cycle through 0, 1, 2
            print(f"POV Mode: {self.pov_modes[self.pov_mode]}")
            # Update the mode label in the main window to reflect the new POV
            if hasattr(self, 'parent') and hasattr(self.parent, 'mode_label'):
                pov_text = self.pov_modes[self.pov_mode]
                self.parent.mode_label.setText(f"Mode: 3D - POV: {pov_text} (Press '2' for 2D, '0' to cycle POV)")
            self.update()
            # Also update the minimap to reflect the new POV mode
            if hasattr(self, 'parent') and hasattr(self.parent, 'minimap'):
                # Get entity data in the format expected by the minimap
                entities_for_minimap = {}
                for key_val, value in self.entities.items():
                    entities_for_minimap[key_val] = {
                        'x': value['x'],
                        'y': value['y'],  # Added y coordinate
                        'z': value['z'],
                        'color': value['color']
                    }
                self.parent.minimap.update_data(entities_for_minimap, self.selector_pos, self.pov_mode, self.camera_direction)
            return
        
        # Handle WASD keys based on POV mode
        camera_moved = False
        if key == ord('W') or key == ord('w'):
            if self.pov_mode == 2:  # Free camera mode - move camera
                self.camera_x -= math.sin(math.radians(self.camera_angle_y)) * self.move_speed
                self.camera_y += math.cos(math.radians(self.camera_angle_y)) * self.move_speed
                camera_moved = True
            elif self.pov_mode in [0, 1]:  # First or Third person mode - rotate to face forward (+Y)
                self.camera_direction = 0  # Face forward (+Y direction)
                camera_moved = True
                # Update the minimap to reflect the new camera direction
                if hasattr(self, 'parent') and hasattr(self.parent, 'minimap'):
                    # Get entity data in the format expected by the minimap
                    entities_for_minimap = {}
                    for key_val, value in self.entities.items():
                        entities_for_minimap[key_val] = {
                            'x': value['x'],
                            'y': value['y'],  # Added y coordinate
                            'z': value['z'],
                            'color': value['color']
                        }
                    self.parent.minimap.update_data(entities_for_minimap, self.selector_pos, self.pov_mode, self.camera_direction)
        elif key == ord('S') or key == ord('s'):
            if self.pov_mode == 2:  # Free camera mode - move camera
                self.camera_x += math.sin(math.radians(self.camera_angle_y)) * self.move_speed
                self.camera_y -= math.cos(math.radians(self.camera_angle_y)) * self.move_speed
                camera_moved = True
            elif self.pov_mode in [0, 1]:  # First or Third person mode - rotate to face back (-Y)
                self.camera_direction = math.pi  # Face back (-Y direction)
                camera_moved = True
                # Update the minimap to reflect the new camera direction
                if hasattr(self, 'parent') and hasattr(self.parent, 'minimap'):
                    # Get entity data in the format expected by the minimap
                    entities_for_minimap = {}
                    for key_val, value in self.entities.items():
                        entities_for_minimap[key_val] = {
                            'x': value['x'],
                            'y': value['y'],  # Added y coordinate
                            'z': value['z'],
                            'color': value['color']
                        }
                    self.parent.minimap.update_data(entities_for_minimap, self.selector_pos, self.pov_mode, self.camera_direction)
        elif key == ord('A') or key == ord('a'):
            if self.pov_mode == 2:  # Free camera mode - move camera
                self.camera_x -= math.cos(math.radians(self.camera_angle_y)) * self.move_speed
                self.camera_y -= math.sin(math.radians(self.camera_angle_y)) * self.move_speed
                camera_moved = True
            elif self.pov_mode in [0, 1]:  # First or Third person mode - rotate to face left (-X)
                self.camera_direction = -math.pi/2  # Face left (-X direction)
                camera_moved = True
                # Update the minimap to reflect the new camera direction
                if hasattr(self, 'parent') and hasattr(self.parent, 'minimap'):
                    # Get entity data in the format expected by the minimap
                    entities_for_minimap = {}
                    for key_val, value in self.entities.items():
                        entities_for_minimap[key_val] = {
                            'x': value['x'],
                            'y': value['y'],  # Added y coordinate
                            'z': value['z'],
                            'color': value['color']
                        }
                    self.parent.minimap.update_data(entities_for_minimap, self.selector_pos, self.pov_mode, self.camera_direction)
        elif key == ord('D') or key == ord('d'):
            if self.pov_mode == 2:  # Free camera mode - move camera
                self.camera_x += math.cos(math.radians(self.camera_angle_y)) * self.move_speed
                self.camera_y += math.sin(math.radians(self.camera_angle_y)) * self.move_speed
                camera_moved = True
            elif self.pov_mode in [0, 1]:  # First or Third person mode - rotate to face right (+X)
                self.camera_direction = math.pi/2  # Face right (+X direction)
                camera_moved = True
                # Update the minimap to reflect the new camera direction
                if hasattr(self, 'parent') and hasattr(self.parent, 'minimap'):
                    # Get entity data in the format expected by the minimap
                    entities_for_minimap = {}
                    for key_val, value in self.entities.items():
                        entities_for_minimap[key_val] = {
                            'x': value['x'],
                            'y': value['y'],  # Added y coordinate
                            'z': value['z'],
                            'color': value['color']
                        }
                    self.parent.minimap.update_data(entities_for_minimap, self.selector_pos, self.pov_mode, self.camera_direction)
        elif key == ord('F') or key == ord('f'):  # F key to toggle entity rotation
            self.rotate_entities = not self.rotate_entities
            print(f"Entity rotation: {'ON' if self.rotate_entities else 'OFF'}")
            self.update()
        elif key == ord('Q') or key == ord('q'):  # Q key to turn left
            if self.pov_mode in [0, 1]:  # For first and third person mode
                self.camera_direction -= math.pi / 8  # Turn 22.5 degrees left
                self.update()
                # Also update the minimap to reflect the new camera direction
                if hasattr(self, 'parent') and hasattr(self.parent, 'minimap'):
                    # Get entity data in the format expected by the minimap
                    entities_for_minimap = {}
                    for key_val, value in self.entities.items():
                        entities_for_minimap[key_val] = {
                            'x': value['x'],
                            'y': value['y'],  # Added y coordinate
                            'z': value['z'],
                            'color': value['color']
                        }
                    self.parent.minimap.update_data(entities_for_minimap, self.selector_pos, self.pov_mode, self.camera_direction)
        elif key == ord('E') or key == ord('e'):  # E key to turn right
            if self.pov_mode in [0, 1]:  # For first and third person mode
                self.camera_direction += math.pi / 8  # Turn 22.5 degrees right
                self.update()
                # Also update the minimap to reflect the new camera direction
                if hasattr(self, 'parent') and hasattr(self.parent, 'minimap'):
                    # Get entity data in the format expected by the minimap
                    entities_for_minimap = {}
                    for key_val, value in self.entities.items():
                        entities_for_minimap[key_val] = {
                            'x': value['x'],
                            'y': value['y'],  # Added y coordinate
                            'z': value['z'],
                            'color': value['color']
                        }
                    self.parent.minimap.update_data(entities_for_minimap, self.selector_pos, self.pov_mode, self.camera_direction)
        elif key == Qt.Key_Return or key == Qt.Key_Enter:  # Enter key to select/deselect or confirm move
            target_pos_key = f"{self.selector_pos[0]},{self.selector_pos[2]}"
            
            if not self.is_selected:
                # Attempt to select an entity at the X,Z coordinates
                if target_pos_key in self.entities:
                    self.is_selected = True
                    self.selected_entity_key_when_picked_up = target_pos_key
                    self.selected_entity_data = self.entities.pop(target_pos_key)
            else:
                # Confirm move or deselect
                if target_pos_key in self.entities:
                    self.entities[self.selected_entity_key_when_picked_up] = self.selected_entity_data
                else:
                    self.selected_entity_data['z'] = self.selector_pos[0]
                    self.selected_entity_data['x'] = self.selector_pos[2]
                    self.selected_entity_data['y'] = self.selector_pos[1]
                    self.entities[target_pos_key] = self.selected_entity_data
                
                # Reset selection state
                self.is_selected = False
                self.selected_entity_key_when_picked_up = None
                self.selected_entity_data = None
            
            self.update()
        elif key == Qt.Key_Space:  # Alternative to Enter for select/deselect/confirm
            target_pos_key = f"{self.selector_pos[0]},{self.selector_pos[2]}"
            if not self.is_selected:
                if target_pos_key in self.entities:
                    self.is_selected = True
                    self.selected_entity_key_when_picked_up = target_pos_key
                    self.selected_entity_data = self.entities.pop(target_pos_key)
            else:
                if target_pos_key in self.entities:
                    self.entities[self.selected_entity_key_when_picked_up] = self.selected_entity_data
                else:
                    self.selected_entity_data['z'] = self.selector_pos[0]
                    self.selected_entity_data['x'] = self.selector_pos[2]
                    self.selected_entity_data['y'] = self.selector_pos[1]
                    self.entities[target_pos_key] = self.selected_entity_data
                
                # Reset selection state
                self.is_selected = False
                self.selected_entity_key_when_picked_up = None
                self.selected_entity_data = None
            self.update()
            return
        else:
            # Movement keys
            old_selector_pos = self.selector_pos[:]
            new_pos = old_selector_pos[:]
            
            # For first and third-person modes, movement should be relative to camera direction
            if self.pov_mode in [0, 1]:  # First or Third person mode
                if key == Qt.Key_Up:  # Move forward in facing direction
                    # Calculate movement based on camera direction
                    delta_x = math.sin(self.camera_direction)
                    delta_y = math.cos(self.camera_direction)
                    
                    # Since Y is inverted in rendering (lower grid Y appears higher), 
                    # we need to account for this in grid coordinate changes
                    new_grid_x = self.selector_pos[2] + round(delta_x)
                    new_grid_y = self.selector_pos[1] - round(delta_y)  # Negative because of inverted Y axis
                    
                    # Apply boundaries
                    if 0 <= new_grid_x < self.grid_size:
                        new_pos[2] = int(new_grid_x)
                    if 0 <= new_grid_y < self.grid_size:
                        new_pos[1] = int(new_grid_y)
                        
                elif key == Qt.Key_Down:  # Move backward
                    delta_x = math.sin(self.camera_direction)
                    delta_y = math.cos(self.camera_direction)
                    
                    new_grid_x = self.selector_pos[2] - round(delta_x)
                    new_grid_y = self.selector_pos[1] + round(delta_y)  # Positive Y in opposite direction
                    
                    # Apply boundaries
                    if 0 <= new_grid_x < self.grid_size:
                        new_pos[2] = int(new_grid_x)
                    if 0 <= new_grid_y < self.grid_size:
                        new_pos[1] = int(new_grid_y)
                        
                elif key == Qt.Key_Left:  # Strafe left
                    # Move perpendicular 90 degrees CCW from facing direction
                    delta_x = math.sin(self.camera_direction - math.pi/2) 
                    delta_y = math.cos(self.camera_direction - math.pi/2)
                    
                    new_grid_x = self.selector_pos[2] + round(delta_x)
                    new_grid_y = self.selector_pos[1] - round(delta_y)
                    
                    # Apply boundaries
                    if 0 <= new_grid_x < self.grid_size:
                        new_pos[2] = int(new_grid_x)
                    if 0 <= new_grid_y < self.grid_size:
                        new_pos[1] = int(new_grid_y)
                        
                elif key == Qt.Key_Right:  # Strafe right
                    # Move perpendicular 90 degrees CW from facing direction  
                    delta_x = math.sin(self.camera_direction + math.pi/2) 
                    delta_y = math.cos(self.camera_direction + math.pi/2)
                    
                    new_grid_x = self.selector_pos[2] + round(delta_x)
                    new_grid_y = self.selector_pos[1] - round(delta_y)
                    
                    # Apply boundaries
                    if 0 <= new_grid_x < self.grid_size:
                        new_pos[2] = int(new_grid_x)
                    if 0 <= new_grid_y < self.grid_size:
                        new_pos[1] = int(new_grid_y)
                elif key == Qt.Key_Z and self.selector_pos[0] > 0:  # Z key to move down in Z-axis
                    new_pos[0] -= 1
                elif key == Qt.Key_X and self.selector_pos[0] < self.grid_size - 1:  # X key to move up in Z-axis
                    new_pos[0] += 1
            else:
                # For third-person and free camera modes, use original movement
                if key == Qt.Key_Up and self.selector_pos[1] > 0:
                    new_pos[1] -= 1  # Moving up decreases Y to compensate for inverted drawing
                elif key == Qt.Key_Down and self.selector_pos[1] < self.grid_size - 1:
                    new_pos[1] += 1  # Moving down increases Y to compensate for inverted drawing
                elif key == Qt.Key_Left and self.selector_pos[2] > 0:
                    new_pos[2] -= 1  # Moving left decreases X
                elif key == Qt.Key_Right and self.selector_pos[2] < self.grid_size - 1:
                    new_pos[2] += 1  # Moving right increases X
                elif key == Qt.Key_Z and self.selector_pos[0] > 0:  # Z key to move down in Z-axis
                    new_pos[0] -= 1
                elif key == Qt.Key_X and self.selector_pos[0] < self.grid_size - 1:  # X key to move up in Z-axis
                    new_pos[0] += 1
            
            # Only update selector position if it actually changed
            if new_pos != old_selector_pos:
                self.selector_pos = new_pos
                camera_moved = True  # Update display if selector moved
        
        # Update the display after any action
        if camera_moved:
            self.update()
        
        # Also update the minimap for all changes that affect position
        # (movement keys, selection, etc.)
        if (camera_moved or 
            key in [Qt.Key_Up, Qt.Key_Down, Qt.Key_Left, Qt.Key_Right, Qt.Key_Z, Qt.Key_X] or
            key == Qt.Key_Return or key == Qt.Key_Enter or key == Qt.Key_Space):
            if hasattr(self, 'parent') and hasattr(self.parent, 'minimap'):
                # Get entity data in the format expected by the minimap
                entities_for_minimap = {}
                for key_val, value in self.entities.items():
                    entities_for_minimap[key_val] = {
                        'x': value['x'],
                        'y': value['y'],  # Added y coordinate
                        'z': value['z'],
                        'color': value['color']
                    }
                self.parent.minimap.update_data(entities_for_minimap, self.selector_pos, self.pov_mode, self.camera_direction)


def main():
    app = QApplication(sys.argv)
    
    window = HybridViewer()
    window.show()
    
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
