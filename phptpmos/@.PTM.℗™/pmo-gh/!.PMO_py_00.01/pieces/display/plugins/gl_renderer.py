#!/usr/bin/env python3
"""
GL Renderer - Python Version
Pixel-perfect port of the C GL renderer's history and scrolling logic.
"""

import os
import sys
import time
from datetime import datetime

# Path resolution to find path_utils
current_dir = os.path.dirname(os.path.abspath(__file__))
# pieces is 3 levels up from plugins
pieces_dir = os.path.abspath(os.path.join(current_dir, "../../..")) 
sys.path.insert(0, pieces_dir)

try:
    from PySide6.QtWidgets import QApplication, QMainWindow
    from PySide6.QtCore import Qt, QTimer, QPointF
    from PySide6.QtOpenGLWidgets import QOpenGLWidget
    from OpenGL.GL import *
    from OpenGL.GLU import *
    from OpenGL.GLUT import *
except ImportError as e:
    print(f"GL Import Error: {e}", file=sys.stderr)
    sys.exit(1)

# Constants (Matched to C)
WINDOW_WIDTH = 800
WINDOW_HEIGHT = 1000
MAX_HISTORY = 50
LINE_HEIGHT = 18
BACKGROUND_COLOR = (0.0, 0.0, 0.0, 1.0)
HEADER_COLOR = (0.4, 0.4, 0.4)

