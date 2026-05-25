"""
Terminal Application for PyOS
Standalone terminal that can be opened as a separate window
"""

from PySide6.QtWidgets import QVBoxLayout, QTextEdit, QWidget, QLabel
from PySide6.QtCore import Qt
from PySide6.QtGui import QFont, QColor, QTextCursor
from PySide6.QtCore import QEvent

from apps.base_app import BaseApplication
from apps.app_manager import AppManager


class TerminalApp(BaseApplication):
    def __init__(self, app_manager=None):
        # We need to bypass the BaseApplication's init to set up our own UI
        QWidget.__init__(self)
        # Use provided app manager or create a default one
        self.app_manager = app_manager or AppManager()
        self.history = []
        self.current_command = ""
        self.prompt_pos = 0
        self.setup_ui()
        
        # Display welcome message
        self.display_message("Welcome to PyOS Terminal!", newline=True)
        self.display_message("Type 'help' for available commands.", newline=True)
        self.display_prompt()

    def setup_ui(self):
        """Setup the terminal UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        
        # Title
        title_label = QLabel("PyOS Terminal")
        title_label.setStyleSheet("font-size: 16px; font-weight: bold; color: #ddd; margin-bottom: 10px;")
        layout.addWidget(title_label)

        # History display - now also handles input (like a real terminal)
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
        self.history_display.setReadOnly(False)  # Allow input
        self.history_display.setAcceptRichText(False)  # Plain text only
        self.history_display.installEventFilter(self)  # Intercept key events
        layout.addWidget(self.history_display)

        # Set focus to the text area
        self.history_display.setFocus()

    def display_message(self, message, newline=False):
        """Display a message in the history"""
        if newline:
            self.history_display.moveCursor(QTextCursor.End)
            self.history_display.setTextColor(QColor("#ffffff"))
            self.history_display.insertPlainText(f"\n{message}")
        else:
            # Just insert the message after current position
            self.history_display.moveCursor(QTextCursor.End)
            self.history_display.setTextColor(QColor("#ffffff"))
            self.history_display.insertPlainText(message)
        
        # Scroll to bottom
        sb = self.history_display.verticalScrollBar()
        sb.setValue(sb.maximum())

    def display_error(self, message):
        """Display an error message"""
        self.history_display.moveCursor(QTextCursor.End)
        self.history_display.setTextColor(QColor("#ff6666"))
        self.history_display.insertPlainText(f"\nError: {message}")
        # Scroll to bottom
        sb = self.history_display.verticalScrollBar()
        sb.setValue(sb.maximum())

    def display_success(self, message):
        """Display a success message"""
        self.history_display.moveCursor(QTextCursor.End)
        self.history_display.setTextColor(QColor("#66ff66"))
        self.history_display.insertPlainText(f"\n{message}")
        # Scroll to bottom
        sb = self.history_display.verticalScrollBar()
        sb.setValue(sb.maximum())

    def display_prompt(self):
        """Display the command prompt"""
        self.history_display.moveCursor(QTextCursor.End)
        self.history_display.setTextColor(QColor("#00ff00"))
        self.history_display.insertPlainText("\n$ ")
        # Store the position right after the prompt
        self.prompt_pos = self.history_display.textCursor().position()
        
        # Scroll to bottom
        sb = self.history_display.verticalScrollBar()
        sb.setValue(sb.maximum())

    def execute_command(self):
        """Execute the current command"""
        # Get the current text
        full_text = self.history_display.toPlainText()
        # Extract the command from the prompt position to the end
        command_text = full_text[self.prompt_pos:].strip()
        
        if command_text:
            self.history.append(command_text)
            
            parts = command_text.split()
            if not parts:
                self.display_prompt()
                return

            cmd = parts[0].lower()

            # Basic commands
            if cmd == 'help':
                self.show_help()
                self.display_prompt()
            elif cmd == 'clear':
                self.clear_history()
            elif cmd in ['list', 'ls', 'apps']:  # Each command separately recognized
                self.list_apps()
                self.display_prompt()
            elif cmd == 'echo':
                self.echo(' '.join(parts[1:]))
                self.display_prompt()
            elif cmd in ['quit', 'exit']:
                self.display_message("\nClosing terminal...", newline=False)
            # Special handling for hangman to ensure it opens in the desktop
            elif cmd == 'hangman':
                self.run_hangman()
                self.display_prompt()
            # Check if it's an application command
            elif self.app_manager.is_app_registered(cmd):
                self.run_app(cmd, parts[1:])
                self.display_prompt()
            else:
                self.display_error(f"Command '{cmd}' not recognized. Type 'help' for available commands.")
                self.display_prompt()
        else:
            # If no command was entered, just show a new prompt
            self.display_prompt()

    def show_help(self):
        """Show help information"""
        help_text = "\nAvailable commands:\n  help     - Show this help message\n  clear    - Clear the command history\n  list/ls/apps - Show available applications\n  echo     - Echo back the provided text\n  quit/exit - Close this terminal\n\nApplication commands:\n  Type the name of any registered application to launch it."
        self.display_message(help_text)

    def clear_history(self):
        """Clear the command history"""
        self.history_display.clear()
        self.display_message("Terminal cleared.", newline=True)
        # Reset prompt position
        self.prompt_pos = 0
        self.display_prompt()

    def list_apps(self):
        """List available applications"""
        apps = self.app_manager.get_registered_apps()
        if apps:
            self.display_message("\nAvailable applications:")
            for app_name in apps:
                self.display_message(f"  - {app_name}")
        else:
            self.display_message("\nNo applications registered.")

    def echo(self, text):
        """Echo back the provided text"""
        self.display_message(f"\n{text}")

    def run_app(self, app_name, args):
        """Run an application"""
        try:
            self.app_manager.launch_app(app_name, args)
            self.display_success(f"Launched application: {app_name}")
        except Exception as e:
            self.display_error(f"Failed to launch {app_name}: {str(e)}")

    def run_hangman(self):
        """Run the hangman game in the desktop"""
        # Try to get the parent desktop to create the game window there
        try:
            parent = self.parent()
            # Navigate up the hierarchy if needed to find the desktop
            while parent and not hasattr(parent, 'add_window'):
                parent = parent.parent()
                
            if parent and hasattr(parent, 'add_window'):
                # Create hangman game the same way as desktop context menu
                from apps.hangman_app import HangmanApp
                from window_manager.mini_window import MiniWindow
                
                hangman_game = HangmanApp()
                hangman_window = MiniWindow(title="Hangman Game", content_widget=hangman_game)
                
                # Position the window within the desktop
                hangman_window.setParent(parent)
                hangman_window.show()
                
                # Add to windows list
                parent.add_window(hangman_window)
                self.display_success("Launched Hangman Game in desktop")
            else:
                # Fallback to app manager if no desktop found
                self.app_manager.launch_app('hangman', [])
                self.display_success("Launched application: hangman")
        except Exception as e:
            self.display_error(f"Failed to launch hangman: {str(e)}")

    def eventFilter(self, obj, event):
        """Handle key press events to intercept Enter key"""
        if obj == self.history_display and event.type() == QEvent.KeyPress:
            if event.key() in (Qt.Key_Return, Qt.Key_Enter):
                # Check if we're at the prompt position or after it
                cursor = self.history_display.textCursor()
                if cursor.position() >= self.prompt_pos:
                    self.execute_command()
                    return True  # Event handled
                else:
                    # If before prompt, just move to after prompt
                    cursor.setPosition(self.prompt_pos)
                    self.history_display.setTextCursor(cursor)
                    return True
            elif event.key() == Qt.Key_Backspace:
                # Prevent user from deleting the prompt
                cursor = self.history_display.textCursor()
                if cursor.position() <= self.prompt_pos:
                    return True  # Don't process backspace at prompt position
            elif event.key() == Qt.Key_Up:
                # TODO: Implement command history navigation
                pass
            elif event.key() == Qt.Key_Down:
                # TODO: Implement command history navigation
                pass
            elif event.key() in (Qt.Key_Left, Qt.Key_Right):
                # Allow cursor to move but prevent going before prompt
                cursor = self.history_display.textCursor()
                if cursor.position() <= self.prompt_pos and event.key() == Qt.Key_Left:
                    # If trying to go left of prompt, just place cursor right after prompt
                    new_cursor = QTextCursor(self.history_display.document())
                    new_cursor.setPosition(self.prompt_pos)
                    self.history_display.setTextCursor(new_cursor)
                    return True
        return False