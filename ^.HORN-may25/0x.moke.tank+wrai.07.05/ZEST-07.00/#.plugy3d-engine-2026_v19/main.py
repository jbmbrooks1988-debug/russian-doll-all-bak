#!/usr/bin/env python3
"""
Main entry point that acts as a power strip for plugins.
Loads and manages plugins using a plugin.config file to determine which plugins to activate.
"""

import sys
import os
import json
import importlib.util
from PySide6.QtWidgets import QApplication, QMainWindow, QWidget
from PySide6.QtCore import QEvent, QTimer

class PluginManager:
    def __init__(self, config_file="plugin.config"):
        self.config_file = config_file
        self.plugins = []
        self.plugin_instances = []
        self.main_window = None
        self.voxel_grid_widget = None
        self.load_config()
        
    def load_config(self):
        """Load plugin configuration from file"""
        if not os.path.exists(self.config_file):
            print(f"Config file {self.config_file} not found. No plugins will be loaded.")
            self.enabled_plugins = []
            self.plugin_paths = {}
            return
        
        with open(self.config_file, 'r') as f:
            config = json.load(f)
            self.enabled_plugins = config.get('enabled_plugins', [])
            self.plugin_paths = config.get('plugin_paths', {})
    
    def load_plugin_module(self, plugin_name):
        """Loads a plugin's module from its file path."""
        if plugin_name not in self.plugin_paths:
            print(f"Plugin '{plugin_name}' not found in plugin_paths.")
            return None
        
        plugin_path = self.plugin_paths[plugin_name]
        try:
            spec = importlib.util.spec_from_file_location(plugin_name, plugin_path)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            return module
        except FileNotFoundError:
            print(f"Plugin file not found at {plugin_path}")
            return None
        except Exception as e:
            print(f"Failed to load plugin module {plugin_name}: {e}")
            return None

    def initialize_plugins(self, main_window):
        """Initialize all enabled plugins."""
        self.main_window = main_window
        self.plugin_instances = []  # Initialize the list of plugin instances
        self.project_manager_instance = None  # Initialize project manager reference

        # First, load the core voxel grid as it's the central widget
        if 'core.voxel_grid' in self.enabled_plugins:
            module = self.load_plugin_module('core.voxel_grid')
            if module and hasattr(module, 'VoxelGridPlugin'):
                # The VoxelGridPlugin is the widget itself
                self.voxel_grid_widget = module.VoxelGridPlugin(main_window)
                main_window.setCentralWidget(self.voxel_grid_widget)
                self.plugin_instances.append(self.voxel_grid_widget)
                self.voxel_grid_widget.initialize() # Initialize the widget
            else:
                print("Could not load core.voxel_grid. Aborting.")
                return
        else:
            print("core.voxel_grid not in enabled_plugins. Aborting.")
            return

        # Load other plugins
        for plugin_name in self.enabled_plugins:
            if plugin_name == 'core.voxel_grid':
                continue

            module = self.load_plugin_module(plugin_name)
            if module and hasattr(module, 'Plugin'):
                plugin_class = getattr(module, 'Plugin')
                instance = plugin_class(main_window)
                # Pass voxel_grid_widget to initialize method
                if hasattr(instance, 'initialize'):
                    instance.initialize(self.voxel_grid_widget)
                self.plugin_instances.append(instance)

                # If this is the selector plugin, attach it to the grid widget
                if plugin_name == 'core.selector':
                    self.voxel_grid_widget.selector_plugin = instance
                
                # If this is the minimap plugin, attach it to the grid widget
                if plugin_name == 'ui.minimap':
                    self.voxel_grid_widget.minimap_plugin = instance
                
                # If this is the project manager plugin, load the default project
                if plugin_name == 'core.project_manager':
                    # Store reference to project manager for later use
                    self.project_manager_instance = instance
                    # Schedule to load default project after other plugins are initialized
                    QTimer.singleShot(100, lambda: self._load_default_project(self.project_manager_instance))
        
        print(f"Initialized {len(self.plugin_instances)} plugins.")

    def handle_event(self, event):
        """Pass events to all plugins."""
        for instance in self.plugin_instances:
            if hasattr(instance, 'handle_event'):
                if instance.handle_event(event):
                    return True
        return False
    
    def _load_default_project(self, project_manager_plugin):
        """Helper method to load the default project after initialization"""
        try:
            # Attempt to load the default project if it exists
            project_manager_plugin.load_project("default")
            print("Default project loaded successfully")
        except Exception as e:
            print(f"Could not load default project: {e}")
            # If default doesn't exist, create and load it
            try:
                project_manager_plugin.create_new_project("default")
                print("Created new default project")
            except Exception as e2:
                print(f"Could not create default project: {e2}")


class MainAppWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Fresh Engine 2026")
        self.setGeometry(100, 100, 1280, 720)
        
        self.plugin_manager = PluginManager()
        self.plugin_manager.initialize_plugins(self)
        
        # Install event filter on the central widget
        central_widget = self.centralWidget()
        if central_widget:
            central_widget.installEventFilter(self)

        # Set up a timer for periodic updates to plugins
        self.update_timer = QTimer(self)
        self.update_timer.timeout.connect(self.update_plugins)
        self.update_timer.start(16) # ~60 FPS update rate

    def update_plugins(self):
        """Call the update method on all plugins that have one."""
        for instance in self.plugin_manager.plugin_instances:
            if hasattr(instance, 'update'):
                instance.update()

    def eventFilter(self, obj, event):
        """Filter events and pass them to the plugin manager."""
        if self.plugin_manager.handle_event(event):
            return True
        return super().eventFilter(obj, event)

def main():
    app = QApplication(sys.argv)
    window = MainAppWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()