"""
Mini Window implementation for PyOS
Provides draggable, resizable windows for applications
"""

from PySide6.QtWidgets import QFrame, QLabel, QPushButton, QHBoxLayout, QVBoxLayout, QWidget
from PySide6.QtCore import Qt, QPoint
from PySide6.QtGui import QPainter, QBrush, QColor


class MiniWindow(QFrame):
    def __init__(self, title="Application", content_widget=None):
        super().__init__()
        
        # Window properties
        self.title = title
        self.is_dragging = False
        self.drag_position = QPoint()
        
        # Set window frame style
        self.setFrameShape(QFrame.StyledPanel)
        self.setFrameShadow(QFrame.Raised)
        self.setLineWidth(2)
        
        # Set initial size and position - make it resizable
        self.resize(400, 300)  # Use resize instead of setFixedSize to make it resizable
        self.setMinimumSize(200, 150)  # Set minimum size
        self.setWindowFlags(Qt.Widget)  # Make it a child widget rather than a separate window
        
        # Create title bar
        self.title_bar = QWidget()
        self.title_bar.setFixedHeight(30)
        self.title_bar.setStyleSheet("""
            QWidget {
                background-color: #4a4a4a;
                border-top-left-radius: 5px;
                border-top-right-radius: 5px;
            }
        """)
        
        title_layout = QHBoxLayout(self.title_bar)
        title_layout.setContentsMargins(5, 5, 5, 5)
        title_layout.setSpacing(5)
        
        # Title label
        self.title_label = QLabel(title)
        self.title_label.setStyleSheet("color: white; font-weight: bold;")
        title_layout.addWidget(self.title_label)
        
        # Control buttons
        self.minimize_button = QPushButton("_")
        self.minimize_button.setFixedSize(25, 25)
        self.minimize_button.clicked.connect(self.showMinimized)
        
        self.maximize_button = QPushButton("□")
        self.maximize_button.setFixedSize(25, 25)
        self.maximize_button.clicked.connect(self.toggle_maximize)
        
        self.close_button = QPushButton("X")
        self.close_button.setFixedSize(25, 25)
        self.close_button.clicked.connect(self.close)
        
        # Style the buttons
        button_style = """
            QPushButton {
                background-color: #6a6a6a;
                color: white;
                border: none;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #7a7a7a;
            }
            QPushButton:pressed {
                background-color: #5a5a5a;
            }
        """
        self.minimize_button.setStyleSheet(button_style)
        self.maximize_button.setStyleSheet(button_style)
        self.close_button.setStyleSheet(button_style)
        
        title_layout.addWidget(self.minimize_button)
        title_layout.addWidget(self.maximize_button)
        title_layout.addWidget(self.close_button)
        
        # Create content area
        self.content_area = QWidget()
        self.content_area.setStyleSheet("background-color: #3a3a3a;")
        
        if content_widget:
            content_layout = QVBoxLayout(self.content_area)
            content_layout.setContentsMargins(0, 0, 0, 0)  # Remove margins to maximize space
            content_layout.addWidget(content_widget)
        else:
            # Default content
            content_layout = QVBoxLayout(self.content_area)
            default_label = QLabel("Application Content Area")
            default_label.setAlignment(Qt.AlignCenter)
            default_label.setStyleSheet("color: #ccc;")
            content_layout.addWidget(default_label)
        
        # Main layout
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)
        main_layout.addWidget(self.title_bar)
        main_layout.addWidget(self.content_area)
        
        # Mouse event handling for dragging
        self.title_bar.mousePressEvent = self.start_drag
        self.title_bar.mouseMoveEvent = self.drag_move
        self.title_bar.mouseReleaseEvent = self.stop_drag
        
    def start_drag(self, event):
        """Start dragging the window"""
        if event.button() == Qt.LeftButton:
            self.is_dragging = True
            self.drag_position = event.globalPos() - self.pos()
            event.accept()
    
    def drag_move(self, event):
        """Move the window during dragging"""
        if self.is_dragging:
            # Calculate new position
            new_pos = event.globalPos() - self.drag_position
            
            # Get parent's geometry to constrain movement (if parent exists)
            parent = self.parent()
            if parent:
                parent_rect = parent.rect()
                
                # Keep window within parent boundaries
                x = max(0, min(new_pos.x(), parent_rect.width() - self.width()))
                y = max(0, min(new_pos.y(), parent_rect.height() - self.height()))
                
                self.move(x, y)
            else:
                self.move(new_pos)
            
            event.accept()
    
    def stop_drag(self, event):
        """Stop dragging the window"""
        self.is_dragging = False
    
    def toggle_maximize(self):
        """Toggle window maximization"""
        # Since we're in a desktop environment, maximization might need special handling
        # For now, we'll just resize to fill available space within parent
        parent = self.parent()
        if parent:
            if hasattr(self, '_was_maximized') and self._was_maximized:
                # Restore to previous size
                self.setFixedSize(400, 300)
                self._was_maximized = False
                self.maximize_button.setText("□")
            else:
                # Maximize to parent size
                parent_size = parent.size()
                self.setFixedSize(parent_size.width() - 20, parent_size.height() - 20)
                self._was_maximized = True
                self.maximize_button.setText("❐")
    
    def setTitle(self, title):
        """Set the window title"""
        self.title = title
        self.title_label.setText(title)