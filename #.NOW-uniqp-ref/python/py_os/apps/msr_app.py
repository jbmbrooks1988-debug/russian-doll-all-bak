"""
MSR (Measurement/Reporting) Application for PyOS
"""

from PySide6.QtWidgets import QVBoxLayout, QLabel, QTextEdit, QPushButton, QProgressBar
from PySide6.QtCore import Qt

from apps.base_app import BaseApplication


class MsrApp(BaseApplication):
    def setup_ui(self):
        """Setup the MSR application UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(15, 15, 15, 15)
        
        title_label = QLabel("System Measurements & Reports")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold; color: #ddd;")
        layout.addWidget(title_label)
        
        # Status indicator
        status_label = QLabel("System Status:")
        status_label.setStyleSheet("color: #aaa; margin-top: 10px;")
        layout.addWidget(status_label)
        
        self.status_display = QTextEdit()
        self.status_display.setMaximumHeight(60)
        self.status_display.setText("All systems operational\nMemory usage: 45%\nCPU usage: 23%")
        self.status_display.setReadOnly(True)
        self.status_display.setStyleSheet("""
            QTextEdit {
                background-color: #2a2a2a;
                color: #ddd;
                border: 1px solid #444;
                padding: 5px;
            }
        """)
        layout.addWidget(self.status_display)
        
        # Progress bars for resources
        cpu_label = QLabel("CPU Usage:")
        cpu_label.setStyleSheet("color: #aaa; margin-top: 10px;")
        layout.addWidget(cpu_label)
        
        self.cpu_progress = QProgressBar()
        self.cpu_progress.setValue(23)
        self.cpu_progress.setStyleSheet("""
            QProgressBar {
                border: 1px solid #444;
                background-color: #2a2a2a;
                height: 15px;
                text-align: center;
            }
            QProgressBar::chunk {
                background-color: #4a90d9;
            }
        """)
        layout.addWidget(self.cpu_progress)
        
        memory_label = QLabel("Memory Usage:")
        memory_label.setStyleSheet("color: #aaa; margin-top: 10px;")
        layout.addWidget(memory_label)
        
        self.memory_progress = QProgressBar()
        self.memory_progress.setValue(45)
        self.memory_progress.setStyleSheet("""
            QProgressBar {
                border: 1px solid #444;
                background-color: #2a2a2a;
                height: 15px;
                text-align: center;
            }
            QProgressBar::chunk {
                background-color: #50c878;
            }
        """)
        layout.addWidget(self.memory_progress)
        
        # Generate report button
        report_button = QPushButton("Generate Report")
        report_button.setStyleSheet("""
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
        layout.addWidget(report_button)
        
        # Connect button to a simple function
        report_button.clicked.connect(self.generate_report)
        
        # Report output
        report_label = QLabel("Report Output:")
        report_label.setStyleSheet("color: #aaa; margin-top: 10px;")
        layout.addWidget(report_label)
        
        self.report_output = QTextEdit()
        self.report_output.setMaximumHeight(100)
        self.report_output.setReadOnly(True)
        self.report_output.setStyleSheet("""
            QTextEdit {
                background-color: #2a2a2a;
                color: #ddd;
                border: 1px solid #444;
                padding: 5px;
            }
        """)
        layout.addWidget(self.report_output)
        
        # Add stretch to push content up
        layout.addStretch()
        
    def generate_report(self):
        """Generate a sample system report"""
        report_text = """System Report Generated
-----------------------
Timestamp: Jan 10, 2026 16:30:45
Platform: PyOS v0.1
CPU Usage: 23%
Memory Usage: 45%
Disk Usage: 31%
Network Status: Connected
Processes: 12 active
"""
        self.report_output.setText(report_text)