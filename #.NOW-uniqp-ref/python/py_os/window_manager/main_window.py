"""
Main Window Manager for PyOS
Handles the primary window container and basic layout
"""

from PySide6.QtWidgets import QMainWindow, QWidget, QVBoxLayout, QMenuBar, QStatusBar, QLabel
from PySide6.QtCore import Qt
from PySide6.QtGui import QPalette, QAction

from window_manager.desktop import DesktopWidget


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PyOS - Virtual Operating System")
        self.setGeometry(100, 100, 1200, 800)
        
        # Set up the central widget
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        
        # Create the main layout
        self.main_layout = QVBoxLayout(self.central_widget)
        self.main_layout.setContentsMargins(0, 0, 0, 0)
        self.main_layout.setSpacing(0)
        
        # Create the desktop environment
        self.desktop = DesktopWidget()
        self.main_layout.addWidget(self.desktop)
        
        # Create menu bar
        self.create_menu_bar()
        
        # Create status bar
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("PyOS Ready")
        
        # Set dark theme
        self.setStyleSheet("""
            QMainWindow {
                background-color: #2e2e2e;
            }
            QMenuBar {
                background-color: #3a3a3a;
                color: white;
                border-bottom: 1px solid #555;
            }
            QMenuBar::item {
                background: transparent;
                padding: 8px;
            }
            QMenuBar::item:selected {
                background: #555;
            }
            QMenuBar::item:pressed {
                background: #666;
            }
            QStatusBar {
                background-color: #3a3a3a;
                color: white;
                border-top: 1px solid #555;
            }
        """)
        
    def create_menu_bar(self):
        """Create the menu bar with system options"""
        menubar = self.menuBar()
        
        # File menu
        file_menu = menubar.addMenu('File')
        
        exit_action = QAction('Exit', self)
        exit_action.triggered.connect(self.close)
        file_menu.addAction(exit_action)
        
        # System menu
        system_menu = menubar.addMenu('System')
        
        settings_action = QAction('Settings', self)
        settings_action.triggered.connect(self.open_settings)
        system_menu.addAction(settings_action)
        
        # Apps menu
        apps_menu = menubar.addMenu('Apps')
        
        # Add actions for each app we want to be accessible from the menu
        terminal_action = QAction('Terminal', self)
        terminal_action.triggered.connect(self.open_terminal)
        apps_menu.addAction(terminal_action)
        
        settings_action_menu = QAction('Settings', self)
        settings_action_menu.triggered.connect(self.open_settings)
        apps_menu.addAction(settings_action_menu)
        
        profiles_action = QAction('Profiles', self)
        profiles_action.triggered.connect(self.open_profiles)
        apps_menu.addAction(profiles_action)
        
        wallet_action = QAction('Wallet', self)
        wallet_action.triggered.connect(self.open_wallet)
        apps_menu.addAction(wallet_action)
        
        store_action = QAction('Store', self)
        store_action.triggered.connect(self.open_store)
        apps_menu.addAction(store_action)
        
        msr_action = QAction('MSR', self)
        msr_action.triggered.connect(self.open_msr)
        apps_menu.addAction(msr_action)
        
        cube_action = QAction('Cube 3D', self)
        cube_action.triggered.connect(self.open_cube)
        apps_menu.addAction(cube_action)
        
        hangman_action = QAction('Hangman', self)
        hangman_action.triggered.connect(self.open_hangman)
        apps_menu.addAction(hangman_action)
        
        # Help menu
        help_menu = menubar.addMenu('Help')
        
        about_action = QAction('About', self)
        about_action.triggered.connect(self.show_about)
        help_menu.addAction(about_action)
        
    def open_settings(self):
        """Open the settings application"""
        from apps.settings_app import SettingsApp
        from window_manager.mini_window import MiniWindow
        
        app_instance = SettingsApp()
        app_window = MiniWindow(title="Settings App", content_widget=app_instance)
        app_window.show()
        self.status_bar.showMessage("Opened Settings App")
        
    def open_terminal(self):
        """Open the terminal application"""
        from apps.terminal_app import TerminalApp
        from window_manager.mini_window import MiniWindow
        
        # Use the shared app manager if available
        app_manager = getattr(self, 'app_manager', None)
        app_instance = TerminalApp(app_manager=app_manager)
        app_window = MiniWindow(title="Terminal", content_widget=app_instance)
        app_window.show()
        self.status_bar.showMessage("Opened Terminal")
        
    def open_profiles(self):
        """Open the profiles application"""
        from apps.profiles_app import ProfilesApp
        from window_manager.mini_window import MiniWindow
        
        app_instance = ProfilesApp()
        app_window = MiniWindow(title="Profiles App", content_widget=app_instance)
        app_window.show()
        self.status_bar.showMessage("Opened Profiles App")
        
    def open_wallet(self):
        """Open the wallet application"""
        from apps.wallet_app import WalletApp
        from window_manager.mini_window import MiniWindow
        
        app_instance = WalletApp()
        app_window = MiniWindow(title="Wallet App", content_widget=app_instance)
        app_window.show()
        self.status_bar.showMessage("Opened Wallet App")
        
    def open_store(self):
        """Open the store application"""
        from apps.store_app import StoreApp
        from window_manager.mini_window import MiniWindow
        
        app_instance = StoreApp()
        app_window = MiniWindow(title="Store App", content_widget=app_instance)
        app_window.show()
        self.status_bar.showMessage("Opened Store App")
        
    def open_msr(self):
        """Open the MSR application"""
        from apps.msr_app import MsrApp
        from window_manager.mini_window import MiniWindow
        
        app_instance = MsrApp()
        app_window = MiniWindow(title="MSR App", content_widget=app_instance)
        app_window.show()
        self.status_bar.showMessage("Opened MSR App")
        
    def open_cube(self):
        """Open the Cube 3D application"""
        from apps.cube_app import CubeApp
        from window_manager.mini_window import MiniWindow
        
        app_instance = CubeApp()
        app_window = MiniWindow(title="Cube 3D App", content_widget=app_instance)
        app_window.show()
        self.status_bar.showMessage("Opened Cube 3D App")
        
    def open_hangman(self):
        """Open the Hangman game application"""
        from apps.hangman_app import HangmanApp
        from window_manager.mini_window import MiniWindow
        
        app_instance = HangmanApp()
        app_window = MiniWindow(title="Hangman Game", content_widget=app_instance)
        app_window.show()
        self.status_bar.showMessage("Opened Hangman Game")
        
    def show_about(self):
        """Show about dialog"""
        self.status_bar.showMessage("PyOS - Virtual Operating System v0.1")
        
    def add_window(self, window):
        """Add a window to the desktop"""
        self.desktop.add_window(window)