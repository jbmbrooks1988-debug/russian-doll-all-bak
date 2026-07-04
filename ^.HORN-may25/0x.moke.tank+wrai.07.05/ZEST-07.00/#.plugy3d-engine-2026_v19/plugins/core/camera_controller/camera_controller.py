"""
Core Camera Controller Plugin
Manages camera controls and POV modes with correct coordinate system
"""
import math
from PySide6.QtCore import Qt, QPoint, QEvent
from PySide6.QtGui import QMouseEvent, QWheelEvent, QKeyEvent
from OpenGL.GL import *
from OpenGL.GLU import *


class CameraControllerPlugin:
    """Plugin for handling camera movement and POV modes with correct coordinates"""
    def __init__(self, main_app):
        self.main_app = main_app
        self.voxel_grid = None
        
        # Camera state for free camera (orbit style)
        self.zoom = 30.0
        self.pitch = 90.0  # Look straight up (to face green top of cube, y+)
        self.yaw = 0.0     # Look toward +Z (front) direction, to have legend cube at top-left
        self.target = [0.0, 0.0, 0.0] # Target point for orbit
        self.last_mouse_pos = None

        # POV mode state
        self.pov_mode = 2  # 0: third-person, 1: first-person, 2: free-camera
        self.pov_modes = ["Third Person", "First Person", "Free Camera"]
        self.camera_direction = -math.pi / 2
        
        # Movement speed
        self.move_speed = 0.5
        self.turn_speed = math.pi / 16

    def initialize(self, voxel_grid_widget):
        """Initialize the camera controller with reference to voxel grid"""
        self.voxel_grid = voxel_grid_widget
        self.voxel_grid.camera = self
        
        # Center the orbit camera on the grid's floor
        grid_center = self.voxel_grid.grid_size * self.voxel_grid.cell_size / 2.0
        self.target = [grid_center, 0.0, grid_center]
        
        print("Camera Controller Plugin initialized")
        return True

    def handle_event(self, event):
        if isinstance(event, QKeyEvent) and event.type() == QEvent.KeyPress:
            return self.handle_key_event(event.key())
        elif isinstance(event, QMouseEvent):
            if event.type() == QEvent.MouseButtonPress:
                return self.handle_mouse_press_event(event)
            elif event.type() == QEvent.MouseMove:
                return self.handle_mouse_move_event(event)
            elif event.type() == QEvent.MouseButtonRelease:
                return self.handle_mouse_release_event(event)
        elif isinstance(event, QWheelEvent):
            return self.handle_wheel_event(event)
        return False

    def handle_key_event(self, key):
        if not self.voxel_grid:
            return False
            
        if key == Qt.Key_0:
            self.cycle_pov_mode()
            return True
            
        if self.pov_mode == 2: # Free Camera
            if key == Qt.Key_W:
                self.target[2] -= self.move_speed # Pan forward
            elif key == Qt.Key_S:
                self.target[2] += self.move_speed # Pan backward
            elif key == Qt.Key_A:
                self.target[0] -= self.move_speed # Pan left
            elif key == Qt.Key_D:
                self.target[0] += self.move_speed # Pan right
            else:
                return False
            self.voxel_grid.update()
            # Note: Camera movement doesn't typically need UI updates for HUD
            return True
        elif self.pov_mode in [0, 1]: # First/Third Person
            if key == Qt.Key_Q:
                self.camera_direction -= self.turn_speed
                self.voxel_grid.update()
                return True
            elif key == Qt.Key_E:
                self.camera_direction += self.turn_speed
                self.voxel_grid.update()
                return True

        return False

    def handle_mouse_press_event(self, event: QMouseEvent):
        if self.pov_mode == 2: # Free camera rotation
            self.last_mouse_pos = event.position().toPoint()
            return True
        return False

    def handle_mouse_move_event(self, event: QMouseEvent):
        if self.pov_mode == 2 and self.last_mouse_pos is not None:
            dx = event.position().toPoint().x() - self.last_mouse_pos.x()
            dy = event.position().toPoint().y() - self.last_mouse_pos.y()
            
            self.yaw += dx * 0.5
            self.pitch += dy * 0.5
            
            self.last_mouse_pos = event.position().toPoint()
            self.voxel_grid.update()
            return True
        return False

    def handle_mouse_release_event(self, event: QMouseEvent):
        if self.pov_mode == 2:
            self.last_mouse_pos = None
            return True
        return False

    def handle_wheel_event(self, event: QWheelEvent):
        if self.pov_mode == 2:
            self.zoom -= event.angleDelta().y() / 120.0
            self.zoom = max(1.0, self.zoom)
            self.voxel_grid.update()
            return True
        return False

    def cycle_pov_mode(self):
        self.pov_mode = (self.pov_mode + 1) % 3
        print(f"POV Mode: {self.pov_modes[self.pov_mode]}")
        self.voxel_grid.update()
        # Update UI elements to reflect new POV mode
        if hasattr(self.voxel_grid, 'update_ui_elements'):
            self.voxel_grid.update_ui_elements()

    def apply_camera_transformations(self):
        if not self.voxel_grid:
            return

        if self.pov_mode == 2:  # Free Camera (Orbit)
            glTranslatef(0, 0, -self.zoom)
            glRotatef(self.pitch, 1, 0, 0)
            glRotatef(self.yaw, 0, 1, 0)
            glTranslatef(-self.target[0], -self.target[1], -self.target[2])

        else: # First and Third Person
            selector_pos = self.voxel_grid.selector_pos
            cell_size = self.voxel_grid.cell_size

            target_x = selector_pos[0] * cell_size + cell_size / 2
            target_y = selector_pos[1] * cell_size + cell_size / 2
            target_z = selector_pos[2] * cell_size + cell_size / 2

            if self.pov_mode == 1: # First Person
                eye_height = cell_size * 0.8
                center_y = target_y + eye_height
                gluLookAt(target_x, center_y, target_z,
                          target_x + math.cos(self.camera_direction), center_y, target_z + math.sin(self.camera_direction),
                          0, 1, 0)

            elif self.pov_mode == 0: # Third Person
                distance = 8 * cell_size
                height = 5 * cell_size
                
                cam_x = target_x - distance * math.cos(self.camera_direction)
                cam_z = target_z - distance * math.sin(self.camera_direction)
                cam_y = target_y + height
                
                gluLookAt(cam_x, cam_y, cam_z,
                          target_x, target_y, target_z,
                          0, 1, 0)

Plugin = CameraControllerPlugin