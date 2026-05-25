"""
Store Application for PyOS
"""

from PySide6.QtWidgets import QVBoxLayout, QLabel, QListWidget, QPushButton, QHBoxLayout
from PySide6.QtCore import Qt

from apps.base_app import BaseApplication


class StoreApp(BaseApplication):
    def setup_ui(self):
        """Setup the store application UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(15, 15, 15, 15)
        
        title_label = QLabel("App Store")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold; color: #ddd;")
        layout.addWidget(title_label)
        
        # App categories
        category_label = QLabel("Categories:")
        category_label.setStyleSheet("color: #aaa; margin-top: 10px;")
        layout.addWidget(category_label)
        
        categories = ["Utilities", "Games", "Media", "Development"]
        self.category_list = QListWidget()
        self.category_list.addItems(categories)
        self.category_list.setStyleSheet("""
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
        layout.addWidget(self.category_list)
        
        # Popular apps
        popular_label = QLabel("Popular Apps:")
        popular_label.setStyleSheet("color: #aaa; margin-top: 10px;")
        layout.addWidget(popular_label)
        
        apps = [
            "Calculator Pro",
            "Photo Editor",
            "Video Player",
            "Music Player",
            "File Manager",
            "Calendar"
        ]
        
        self.apps_list = QListWidget()
        self.apps_list.addItems(apps)
        self.apps_list.setStyleSheet("""
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
        layout.addWidget(self.apps_list)
        
        # Action buttons
        button_layout = QHBoxLayout()
        
        install_button = QPushButton("Install")
        install_button.setStyleSheet("""
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
        button_layout.addWidget(install_button)
        
        update_button = QPushButton("Update")
        update_button.setStyleSheet("""
            QPushButton {
                background-color: #f0a04a;
                color: white;
                border: none;
                padding: 8px;
                margin-top: 10px;
            }
            QPushButton:hover {
                background-color: #f2b06a;
            }
            QPushButton:pressed {
                background-color: #e0903a;
            }
        """)
        button_layout.addWidget(update_button)
        
        layout.addLayout(button_layout)
        
        # Add stretch to push content up
        layout.addStretch()