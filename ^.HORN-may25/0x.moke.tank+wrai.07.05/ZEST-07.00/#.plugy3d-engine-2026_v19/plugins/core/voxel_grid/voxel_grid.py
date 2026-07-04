"""
Core Voxel Grid Plugin
Based on the working reference implementation with correct coordinate system
"""

import sys
import math
from PySide6.QtWidgets import QApplication, QMainWindow, QWidget, QVBoxLayout
from PySide6.QtCore import Qt, QPoint
from PySide6.QtGui import QMouseEvent, QPainter, QColor, QFont
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from OpenGL.GL import *
from OpenGL.GLU import *

class UIElement:
    """Base class for UI elements."""
    def __init__(self, x=0, y=0, width=0, height=0, z_order=0, element_id=None):
        self.x, self.y, self.width, self.height = x, y, width, height
        self.z_order, self.element_id = z_order, element_id
        self.visible = True
    def draw(self, widget): pass

class TranslucentRect(UIElement):
    """Translucent rectangle UI element."""
    def __init__(self, x=0, y=0, width=0, height=0, opacity=0.5, color=(0.0, 0.0, 0.0), element_id=None, border_enabled=True, border_color=(1.0, 1.0, 1.0)):
        super().__init__(x, y, width, height, element_id=element_id)
        self.opacity, self.color = opacity, color
        self.border_enabled, self.border_color = border_enabled, border_color
    def draw(self, widget):
        widget.draw_translucent_rect_opengl(self.x, self.y, self.width, self.height, self.opacity, self.color)
        if self.border_enabled:
            widget.draw_rect_border_opengl(self.x, self.y, self.width, self.height, self.border_color)

class TextElement(UIElement):
    """Text UI element."""
    def __init__(self, x=0, y=0, text="", font_size=10, color=QColor(255, 255, 255), element_id=None):
        super().__init__(x, y, 0, 0, element_id=element_id)
        self.text, self.font_size, self.color = text, font_size, color
        self.font = QFont("Arial", font_size, QFont.Bold)
    def draw(self, widget): pass

