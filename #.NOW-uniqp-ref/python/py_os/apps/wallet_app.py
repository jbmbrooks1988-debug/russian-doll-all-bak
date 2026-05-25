"""
Wallet Application for PyOS
"""

from PySide6.QtWidgets import QVBoxLayout, QLabel, QLineEdit, QPushButton, QTextEdit
from PySide6.QtCore import Qt

from apps.base_app import BaseApplication


class WalletApp(BaseApplication):
    def setup_ui(self):
        """Setup the wallet application UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(15, 15, 15, 15)
        
        title_label = QLabel("Digital Wallet")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold; color: #ddd;")
        layout.addWidget(title_label)
        
        # Balance display
        balance_label = QLabel("Current Balance:")
        balance_label.setStyleSheet("color: #aaa; margin-top: 10px;")
        layout.addWidget(balance_label)
        
        self.balance_display = QTextEdit()
        self.balance_display.setMaximumHeight(40)
        self.balance_display.setText("$1,250.75")
        self.balance_display.setReadOnly(True)
        self.balance_display.setStyleSheet("""
            QTextEdit {
                background-color: #2a2a2a;
                color: #ddd;
                border: 1px solid #444;
                padding: 5px;
            }
        """)
        layout.addWidget(self.balance_display)
        
        # Transaction section
        transaction_label = QLabel("New Transaction:")
        transaction_label.setStyleSheet("color: #aaa; margin-top: 10px;")
        layout.addWidget(transaction_label)
        
        self.transaction_input = QLineEdit()
        self.transaction_input.setPlaceholderText("Enter amount...")
        self.transaction_input.setStyleSheet("""
            QLineEdit {
                background-color: #2a2a2a;
                color: #ddd;
                border: 1px solid #444;
                padding: 5px;
            }
        """)
        layout.addWidget(self.transaction_input)
        
        # Buttons
        button_layout = QVBoxLayout()
        
        send_button = QPushButton("Send Funds")
        send_button.setStyleSheet("""
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
        button_layout.addWidget(send_button)
        
        receive_button = QPushButton("Receive Funds")
        receive_button.setStyleSheet("""
            QPushButton {
                background-color: #50c878;
                color: white;
                border: none;
                padding: 8px;
                margin-top: 5px;
            }
            QPushButton:hover {
                background-color: #5fd888;
            }
            QPushButton:pressed {
                background-color: #40b868;
            }
        """)
        button_layout.addWidget(receive_button)
        
        layout.addLayout(button_layout)
        
        # Add stretch to push content up
        layout.addStretch()