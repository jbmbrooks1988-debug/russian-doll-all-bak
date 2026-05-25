"""
Command Line Interface for PyOS
Provides terminal-like functionality for executing commands
"""

from PySide6.QtWidgets import QWidget, QVBoxLayout, QTextEdit, QLineEdit, QLabel
from PySide6.QtCore import Qt
from PySide6.QtGui import QFont, QColor, QTextCursor

from apps.app_manager import AppManager


class CommandLineInterface(QWidget):
    def __init__(self):
        super().__init__()
        
        self.app_manager = AppManager()
        self.history = []
        self.history_index = 0
        
        self.setup_ui()
        
        # Display welcome message
        self.display_message("Welcome to PyOS Command Line Interface!")
        self.display_message("Type 'help' for available commands.")
        self.display_prompt()
        
    def setup_ui(self):
        """Setup the command line UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        
        # Title
        title = QLabel("PyOS Command Line")
        title.setStyleSheet("font-size: 16px; font-weight: bold; color: #ddd; margin-bottom: 10px;")
        layout.addWidget(title)
        
        # History display
        self.history_display = QTextEdit()
        self.history_display.document().setMaximumBlockCount(1000)  # Limit history size
        self.history_display.setFont(QFont("Courier New", 10))
        self.history_display.setStyleSheet("""
            QTextEdit {
                background-color: #1e1e1e;
                color: #ffffff;
                border: 1px solid #444;
                padding: 10px;
            }
        """)
        self.history_display.setReadOnly(True)
        layout.addWidget(self.history_display)
        
        # Input area
        input_layout = QVBoxLayout()
        
        # Prompt label
        self.prompt_label = QLabel("$ ")
        self.prompt_label.setStyleSheet("color: #00ff00; font-family: 'Courier New'; font-size: 10pt;")
        self.prompt_label.setMinimumWidth(20)
        
        # Input field
        self.input_field = QLineEdit()
        self.input_field.setFont(QFont("Courier New", 10))
        self.input_field.setStyleSheet("""
            QLineEdit {
                background-color: #2e2e2e;
                color: #ffffff;
                border: 1px solid #444;
                padding: 5px;
            }
        """)
        self.input_field.returnPressed.connect(self.process_input)
        
        # Layout for prompt and input
        prompt_input_layout = QVBoxLayout()
        prompt_input_layout.addWidget(self.prompt_label)
        prompt_input_layout.addWidget(self.input_field)
        
        input_widget = QWidget()
        input_widget.setLayout(prompt_input_layout)
        layout.addWidget(input_widget)
        
        # Set focus to input field
        self.input_field.setFocus()
        
    def display_message(self, message):
        """Display a message in the history"""
        self.history_display.setTextColor(QColor("#ffffff"))
        self.history_display.append(message)
        
    def display_error(self, message):
        """Display an error message"""
        self.history_display.setTextColor(QColor("#ff6666"))
        self.history_display.append(f"Error: {message}")
        
    def display_success(self, message):
        """Display a success message"""
        self.history_display.setTextColor(QColor("#66ff66"))
        self.history_display.append(message)
        
    def display_prompt(self):
        """Display the command prompt"""
        # Move cursor to end and add a new prompt line
        self.history_display.moveCursor(QTextCursor.End)
        self.history_display.setTextColor(QColor("#00ff00"))
        self.history_display.insertPlainText("$ ")
        self.history_display.moveCursor(QTextCursor.End)
        
    def process_input(self):
        """Process the user input"""
        command_text = self.input_field.text().strip()
        
        if command_text:
            # Add to history
            self.history.append(command_text)
            self.history_index = len(self.history)
            
            # Display the command just entered
            self.history_display.moveCursor(QTextCursor.End)
            self.history_display.setTextColor(QColor("#8888ff"))
            self.history_display.insertPlainText(f"{command_text}\n")
            
            # Process the command
            self.execute_command(command_text)
        
        # Clear input and show new prompt
        self.input_field.clear()
        
    def execute_command(self, command):
        """Execute a command"""
        parts = command.split()
        if not parts:
            self.display_message("")
            self.display_prompt()
            return
        
        cmd = parts[0].lower()
        
        # Basic commands
        if cmd == 'help':
            self.show_help()
        elif cmd == 'clear':
            self.clear_history()
        elif cmd == 'list' or cmd == 'ls':
            self.list_apps()
        elif cmd == 'echo':
            self.echo(' '.join(parts[1:]))
        elif cmd == 'quit' or cmd == 'exit':
            self.display_message("Exiting PyOS CLI...")
        # Check if it's an application command
        elif self.app_manager.is_app_registered(cmd):
            self.run_app(cmd, parts[1:])
        else:
            self.display_error(f"Command '{cmd}' not recognized. Type 'help' for available commands.")
        
        self.display_prompt()
        
    def show_help(self):
        """Show help information"""
        help_text = """
Available commands:
  help     - Show this help message
  clear    - Clear the command history
  list/ls  - Show available applications
  echo     - Echo back the provided text
  quit/exit - Exit the command line
  
Application commands:
  Type the name of any registered application to launch it.
        """
        self.display_message(help_text.strip())
        
    def clear_history(self):
        """Clear the command history"""
        self.history_display.clear()
        
    def list_apps(self):
        """List available applications"""
        apps = self.app_manager.get_registered_apps()
        if apps:
            self.display_message("Available applications:")
            for app_name in apps:
                self.display_message(f"  - {app_name}")
        else:
            self.display_message("No applications registered.")
            
    def echo(self, text):
        """Echo back the provided text"""
        self.display_message(text)
        
    def run_app(self, app_name, args):
        """Run an application"""
        try:
            self.app_manager.launch_app(app_name, args)
            self.display_success(f"Launched application: {app_name}")
        except Exception as e:
            self.display_error(f"Failed to launch {app_name}: {str(e)}")


# Test function to run the CLI standalone
if __name__ == "__main__":
    import sys
    from PySide6.QtWidgets import QApplication
    from apps.settings_app import SettingsApp
    from apps.profiles_app import ProfilesApp
    
    app = QApplication(sys.argv)
    
    # Register some sample apps
    cli = CommandLineInterface()
    cli.app_manager.register_app("settings", SettingsApp)
    cli.app_manager.register_app("profiles", ProfilesApp)
    
    cli.show()
    sys.exit(app.exec())