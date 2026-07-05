"""
Menu Plugin
Implements a configurable menu system for the hybrid time system using Qt/PySide6.
"""

import json
import os
from PySide6.QtWidgets import QWidget, QDialog, QVBoxLayout, QHBoxLayout, QPushButton, QTabWidget, QCheckBox, QLabel, QSlider, QSpinBox, QDoubleSpinBox, QButtonGroup
from PySide6.QtCore import Qt, QEvent, QTimer
from PySide6.QtGui import QFont, QColor


class TimeDisplayWidget(QWidget):
    """Widget to display current game time on screen."""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(Qt.WA_TransparentForMouseEvents, True)  # Allow mouse events to pass through
        self.setStyleSheet("background-color: rgba(30, 30, 40, 180); border: 1px solid rgb(200, 200, 100); border-radius: 5px;")  # Dark translucent background with golden border
        
        # Create layout and label
        layout = QVBoxLayout()
        layout.setContentsMargins(10, 10, 10, 10)  # Add some padding
        
        # Create label for time display
        self.time_label = QLabel("Time: 2026-01-01 00:00")
        self.time_label.setStyleSheet("color: lightgreen; font-size: 16px; font-weight: bold; qproperty-alignment: AlignLeft;")
        self.time_label.setAlignment(Qt.AlignLeft)
        
        layout.addWidget(self.time_label)
        self.setLayout(layout)
        
        # Set fixed size for the time display
        self.setFixedWidth(250)
        self.setFixedHeight(50)  # Height to accommodate the content
        
    def update_time(self, time_str):
        """Update the displayed time."""
        self.time_label.setText(f"Time: {time_str}")


class SettingsDialog(QDialog):
    """Qt dialog for settings configuration"""
    def __init__(self, parent=None, settings=None):
        super().__init__(parent)
        self.settings = settings or {}
        self.init_ui()
    
    def init_ui(self):
        self.setWindowTitle("Game Settings Menu")
        self.setModal(True)
        self.resize(400, 500)
        
        layout = QVBoxLayout()
        
        # Create tab widget
        tab_widget = QTabWidget()
        
        # Time Settings Tab
        time_tab = self.create_time_tab()
        tab_widget.addTab(time_tab, "Time Settings")
        
        # Reality Field Tab
        reality_tab = self.create_reality_field_tab()
        tab_widget.addTab(reality_tab, "Reality Field")
        
        # Visual Settings Tab
        visual_tab = self.create_visual_tab()
        tab_widget.addTab(visual_tab, "Visual")
        
        layout.addWidget(tab_widget)
        
        # Buttons
        button_layout = QHBoxLayout()
        self.apply_btn = QPushButton("Apply")
        self.apply_btn.clicked.connect(self.apply_settings)
        self.ok_btn = QPushButton("OK")
        self.ok_btn.clicked.connect(self.accept)
        self.cancel_btn = QPushButton("Cancel")
        self.cancel_btn.clicked.connect(self.reject)
        
        button_layout.addWidget(self.apply_btn)
        button_layout.addStretch()
        button_layout.addWidget(self.ok_btn)
        button_layout.addWidget(self.cancel_btn)
        
        layout.addLayout(button_layout)
        self.setLayout(layout)
        
        # Initialize controls with settings
        self.load_settings()
    
    def create_time_tab(self):
        tab = QWidget()
        layout = QVBoxLayout()
        
        # Time advancement mode
        self.time_manual_radio = QPushButton("Manual (on player move)")
        self.time_manual_radio.setCheckable(True)
        self.time_auto_radio = QPushButton("Automatic (timer-based)")
        self.time_auto_radio.setCheckable(True)
        
        # Group them so only one can be selected
        self.time_mode_group = QButtonGroup()
        self.time_mode_group.addButton(self.time_manual_radio, 0)
        self.time_mode_group.addButton(self.time_auto_radio, 1)
        
        layout.addWidget(QLabel("Time Advancement Mode:"))
        layout.addWidget(self.time_manual_radio)
        layout.addWidget(self.time_auto_radio)
        
        # Timer speed
        layout.addWidget(QLabel("Timer Speed Multiplier:"))
        self.speed_slider = QSlider(Qt.Horizontal)
        self.speed_slider.setMinimum(10)  # 0.1x
        self.speed_slider.setMaximum(500)  # 5.0x
        self.speed_slider.setValue(100)  # 1.0x
        
        self.speed_spinbox = QDoubleSpinBox()
        self.speed_spinbox.setRange(0.1, 5.0)
        self.speed_spinbox.setSingleStep(0.1)
        self.speed_spinbox.setValue(1.0)
        
        # Connect slider and spinbox
        self.speed_slider.valueChanged.connect(
            lambda value: self.speed_spinbox.setValue(value / 100.0)
        )
        self.speed_spinbox.valueChanged.connect(
            lambda value: self.speed_slider.setValue(int(value * 100))
        )
        
        slider_layout = QHBoxLayout()
        slider_layout.addWidget(self.speed_slider)
        slider_layout.addWidget(self.speed_spinbox)
        layout.addLayout(slider_layout)
        
        tab.setLayout(layout)
        return tab
    
    def create_reality_field_tab(self):
        tab = QWidget()
        layout = QVBoxLayout()
        
        layout.addWidget(QLabel("Reality Field Radius:"))
        self.radius_spinbox = QSpinBox()
        self.radius_spinbox.setRange(1, 50)
        self.radius_spinbox.setValue(10)
        layout.addWidget(self.radius_spinbox)
        
        tab.setLayout(layout)
        return tab
    
    def create_visual_tab(self):
        tab = QWidget()
        layout = QVBoxLayout()
        
        self.show_time_check = QCheckBox("Show Time Display")
        layout.addWidget(self.show_time_check)
        
        self.show_reality_check = QCheckBox("Show Reality Field Visual Indicator")
        layout.addWidget(self.show_reality_check)
        
        tab.setLayout(layout)
        return tab
    
    def load_settings(self):
        """Load settings into UI controls"""
        # Time advancement mode
        mode = self.settings.get("time_advancement_mode", "manual")
        if mode == "manual":
            self.time_manual_radio.setChecked(True)
        else:
            self.time_auto_radio.setChecked(True)
        
        # Timer speed
        speed = self.settings.get("timer_speed", 1.0)
        self.speed_spinbox.setValue(speed)
        
        # Reality field radius
        radius = self.settings.get("reality_field_radius", 10)
        self.radius_spinbox.setValue(radius)
        
        # Visual settings
        self.show_time_check.setChecked(self.settings.get("show_time_display", True))
        self.show_reality_check.setChecked(self.settings.get("show_reality_field", False))
    
    def apply_settings(self):
        """Apply settings without closing dialog"""
        self.settings["time_advancement_mode"] = "manual" if self.time_manual_radio.isChecked() else "automatic"
        self.settings["timer_speed"] = self.speed_spinbox.value()
        self.settings["reality_field_radius"] = self.radius_spinbox.value()
        self.settings["show_time_display"] = self.show_time_check.isChecked()
        self.settings["show_reality_field"] = self.show_reality_check.isChecked()


