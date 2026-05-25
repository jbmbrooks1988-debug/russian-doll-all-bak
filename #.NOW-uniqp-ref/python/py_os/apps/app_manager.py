"""
Application Manager for PyOS
Handles registration and launching of applications
"""

from PySide6.QtCore import QObject
from window_manager.mini_window import MiniWindow


class AppManager(QObject):
    def __init__(self):
        super().__init__()
        self.apps = {}
        
    def register_app(self, name, app_class):
        """Register an application class"""
        self.apps[name] = app_class
        
    def unregister_app(self, name):
        """Unregister an application"""
        if name in self.apps:
            del self.apps[name]
            
    def is_app_registered(self, name):
        """Check if an app is registered"""
        return name in self.apps
        
    def get_registered_apps(self):
        """Get list of registered app names"""
        return list(self.apps.keys())
        
    def launch_app(self, name, args=None):
        """Launch an application in a new window"""
        if not self.is_app_registered(name):
            raise ValueError(f"Application '{name}' is not registered")
            
        app_class = self.apps[name]
        app_instance = app_class()
        
        # Create a mini window for the app
        window_title = f"{name.capitalize()} App"
        app_window = MiniWindow(title=window_title, content_widget=app_instance)
        app_window.show()
        
        return app_window