class VoxelGridPlugin(QOpenGLWidget):
    """Main class for the voxel grid plugin, now also the widget"""
    def __init__(self, main_app):
        super().__init__()
        self.main_app = main_app
        self.camera = None # Will be set by the camera controller
        
        # Grid setup
        self.grid_size = 12
        self.cell_size = 1.0
        
        # Selector position [x, y, z] using proper coordinate system
        self.selector_pos = [0, 0, 0]
        
        # Entity storage
        self.entities = {}
        self.is_selected = False
        self.selected_entity_data = None
        
        # UI Elements
        self.ui_elements = []

        # Focus policy for keyboard events
        self.setFocusPolicy(Qt.StrongFocus)

    def initialize(self):
        """Initialize the voxel grid plugin"""
        print("Voxel Grid Plugin initialized")
        return self

    def initializeGL(self):
        """Initialize OpenGL settings based on reference implementation"""
        glClearColor(0.1, 0.1, 0.15, 1.0)  # Darker background
        glEnable(GL_DEPTH_TEST)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
        glDisable(GL_CULL_FACE)
        glShadeModel(GL_SMOOTH)

    def resizeGL(self, width, height):
        """Handle window resize"""
        glViewport(0, 0, width, height)
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        gluPerspective(45, width / height, 0.1, 100.0)
        glMatrixMode(GL_MODELVIEW)

    def paintGL(self):
        """Render the 3D scene"""
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()
        
        # Apply camera transformations from the camera controller
        if self.camera:
            self.camera.apply_camera_transformations()
        
        # Draw coordinate reference (legend cube) at origin
        self.draw_coordinate_reference()
        
        # Draw the grid
        self.draw_grid()
        
        # Draw entities
        self.draw_entities()
        
        # Draw selector at current position
        self.draw_selector()

        # Draw UI elements
        self._draw_ui_elements()

    def update_ui_elements(self):
        """Update all UI elements with current data - used by selector and control plugins"""
        # This updates TextElement UI elements with current position and mode data
        pos_text_elem = next((elem for elem in self.ui_elements 
                             if isinstance(elem, TextElement) and 
                             hasattr(elem, 'text') and 
                             ('Selector:' in elem.text or 'Selector X' in elem.text)), 
                            None)
        if pos_text_elem and hasattr(self, 'selector_pos'):
            pos = self.selector_pos
            pos_text_elem.text = f"Selector: X:{pos[0]}, Y:{pos[1]}, Z:{pos[2]}"
        
        pov_text_elem = next((elem for elem in self.ui_elements 
                             if isinstance(elem, TextElement) and 
                             elem.text.startswith("POV Mode:")), 
                            None)
        if pov_text_elem and hasattr(self, 'camera') and self.camera:
            pov_mode_text = self.camera.pov_modes[self.camera.pov_mode]
            pov_text_elem.text = f"POV Mode: {pov_mode_text}"
        
        # Update the display
        self.update()

    def _draw_ui_elements(self):
        """Draw all UI elements respecting z_order across all types using a DOM-like hierarchy."""
        visible_elements = [elem for elem in self.ui_elements if elem.visible]
        
        # Sort all elements by z_order to create a proper DOM-like hierarchy
        all_elements_sorted = sorted(visible_elements, key=lambda e: e.z_order)
        
        # Because we have elements that use different rendering contexts (OpenGL vs QPainter),
        # we'll process them in z_order but group by rendering type for efficiency
        # However, to respect true z-ordering, we must ensure the rendering order is accurate
        
        # Split into elements by type, maintaining the sorted order
        opengl_elements = []  # TranslucentRect elements
        text_elements = []    # TextElement elements
        
        for element in all_elements_sorted:
            if isinstance(element, TranslucentRect):
                opengl_elements.append(element)
            elif isinstance(element, TextElement):
                text_elements.append(element)
        
        # Draw OpenGL elements (TranslucentRect) using OpenGL
        for element in opengl_elements:
            element.draw(self)
        
        # Draw Text elements using QPainter
        if text_elements:
            painter = QPainter(self)
            painter.setRenderHint(QPainter.Antialiasing, False)
            painter.setRenderHint(QPainter.TextAntialiasing, True)
            
            # Sort text elements by z_order again to ensure proper layering within text elements
            text_elements_sorted = sorted(text_elements, key=lambda e: e.z_order)
            for element in text_elements_sorted:
                painter.setFont(element.font)
                painter.setPen(element.color)
                painter.drawText(element.x, element.y, element.text)
            
            painter.end()

    def draw_translucent_rect_opengl(self, x, y, width, height, opacity, color=(0.0, 0.0, 0.0)):
        """Draw a translucent rectangle using OpenGL commands."""
        viewport = glGetIntegerv(GL_VIEWPORT)
        win_width, win_height = viewport[2], viewport[3]
        
        glPushAttrib(GL_ALL_ATTRIB_BITS)
        glMatrixMode(GL_PROJECTION)
        glPushMatrix()
        glLoadIdentity()
        glOrtho(0, win_width, win_height, 0, -1, 1)
        glMatrixMode(GL_MODELVIEW)
        glPushMatrix()
        glLoadIdentity()
        
        glDisable(GL_DEPTH_TEST)
        glDepthMask(GL_FALSE)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
        
        glColor4f(color[0], color[1], color[2], opacity)
        glBegin(GL_QUADS)
        glVertex2f(x, y)
        glVertex2f(x + width, y)
        glVertex2f(x + width, y + height)
        glVertex2f(x, y + height)
        glEnd()
        
        glPopAttrib()
        glMatrixMode(GL_PROJECTION)
        glPopMatrix()
        glMatrixMode(GL_MODELVIEW)
        glPopMatrix()

    def draw_rect_border_opengl(self, x, y, width, height, color):
        """Draw a border around a rectangle using OpenGL commands."""
        viewport = glGetIntegerv(GL_VIEWPORT)
        win_width, win_height = viewport[2], viewport[3]
        
        glPushAttrib(GL_ALL_ATTRIB_BITS)
        glMatrixMode(GL_PROJECTION)
        glPushMatrix()
        glLoadIdentity()
        glOrtho(0, win_width, win_height, 0, -1, 1)
        glMatrixMode(GL_MODELVIEW)
        glPushMatrix()
        glLoadIdentity()
        
        glDisable(GL_DEPTH_TEST)
        glDepthMask(GL_FALSE)
        
        glColor3f(color[0], color[1], color[2])
        glLineWidth(2.0)
        glBegin(GL_LINE_LOOP)
        glVertex2f(x, y)
        glVertex2f(x + width, y)
        glVertex2f(x + width, y + height)
        glVertex2f(x, y + height)
        glEnd()
        glLineWidth(1.0)
        
        glPopAttrib()
        glMatrixMode(GL_PROJECTION)
        glPopMatrix()
        glMatrixMode(GL_MODELVIEW)
        glPopMatrix()

    def draw_coordinate_reference(self):
        """Draw the reference cube and axis indicators with offset to avoid grid interference"""
        # Position legend cube at top-left when viewing from top (looking at green face, y+)
        # With yaw=0 (looking toward +Z), -X is to the left and +Z is toward the back (top of screen)
        offset_x = -2.5  # Left side of the grid (from viewer perspective)
        offset_y = 1.5   # Above the grid level (since we're looking down at the top face)
        offset_z = -2.5  # Back side of the grid (top of screen from this viewing angle)
        
        glPushMatrix()
        glTranslatef(offset_x, offset_y, offset_z)
        self.draw_reference_cube()
        self.draw_axis_indicators()
        glPopMatrix()

    def draw_reference_cube(self):
        """Draw the colored reference cube with correct face orientations based on user's truth"""
        size = 0.5
        # Vertices from the original reference colored_cube_renderer.py
        vertices = [
            [-size, -size, -size],  # Vertex 0: back-bottom-left  (X-, Y-, Z-)
            [size, -size, -size],   # Vertex 1: back-bottom-right (X+, Y-, Z-)
            [size, size, -size],    # Vertex 2: back-top-right    (X+, Y+, Z-)
            [-size, size, -size],   # Vertex 3: back-top-left     (X-, Y+, Z-)
            [-size, -size, size],   # Vertex 4: front-bottom-left (X-, Y-, Z+)
            [size, -size, size],    # Vertex 5: front-bottom-right(X+, Y-, Z+)
            [size, size, size],     # Vertex 6: front-top-right   (X+, Y+, Z+)
            [-size, size, size]     # Vertex 7: front-top-left    (X-, Y+, Z+)
        ]
        
        # Faces and colors based on user's latest truth and reference code
        # Face order: +Y (top), -Y (bottom), +Z (back), -Z (front/eyes), +X (right), -X (left)
        faces = [
            {  # Top face (+Y) - Green
                'vertices': [3, 2, 6, 7], # Y+ vertices
                'color': [0.0, 1.0, 0.0]  # Green
            },
            {  # Bottom face (-Y) - Brown
                'vertices': [0, 1, 5, 4], # Y- vertices
                'color': [0.6, 0.4, 0.2]  # Brown
            },
            {  # Back face (+Z) - Black (user's explicit instruction)
                'vertices': [4, 5, 6, 7], # Z+ vertices
                'color': [0.0, 0.0, 0.0]  # Black
            },
            {  # Front face (-Z) - White/Eyes (user's explicit instruction)
                'vertices': [0, 1, 2, 3], # Z- vertices
                'color': [1.0, 1.0, 1.0]  # White
            },
            {  # Right face (+X) - Red (user's explicit instruction)
                'vertices': [1, 5, 6, 2], # X+ vertices
                'color': [1.0, 0.0, 0.0]  # Red
            },
            {  # Left face (-X) - Blue (user's explicit instruction)
                'vertices': [4, 0, 3, 7], # X- vertices
                'color': [0.0, 0.0, 1.0]  # Blue
            }
        ]
        
        glBegin(GL_QUADS)
        for face in faces:
            glColor3fv(face['color'])
            for vertex_index in face['vertices']:
                glVertex3fv(vertices[vertex_index])
        glEnd()

    def draw_grid(self):
        """Draw the 3D grid"""
        glColor3f(0.3, 0.3, 0.3)
        glLineWidth(1.0)
        glBegin(GL_LINES)
        for i in range(self.grid_size + 1):
            glVertex3f(i * self.cell_size, 0, 0)
            glVertex3f(i * self.cell_size, 0, self.grid_size * self.cell_size)
            glVertex3f(0, 0, i * self.cell_size)
            glVertex3f(self.grid_size * self.cell_size, 0, i * self.cell_size)
        glEnd()

    def draw_entities(self):
        """Draw 3D entities"""
        # This will be handled by an entity renderer plugin
        pass

    def draw_selector(self):
        """Draw selector at current position with color coding."""
        x, y, z = self.selector_pos
        
        # Determine selector color
        pos_key = f"{x},{y},{z}"
        
        if not self.is_selected and pos_key in self.entities:
            # Purple when over a selectable entity
            glColor3f(0.8, 0.0, 0.8)
        elif self.is_selected:
            if pos_key in self.entities:
                # Yellow for invalid (occupied) move target
                glColor3f(1.0, 1.0, 0.0)
            else:
                # Green for valid move target
                glColor3f(0.0, 1.0, 0.0)
        else:
            # White for default selector
            glColor3f(1.0, 1.0, 1.0)

        glPushMatrix()
        glTranslatef(
            x * self.cell_size + self.cell_size / 2, 
            y * self.cell_size + self.cell_size / 2, 
            z * self.cell_size + self.cell_size / 2
        )
        self.draw_wireframe_cube(0, 0, 0, self.cell_size * 1.1) # Slightly larger
        glPopMatrix()

    def draw_wireframe_cube(self, x, y, z, size):
        """Draw a wireframe cube"""
        half = size / 2
        glBegin(GL_LINES)
        # Define 8 vertices
        v = [[x-half, y-half, z-half], [x+half, y-half, z-half], [x+half, y+half, z-half], [x-half, y+half, z-half],
             [x-half, y-half, z+half], [x+half, y-half, z+half], [x+half, y+half, z+half], [x-half, y+half, z+half]]
        # Draw 12 edges
        edges = [(0,1), (1,2), (2,3), (3,0), (4,5), (5,6), (6,7), (7,4), (0,4), (1,5), (2,6), (3,7)]
        for edge in edges:
            glVertex3fv(v[edge[0]])
            glVertex3fv(v[edge[1]])
        glEnd()

    def draw_axis_indicators(self):
        """Draw axis indicators at origin"""
        axis_length = 1.0
        glLineWidth(2.0)
        glBegin(GL_LINES)
        # X-axis (Red)
        glColor3f(1.0, 0.0, 0.0)
        glVertex3f(0, 0, 0)
        glVertex3f(axis_length, 0, 0)
        # Y-axis (Green)
        glColor3f(0.0, 1.0, 0.0)
        glVertex3f(0, 0, 0)
        glVertex3f(0, axis_length, 0)
        # Z-axis (Blue)
        glColor3f(0.0, 0.0, 1.0)
        glVertex3f(0, 0, 0)
        glVertex3f(0, 0, axis_length)
        glEnd()
        glLineWidth(1.0)

    def set_selector_position(self, x, y, z):
        """Set the selector position"""
        self.selector_pos[0] = max(0, min(int(x), self.grid_size - 1))
        self.selector_pos[1] = max(0, min(int(y), self.grid_size - 1))
        self.selector_pos[2] = max(0, min(int(z), self.grid_size - 1))
        self.update()

    def mousePressEvent(self, event: QMouseEvent):
        if self.camera and hasattr(self.camera, 'handle_mouse_press_event'):
            if self.camera.handle_mouse_press_event(event):
                return
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event: QMouseEvent):
        if self.camera and hasattr(self.camera, 'handle_mouse_move_event'):
            if self.camera.handle_mouse_move_event(event):
                return
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event: QMouseEvent):
        if self.camera and hasattr(self.camera, 'handle_mouse_release_event'):
            if self.camera.handle_mouse_release_event(event):
                return
        super().mouseReleaseEvent(event)