class Plugin:
    def __init__(self, main_app):
        self.main_app = main_app
        self.enabled = True
        self.settings_dialog = None
        self.settings_file = "data/settings.json"
        self.settings = self.load_settings()
        self.time_display_widget = None  # For displaying game time on screen
        
        # Initialize default settings if needed
        self.ensure_default_settings()
        
        # Timer for updating the time display
        self.time_update_timer = None

    def ensure_default_settings(self):
        """Ensure default settings exist."""
        defaults = {
            "time_advancement_mode": "manual",  # Either "manual" or "automatic"
            "timer_speed": 1.0,               # Speed multiplier for automatic mode
            "reality_field_radius": 10,       # Radius for player-based entity movement
            "show_time_display": True,        # Whether to show time on screen
            "show_reality_field": False       # Whether to visualize reality field
        }
        
        for key, default_value in defaults.items():
            if key not in self.settings:
                self.settings[key] = default_value
        
        self.save_settings()

    def load_settings(self):
        """Load settings from file."""
        if os.path.exists(self.settings_file):
            try:
                with open(self.settings_file, 'r') as f:
                    return json.load(f)
            except:
                return {}
        return {}

    def save_settings(self):
        """Save settings to file."""
        os.makedirs(os.path.dirname(self.settings_file), exist_ok=True)
        with open(self.settings_file, 'w') as f:
            json.dump(self.settings, f, indent=2)

    def initialize(self, voxel_grid_widget=None):
        """Called when the plugin is loaded."""
        self.voxel_grid_widget = voxel_grid_widget
        
        # Create the time display widget if enabled in settings
        if self.settings.get("show_time_display", True):
            self.create_time_display()
        
        # Set up timer to periodically update the time display
        self.setup_time_display_update()

    def create_time_display(self):
        """Create and display the time display widget."""
        if self.time_display_widget is None and hasattr(self.main_app, 'centralWidget'):
            # Create the time display widget as child of main app
            self.time_display_widget = TimeDisplayWidget(self.main_app)
            self.time_display_widget.show()
            
            # Position it in the top-right corner of the main window (away from HUD)
            # Calculate position (top-right area)
            self.time_display_widget.move(self.main_app.width() - self.time_display_widget.width() - 20, 10)
            
            # Patch the main app resize event to reposition on window resize
            if not hasattr(self, 'original_resizeEvent'):
                self.original_resizeEvent = self.main_app.resizeEvent
                self.main_app.resizeEvent = self.patched_resizeEvent

    def setup_time_display_update(self):
        """Set up timer to periodically update the time display."""
        # Create a timer that updates the time display periodically
        if self.time_update_timer is None:
            self.time_update_timer = QTimer()
            self.time_update_timer.timeout.connect(self.periodic_time_update)
            self.time_update_timer.start(1000)  # Update every second (1000 ms)

    def periodic_time_update(self):
        """Periodically update the time display with current game time."""
        if self.time_display_widget and hasattr(self.main_app, 'plugin_manager'):
            # Find the time plugin to get current time
            for plugin in self.main_app.plugin_manager.plugin_instances:
                if hasattr(plugin, 'get_formatted_time'):
                    try:
                        time_str = plugin.get_formatted_time()
                        # Only update if time has changed to avoid unnecessary updates
                        current_text = self.time_display_widget.time_label.text()
                        if f"Time: {time_str}" != current_text:
                            self.update_time_display(time_str)
                        break
                    except Exception as e:
                        # If time plugin isn't ready, skip this update
                        print(f"Error updating time display: {e}")
                        pass

    def update_time_display(self, time_str):
        """Update the time display with new time string."""
        if self.time_display_widget:
            self.time_display_widget.update_time(time_str)

    def patched_resizeEvent(self, event):
        """Called when the main window is resized."""
        # Call original resize event
        self.original_resizeEvent(event)
        # Reposition time display
        if self.time_display_widget:
            self.time_display_widget.move(self.main_app.width() - self.time_display_widget.width() - 20, 10)

    def handle_event(self, event):
        """Handle events - specifically check for 'M' key press."""
        # Check if the event is a key event for 'M' or 'm'
        from PySide6.QtCore import QEvent
        from PySide6.QtGui import QKeyEvent
        
        if hasattr(event, 'type') and event.type() == QEvent.KeyPress:
            # Handle both uppercase and lowercase 'M'
            if hasattr(event, 'key'):
                if event.key() == Qt.Key_M:
                    # Create and show the settings dialog
                    self.toggle_menu()
                    return True  # Indicate event was handled
        return False

    def toggle_menu(self):
        """Toggle the visibility of the menu."""
        if self.settings_dialog and self.settings_dialog.isVisible():
            self.settings_dialog.close()
            self.settings_dialog = None
        else:
            # Create new dialog, passing current settings
            self.settings_dialog = SettingsDialog(parent=self.main_app, settings=self.settings)
            
            # Connect the dialog's apply button
            if hasattr(self.settings_dialog, 'apply_btn'):
                self.settings_dialog.apply_btn.clicked.connect(self.apply_settings_from_dialog)
            
            # Connect the OK button to save changes
            if hasattr(self.settings_dialog, 'ok_btn'):
                self.settings_dialog.ok_btn.clicked.connect(self.dialog_accepted)
            
            self.settings_dialog.show()
    
    def apply_settings_from_dialog(self):
        """Apply settings from the dialog."""
        if self.settings_dialog:
            self.settings_dialog.apply_settings()
            # Update local settings
            self.settings = {**self.settings, **self.settings_dialog.settings}
            self.save_settings()
            
            # Handle show_time_display setting change
            show_time = self.settings.get("show_time_display", True)
            if show_time and self.time_display_widget is None:
                # Need to create the time display
                self.create_time_display()
            elif not show_time and self.time_display_widget:
                # Need to hide the time display
                self.time_display_widget.hide()
                self.time_display_widget = None
            
            # Notify other plugins of settings change
            self.notify_settings_change()
    
    def dialog_accepted(self):
        """Called when dialog is accepted (OK clicked)."""
        self.apply_settings_from_dialog()
        
        # Handle show_time_display setting change
        show_time = self.settings.get("show_time_display", True)
        if show_time and self.time_display_widget is None:
            # Need to create the time display
            self.create_time_display()
        elif not show_time and self.time_display_widget:
            # Need to hide the time display
            self.time_display_widget.hide()
            self.time_display_widget = None
        
        self.settings_dialog = None
    
    def notify_settings_change(self):
        """Notify other plugins of settings changes."""
        # Find the time plugin and notify it of changes
        if hasattr(self.main_app, 'plugin_manager'):
            if hasattr(self.main_app.plugin_manager, 'plugin_instances'):
                for plugin in self.main_app.plugin_manager.plugin_instances:
                    if hasattr(plugin, 'on_settings_changed'):
                        plugin.on_settings_changed(self.settings)

    def activate(self):
        """Called when the plugin is activated."""
        self.enabled = True

    def deactivate(self):
        """Called when the plugin is deactivated."""
        self.enabled = False
        if self.settings_dialog:
            self.settings_dialog.close()
            self.settings_dialog = None
        if self.time_display_widget:
            self.time_display_widget.hide()

    def get_widget(self):
        """Return the widget for UI integration if applicable."""
        return None