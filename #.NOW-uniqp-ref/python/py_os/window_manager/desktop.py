"""
Desktop environment for PyOS
Manages the workspace where applications appear
"""

from PySide6.QtWidgets import QWidget, QHBoxLayout, QLabel, QMenu
from PySide6.QtCore import Qt
from PySide6.QtGui import QPalette, QAction, QColor


class DesktopWidget(QWidget):
    def __init__(self):
        super().__init__()
        self.layout = QHBoxLayout(self)
        self.layout.setContentsMargins(0, 0, 0, 0)
        
        # Create a background label to fill the desktop
        self.background_label = QLabel()
        self.background_label.setAlignment(Qt.AlignCenter)
        self.background_label.setText("PyOS Desktop Environment\n(Right-click for options)")
        self.background_label.setStyleSheet("""
            QLabel {
                font-size: 24px;
                color: #aaa;
                background-color: #1e1e1e;
                qproperty-alignment: AlignCenter;
            }
        """)
        self.layout.addWidget(self.background_label)
        
        # Maintain a list of open windows
        self.windows = []
        
        # Enable context menu
        self.setContextMenuPolicy(Qt.CustomContextMenu)
        self.customContextMenuRequested.connect(self.show_context_menu)
        
        # Set the background color
        palette = self.palette()
        palette.setColor(QPalette.Window, QColor(30, 30, 30))
        self.setPalette(palette)
        self.setAutoFillBackground(True)
        
    def add_window(self, window):
        """Add a window to the desktop"""
        self.windows.append(window)
        # In a real implementation, we would position the window appropriately
        # For now, we'll just show it
        window.show()
        
    def show_context_menu(self, position):
        """Show the context menu at the given position"""
        menu = QMenu(self)
        
        # Add terminal option
        terminal_action = QAction("Open Terminal", self)
        terminal_action.triggered.connect(self.open_terminal)
        menu.addAction(terminal_action)
        
        # Add screensaver option
        screensaver_action = QAction("Show Screensaver", self)
        screensaver_action.triggered.connect(self.show_screensaver)
        menu.addAction(screensaver_action)
        
        # Add hangman game option
        hangman_action = QAction("Play Hangman", self)
        hangman_action.triggered.connect(self.show_hangman)
        menu.addAction(hangman_action)
        
        # Add other application options
        apps_action = QAction("Open Apps Menu", self)
        apps_action.triggered.connect(self.open_apps_menu)
        menu.addAction(apps_action)
        
        # Show the menu at the right-click position
        menu.exec_(self.mapToGlobal(position))
        
    def open_terminal(self):
        """Open a new terminal window"""
        from apps.terminal_app import TerminalApp
        from window_manager.mini_window import MiniWindow
        
        # Use the app manager from the main window if available
        main_window = self.window()  # Get the top-level window (MainWindow)
        app_manager = getattr(main_window, 'app_manager', None)
        
        terminal_app = TerminalApp(app_manager=app_manager)
        terminal_window = MiniWindow(title="Terminal", content_widget=terminal_app)
        
        # Position the window within the desktop
        terminal_window.setParent(self)
        terminal_window.show()
        
        # Add to windows list
        self.add_window(terminal_window)
        
    def show_screensaver(self):
        """Show the OpenGL cube screensaver"""
        from graphics.cube_screensaver import CubeScreensaver
        from window_manager.mini_window import MiniWindow
        
        screensaver = CubeScreensaver()
        screensaver_window = MiniWindow(title="Screensaver", content_widget=screensaver)
        
        # Position the window within the desktop
        screensaver_window.setParent(self)
        screensaver_window.show()
        
        # Add to windows list
        self.add_window(screensaver_window)
        
    def show_hangman(self):
        """Show the Hangman game"""
        from apps.hangman_app import HangmanApp
        from window_manager.mini_window import MiniWindow
        
        hangman_game = HangmanApp()
        hangman_window = MiniWindow(title="Hangman Game", content_widget=hangman_game)
        
        # Position the window within the desktop
        hangman_window.setParent(self)
        hangman_window.show()
        
        # Add to windows list
        self.add_window(hangman_window)
        
    def open_apps_menu(self):
        """Open the apps menu (placeholder for now)"""
        from apps.settings_app import SettingsApp
        from window_manager.mini_window import MiniWindow
        
        app_instance = SettingsApp()
        app_window = MiniWindow(title="Settings App", content_widget=app_instance)
        app_window.setParent(self)
        app_window.show()
        self.add_window(app_window)