"""
Profiles Application for PyOS
"""

from PySide6.QtWidgets import QVBoxLayout, QLabel, QListWidget, QPushButton, QLineEdit
from PySide6.QtCore import Qt

from apps.base_app import BaseApplication


class ProfilesApp(BaseApplication):
    def setup_ui(self):
        """Setup the profiles application UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(15, 15, 15, 15)
        
        title_label = QLabel("User Profiles")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold; color: #ddd;")
        layout.addWidget(title_label)
        
        # Profile list
        profile_list_label = QLabel("Saved Profiles:")
        profile_list_label.setStyleSheet("color: #aaa; margin-top: 10px;")
        layout.addWidget(profile_list_label)
        
        self.profile_list = QListWidget()
        self.profile_list.addItems(["Default User", "Admin", "Guest"])
        self.profile_list.setStyleSheet("""
            QListWidget {
                background-color: #2a2a2a;
                color: #ddd;
                border: 1px solid #444;
                padding: 5px;
            }
            QListWidget::item {
                padding: 5px;
            }
            QListWidget::item:selected {
                background-color: #4a4a4a;
            }
        """)
        layout.addWidget(self.profile_list)
        
        # Add profile section
        add_label = QLabel("Add New Profile:")
        add_label.setStyleSheet("color: #aaa; margin-top: 10px;")
        layout.addWidget(add_label)
        
        self.new_profile_input = QLineEdit()
        self.new_profile_input.setPlaceholderText("Enter profile name...")
        self.new_profile_input.setStyleSheet("""
            QLineEdit {
                background-color: #2a2a2a;
                color: #ddd;
                border: 1px solid #444;
                padding: 5px;
            }
        """)
        layout.addWidget(self.new_profile_input)
        
        add_button = QPushButton("Add Profile")
        add_button.setStyleSheet("""
            QPushButton {
                background-color: #4a90d9;
                color: white;
                border: none;
                padding: 8px;
                margin-top: 10px;
            }
            QPushButton:hover {
                background-color: #5a9fe9;
            }
            QPushButton:pressed {
                background-color: #3a80c9;
            }
        """)
        layout.addWidget(add_button)
        
        # Connect button to a simple function
        add_button.clicked.connect(self.add_profile)
        
        # Add stretch to push content up
        layout.addStretch()
        
    def add_profile(self):
        """Add a new profile from the input field"""
        profile_name = self.new_profile_input.text().strip()
        if profile_name:
            self.profile_list.addItem(profile_name)
            self.new_profile_input.clear()