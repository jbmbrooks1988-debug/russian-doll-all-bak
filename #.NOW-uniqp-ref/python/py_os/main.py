#!/usr/bin/env python3
"""
Main entry point for PyOS - Virtual Operating System
"""

import sys
from PySide6.QtWidgets import QApplication, QMainWindow, QWidget, QVBoxLayout
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPainter, QBrush

from window_manager.main_window import MainWindow
from apps.app_manager import AppManager
from apps.settings_app import SettingsApp
from apps.profiles_app import ProfilesApp
from apps.wallet_app import WalletApp
from apps.store_app import StoreApp
from apps.msr_app import MsrApp
from apps.terminal_app import TerminalApp
from apps.cube_app import CubeApp
from apps.hangman_app import HangmanApp


def main():
    app = QApplication(sys.argv)
    
    # Create the main app manager and register all apps
    app_manager = AppManager()
    app_manager.register_app('settings', SettingsApp)
    app_manager.register_app('profiles', ProfilesApp)
    app_manager.register_app('wallet', WalletApp)
    app_manager.register_app('store', StoreApp)
    app_manager.register_app('msr', MsrApp)
    app_manager.register_app('terminal', TerminalApp)
    app_manager.register_app('cube', CubeApp)  # Now using the Qt-only cube app
    app_manager.register_app('hangman', HangmanApp)
    
    # Create the main window and assign the app manager
    main_window = MainWindow()
    main_window.app_manager = app_manager
    
    main_window.show()
    
    sys.exit(app.exec())


if __name__ == "__main__":
    main()