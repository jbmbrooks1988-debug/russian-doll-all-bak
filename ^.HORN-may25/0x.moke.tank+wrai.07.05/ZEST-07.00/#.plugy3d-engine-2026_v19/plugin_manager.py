#!/usr/bin/env python3
"""
Plugin Manager for Fresh Engine 2026
Handles loading, initializing, and managing plugins
"""

import os
import sys
import importlib.util
from pathlib import Path


class PluginManager:
    def __init__(self, main_app):
        self.main_app = main_app
        self.plugins = {}
        self.plugin_paths = []
    
    def register_plugin_path(self, path):
        """Register a directory containing plugins"""
        self.plugin_paths.append(path)
    
    def discover_plugins(self, directory):
        """Discover all available plugins in a directory"""
        plugin_dir = Path(directory)
        plugins = []
        
        for plugin_file in plugin_dir.rglob("*.py"):
            if plugin_file.name != "__init__.py":
                plugins.append(plugin_file)
        
        return plugins
    
    def load_plugin(self, plugin_path):
        """Load a plugin from a file path"""
        spec = importlib.util.spec_from_file_location("plugin", plugin_path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        
        # Look for PluginClass in the module
        if hasattr(module, 'PluginClass'):
            plugin_instance = module.PluginClass(self.main_app)
            plugin_name = plugin_instance.__class__.__name__
            self.plugins[plugin_name] = plugin_instance
            return plugin_instance
        else:
            print(f"Plugin at {plugin_path} does not define PluginClass")
            return None
    
    def initialize_plugins(self):
        """Initialize all loaded plugins"""
        for name, plugin in self.plugins.items():
            try:
                plugin.initialize()
                print(f"Initialized plugin: {name}")
            except Exception as e:
                print(f"Failed to initialize plugin {name}: {e}")
    
    def get_plugin(self, name):
        """Get a plugin by name"""
        return self.plugins.get(name)
    
    def enable_plugin(self, name):
        """Enable a plugin"""
        plugin = self.get_plugin(name)
        if plugin:
            plugin.activate()
    
    def disable_plugin(self, name):
        """Disable a plugin"""
        plugin = self.get_plugin(name)
        if plugin:
            plugin.deactivate()


# Example plugin base class
class BasePlugin:
    def __init__(self, main_app):
        self.main_app = main_app
        self.enabled = True
    
    def initialize(self):
        """Called when the plugin is loaded"""
        print(f"{self.__class__.__name__} initialized")
    
    def activate(self):
        """Called when the plugin is activated"""
        self.enabled = True
        print(f"{self.__class__.__name__} activated")
    
    def deactivate(self):
        """Called when the plugin is deactivated"""
        self.enabled = False
        print(f"{self.__class__.__name__} deactivated")
    
    def get_widget(self):
        """Return the widget for UI integration if applicable"""
        return None