class RenderGLWidget(QOpenGLWidget):
    def __init__(self):
        super().__init__()
        self.history = [] # List of {timestamp, lines}
        self.total_lines = 0
        self.scroll_offset = 0
        self.last_pulse_size = 0
        
        self.is_dragging = False
        self.drag_start_y = 0
        self.scroll_start_offset = 0
        
        self.setMinimumSize(WINDOW_WIDTH, WINDOW_HEIGHT)
        self.setFocusPolicy(Qt.StrongFocus)

    def add_frame(self, content):
        if not content: return
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        lines = content.splitlines()
        
        self.history.append({'timestamp': timestamp, 'lines': lines})
        self.total_lines += len(lines) + 1
        
        if len(self.history) > MAX_HISTORY:
            removed = self.history.pop(0)
            self.total_lines -= len(removed['lines']) + 1
            
        self.scroll_offset = 0 # Auto-scroll to bottom
        self.update()

    def initializeGL(self):
        glClearColor(*BACKGROUND_COLOR)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

    def resizeGL(self, w, h):
        glViewport(0, 0, w, h)
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        glOrtho(0, w, 0, h, -1, 1)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()

    def paintGL(self):
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glLoadIdentity()
        
        max_visible = (self.height() // LINE_HEIGHT) - 1
        total_drawn = 0
        
        # Exact C Logic: newest frame (i=0) at bottom, history above it
        for i, frame in enumerate(reversed(self.history)):
            # Lines of the frame bottom-to-top
            for j in range(len(frame['lines']) - 1, -2, -1):
                total_drawn += 1
                if self.scroll_offset < total_drawn <= self.scroll_offset + max_visible:
                    y = 20 + (total_drawn - self.scroll_offset - 1) * LINE_HEIGHT
                    
                    if j == -1: # Header
                        glColor3f(*HEADER_COLOR)
                        self.render_text(f"--- FRAME UPDATE at {frame['timestamp']} ---", 15, y)
                    else:
                        if i == 0: glColor3f(1.0, 1.0, 1.0) # Newest
                        else: glColor3f(0.8, 0.8, 0.8) # History
                        self.render_text(frame['lines'][j], 15, y)
                
                if total_drawn > self.scroll_offset + max_visible: break
            if total_drawn > self.scroll_offset + max_visible: break

        # Scrollbar Thumb
        sb_w = 16
        sb_x = self.width() - sb_w - 5
        track_h = self.height() - 20
        
        # Track
        glColor3f(0.1, 0.1, 0.1)
        glBegin(GL_QUADS)
        glVertex2f(sb_x, 10); glVertex2f(sb_x+sb_w, 10)
        glVertex2f(sb_x+sb_w, 10+track_h); glVertex2f(sb_x, 10+track_h)
        glEnd()

        if self.total_lines > 0:
            thumb_h = max(30, min(track_h, (max_visible / (self.total_lines + 1)) * track_h))
            
            scroll_range = max(1, self.total_lines - max_visible)
            scroll_perc = self.scroll_offset / (scroll_range + 5)
            thumb_y = 10 + scroll_perc * (track_h - thumb_h)
            
            # Thumb (Match C color)
            glColor3f(0.5, 0.5, 0.5)
            glBegin(GL_QUADS)
            glVertex2f(sb_x+1, thumb_y); glVertex2f(sb_x+sb_w-1, thumb_y)
            glVertex2f(sb_x+sb_w-1, thumb_y+thumb_h); glVertex2f(sb_x+1, thumb_y+thumb_h)
            glEnd()

    def render_text(self, text, x, y):
        glRasterPos2f(x, y)
        for char in text:
            try: glutBitmapCharacter(GLUT_BITMAP_9_BY_15, ord(char))
            except: pass

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            sb_w = 16
            sb_x = self.width() - sb_w - 5
            if event.position().x() >= sb_x:
                self.is_dragging = True
                self.drag_start_y = event.position().y()
                self.scroll_start_offset = self.scroll_offset
    
    def mouseReleaseEvent(self, event):
        if event.button() == Qt.LeftButton:
            self.is_dragging = False

    def mouseMoveEvent(self, event):
        if self.is_dragging:
            # dy > 0 means mouse moved DOWN. 
            # Window coords: down is positive. GL coords: down is negative.
            dy = event.position().y() - self.drag_start_y
            pixels_to_lines = self.total_lines / (self.height() - 20)
            # Inversion: Dragging thumb DOWN (positive dy) should decrease scroll_offset (newer content)
            self.scroll_offset = self.scroll_start_offset - int(dy * pixels_to_lines)
            
            max_visible = (self.height() // LINE_HEIGHT) - 1
            max_scroll = max(0, self.total_lines - 5)
            self.scroll_offset = max(0, min(max_scroll, self.scroll_offset))
            self.update()

    def keyPressEvent(self, event):
        key = event.key()
        max_scroll = max(0, self.total_lines - 5)
        
        if key == Qt.Key_Up: self.scroll_offset = min(max_scroll, self.scroll_offset + 2)
        elif key == Qt.Key_Down: self.scroll_offset = max(0, self.scroll_offset - 2)
        elif key == Qt.Key_PageUp: self.scroll_offset = min(max_scroll, self.scroll_offset + 20)
        elif key == Qt.Key_PageDown: self.scroll_offset = max(0, self.scroll_offset - 20)
        else:
            # Standard TPM Passthrough
            key_code = 0
            if key == Qt.Key_Left: key_code = 1000
            elif key == Qt.Key_Right: key_code = 1001
            elif key == Qt.Key_W: key_code = ord('w')
            elif key == Qt.Key_S: key_code = ord('s')
            elif key == Qt.Key_A: key_code = ord('a')
            elif key == Qt.Key_D: key_code = ord('d')
            elif key in [Qt.Key_Return, Qt.Key_Enter]: key_code = 13
            elif Qt.Key_0 <= key <= Qt.Key_9: key_code = ord('0') + (key - Qt.Key_0)
            
            if key_code > 0:
                with open("pieces/keyboard/history.txt", "a") as f:
                    f.write(f"KEY_PRESSED: {key_code}\n")
        self.update()

class GLRendererWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.last_pulse_size = 0
        self.gl_widget = RenderGLWidget()
        self.setCentralWidget(self.gl_widget)
        self.setWindowTitle("TPM GL Terminal (Python)")
        self.setGeometry(100, 100, WINDOW_WIDTH, WINDOW_HEIGHT)
        
        self.timer = QTimer()
        self.timer.timeout.connect(self.check_for_updates)
        self.timer.start(16)
        
        self.load_frame()

    def check_for_updates(self):
        pulse_path = "pieces/master_ledger/frame_changed.txt"
        if os.path.exists(pulse_path):
            curr_size = os.path.getsize(pulse_path)
            if curr_size != self.last_pulse_size:
                self.load_frame()
                self.last_pulse_size = curr_size

    def load_frame(self):
        frame_path = "pieces/display/current_frame.txt"
        if os.path.exists(frame_path):
            with open(frame_path, 'r', encoding='utf-8') as f:
                self.gl_widget.add_frame(f.read())

def main():
    glutInit(sys.argv)
    app = QApplication(sys.argv)
    window = GLRendererWindow()
    window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
