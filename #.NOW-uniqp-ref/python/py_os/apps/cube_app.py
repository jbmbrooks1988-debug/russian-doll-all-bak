"""
3D Cube Application for PyOS
A 3D visualization app using OpenGL (based on hybrid_clean.py pattern)
"""

from PySide6.QtWidgets import QVBoxLayout, QWidget, QLabel, QSizePolicy
from PySide6.QtCore import QTimer
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from OpenGL.GL import *
from OpenGL.GLU import *

from apps.base_app import BaseApplication


class CubeWidget(QOpenGLWidget):
    def __init__(self):
        super().__init__()
        
        self.rotation_x = 0
        self.rotation_y = 0
        self.rotation_z = 0
        
        # Timer for animation
        self.timer = QTimer()
        self.timer.timeout.connect(self.rotate_cube)
        self.timer.start(30)  # Update every ~30ms for ~33 FPS
    
    def initializeGL(self):
        """Initialize OpenGL settings"""
        glClearColor(0.0, 0.0, 0.0, 1.0)  # Black background
        glEnable(GL_DEPTH_TEST)  # Enable depth testing
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
        glEnable(GL_CULL_FACE)
        glCullFace(GL_BACK)
        glShadeModel(GL_SMOOTH)
    
    def resizeGL(self, width, height):
        """Handle window resize"""
        glViewport(0, 0, width, height)
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        gluPerspective(45, width / height, 0.1, 100.0)
        glMatrixMode(GL_MODELVIEW)
    
    def paintGL(self):
        """Render the scene"""
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()
        
        # Move the cube back so we can see it
        glTranslatef(0.0, 0.0, -5.0)
        
        # Apply rotation
        glRotatef(self.rotation_x, 1, 0, 0)  # Rotate around X-axis
        glRotatef(self.rotation_y, 0, 1, 0)  # Rotate around Y-axis
        glRotatef(self.rotation_z, 0, 0, 1)  # Rotate around Z-axis
        
        # Draw the cube
        self.draw_cube()
    
    def draw_cube(self):
        """Draw a simple colored cube"""
        # Define the 8 vertices of a unit cube centered at origin
        vertices = [
            -1, -1, -1,  # Vertex 0: Back bottom left
             1, -1, -1,  # Vertex 1: Back bottom right
             1,  1, -1,  # Vertex 2: Back top right
            -1,  1, -1,  # Vertex 3: Back top left
            -1, -1,  1,  # Vertex 4: Front bottom left
             1, -1,  1,  # Vertex 5: Front bottom right
             1,  1,  1,  # Vertex 6: Front top right
            -1,  1,  1   # Vertex 7: Front top left
        ]
        
        # Define the 6 faces of the cube (each as 4 vertex indices)
        faces = [
            [0, 1, 2, 3],  # Back face (z = -1)
            [4, 5, 6, 7],  # Front face (z = 1)
            [0, 4, 7, 3],  # Left face (x = -1)
            [1, 2, 6, 5],  # Right face (x = 1)
            [3, 2, 6, 7],  # Top face (y = 1)
            [0, 1, 5, 4]   # Bottom face (y = -1)
        ]
        
        # Colors for each face
        colors = [
            [1, 0, 0, 0.8], [0, 1, 0, 0.8], [0, 0, 1, 0.8],  # Red, Green, Blue (back/left/top)
            [1, 1, 0, 0.8], [1, 0, 1, 0.8], [0, 1, 1, 0.8]   # Yellow, Magenta, Cyan (front/right/bottom)
        ]
        
        # Draw each face with transparency
        for i, face in enumerate(faces):
            glColor4f(*colors[i])
            glBegin(GL_QUADS)
            for vertex_index in face:
                glVertex3f(vertices[vertex_index*3], vertices[vertex_index*3+1], vertices[vertex_index*3+2])
            glEnd()
        
        # Draw edges for better visibility
        glColor3f(1, 1, 1)  # White edges
        glLineWidth(2.0)
        edges = [
            (0, 1), (1, 2), (2, 3), (3, 0),  # Back face
            (4, 5), (5, 6), (6, 7), (7, 4),  # Front face
            (0, 4), (1, 5), (2, 6), (3, 7)   # Connecting edges
        ]
        glBegin(GL_LINES)
        for start_idx, end_idx in edges:
            glVertex3f(vertices[start_idx*3], vertices[start_idx*3+1], vertices[start_idx*3+2])
            glVertex3f(vertices[end_idx*3], vertices[end_idx*3+1], vertices[end_idx*3+2])
        glEnd()
        glLineWidth(1.0)
    
    def rotate_cube(self):
        """Update rotation values"""
        self.rotation_x += 1.0
        self.rotation_y += 1.5
        self.rotation_z += 0.5
        self.update()


class CubeApp(BaseApplication):
    def setup_ui(self):
        """Setup the 3D cube application UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        
        # Title
        title_label = QLabel("3D Cube Visualization")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold; color: #ddd;")
        layout.addWidget(title_label)
        
        # Description
        desc_label = QLabel("A rotating 3D cube rendered with OpenGL")
        desc_label.setStyleSheet("color: #aaa; margin-top: 5px;")
        layout.addWidget(desc_label)
        
        # Create OpenGL widget for the cube (use the same pattern as hybrid_clean.py)
        self.cube_widget = CubeWidget()
        self.cube_widget.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        layout.addWidget(self.cube_widget)
        
        # Controls info
        controls_label = QLabel("\nControls: The cube rotates automatically")
        controls_label.setStyleSheet("color: #aaa; font-style: italic;")
        layout.addWidget(controls_label)
        
        # Add stretch to push content up
        layout.addStretch()