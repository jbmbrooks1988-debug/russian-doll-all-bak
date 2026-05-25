"""
Settings Application for PyOS
"""

from PySide6.QtWidgets import QVBoxLayout, QLabel, QCheckBox, QPushButton, QSlider
from PySide6.QtCore import Qt

from apps.base_app import BaseApplication


class SettingsApp(BaseApplication):
    def setup_ui(self):
        """Setup the settings application UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(15, 15, 15, 15)
        
        title_label = QLabel("System Settings")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold; color: #ddd;")
        layout.addWidget(title_label)
        
        # Theme setting
        theme_label = QLabel("Dark Theme Enabled")
        theme_label.setStyleSheet("color: #aaa; margin-top: 10px;")
        layout.addWidget(theme_label)
        
        self.theme_checkbox = QCheckBox("Enable Dark Mode")
        self.theme_checkbox.setChecked(True)
        layout.addWidget(self.theme_checkbox)
        
        # Volume setting
        volume_label = QLabel("System Volume")
        volume_label.setStyleSheet("color: #aaa; margin-top: 10px;")
        layout.addWidget(volume_label)
        
        self.volume_slider = QSlider(Qt.Horizontal)
        self.volume_slider.setValue(75)
        self.volume_slider.setMaximum(100)
        layout.addWidget(self.volume_slider)
        
        # Save button
        save_button = QPushButton("Save Settings")
        save_button.setStyleSheet("""
            QPushButton {
                background-color: #4a90d9;
                color: white;
                border: none;
                padding: 8px;
                margin-top: 20px;
            }
            QPushButton:hover {
                background-color: #5a9fe9;
            }
            QPushButton:pressed {
                background-color: #3a80c9;
            }
        """)
        layout.addWidget(save_button)
        
        # Add stretch to push content up
        layout.addStretch()