"""
Base Application Class for PyOS
Provides common functionality for all applications
"""

from PySide6.QtWidgets import QWidget, QVBoxLayout, QLabel
from PySide6.QtCore import QObject


class BaseApplication(QWidget):
    def __init__(self):
        super().__init__()
        self.setup_ui()
        
    def setup_ui(self):
        """Setup the basic UI for the application"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        
        # Default content
        title_label = QLabel(f"{self.__class__.__name__}")
        title_label.setStyleSheet("font-size: 16px; font-weight: bold; color: #ddd;")
        layout.addWidget(title_label)
        
        desc_label = QLabel("This is a PyOS application")
        desc_label.setStyleSheet("color: #aaa; margin-top: 10px;")
        layout.addWidget(desc_label)
        
        # Add a stretch to center content vertically
        layout.addStretch()