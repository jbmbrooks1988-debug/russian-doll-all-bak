from PySide6.QtCore import (QCoreApplication, QMetaObject, QObject, 
                            QPoint, QRect, QSize, QTime, QUrl, Qt,
                            QTimer, QThread, Signal)
from PySide6.QtGui import (QAction, QBrush, QColor, QConicalGradient,
                          QCursor, QFont, QFontDatabase, QGradient,
                          QIcon, QImage, QKeySequence, QLinearGradient,
                          QPainter, QPalette, QPixmap, QRadialGradient,
                          QTransform)
from PySide6.QtWidgets import (QApplication, QFrame, QGridLayout, QGroupBox,
                              QLabel, QLayout, QMainWindow, QMenu,
                              QMenuBar, QPushButton, QSizePolicy, QSlider,
                              QStatusBar, QVBoxLayout, QWidget, QFileDialog,
                              QListWidget, QProgressBar, QHBoxLayout)
import pygame
import os
import sys
import time
import math


class AudioPlayer(QObject):
    """
    Audio player class that handles music playback
    """
    position_changed = Signal(int)  # Signal emitted when playback position changes
    duration_changed = Signal(int)  # Signal emitted when track duration changes
    track_ended = Signal()          # Signal emitted when track ends
    audio_visualization_data = Signal(list)  # Signal for audio visualization data
    
    def __init__(self):
        super().__init__()
        pygame.mixer.init(frequency=22050, size=-16, channels=2, buffer=512)
        self.current_track = None
        self.is_playing = False
        self.paused = False
        self.start_time = 0  # Track start time for position calculation
        self.total_time = 0  # Total track duration
        
        # Timer to check for track ending and update position
        self.timer = QTimer()
        self.timer.timeout.connect(self.check_state)
        self.timer.start(50)  # Check every 50ms for smoother updates
        
        # Visualization timer - generate visualization data
        self.viz_timer = QTimer()
        self.viz_timer.timeout.connect(self.generate_viz_data)
        self.viz_timer.start(30)  # Update visualization every 30ms
        
        # Simulated audio data for visualization
        self.viz_phase = 0
        
    def load_track(self, filepath):
        """Load a track for playback"""
        try:
            pygame.mixer.music.load(filepath)
            self.current_track = filepath
            
            # Try to get track duration (this is approximate)
            # Try using mutagen to get accurate track duration
            try:
                from mutagen.mp3 import MP3
                audio = MP3(filepath)
                self.total_time = int(audio.info.length)
                self.duration_changed.emit(self.total_time * 1000)  # Convert to milliseconds
            except ImportError:
                # Fallback: try wave module
                try:
                    import wave
                    with wave.open(filepath, 'rb') as wf:
                        frames = wf.getnframes()
                        rate = wf.getframerate()
                        self.total_time = int(frames / float(rate))
                        self.duration_changed.emit(self.total_time * 1000)  # Convert to milliseconds
                except:
                    # Ultimate fallback value
                    self.duration_changed.emit(180000)  # 3 minutes default
            except:
                # Ultimate fallback value
                self.duration_changed.emit(180000)  # 3 minutes default
                
            return True
        except Exception as e:
            print(f"Error loading track: {e}")
            return False
            
    def play(self):
        """Play the current track"""
        if self.current_track:
            pygame.mixer.music.play()
            self.is_playing = True
            self.paused = False
            self.start_time = time.time()
            
    def pause(self):
        """Pause/resume playback"""
        if self.is_playing:
            if self.paused:
                pygame.mixer.music.unpause()
                self.start_time = time.time() - self.get_played_time()  # Adjust start time
                self.paused = False
            else:
                pygame.mixer.music.pause()
                self.paused = True
                
    def stop(self):
        """Stop playback"""
        pygame.mixer.music.stop()
        self.is_playing = False
        self.paused = False
        self.position_changed.emit(0)
        
    def set_volume(self, volume):
        """Set playback volume (0.0 to 1.0)"""
        pygame.mixer.music.set_volume(volume / 100.0)
        
    def get_played_time(self):
        """Get time played since start of track in seconds"""
        if self.current_track and self.is_playing and not self.paused:
            elapsed = time.time() - self.start_time
            return min(elapsed, self.total_time)  # Don't exceed total time
        elif self.current_track and not self.is_playing and not self.paused:
            return self.total_time
        else:
            return 0
            
    def get_position_percentage(self):
        """Get current playback position as percentage"""
        if self.total_time > 0:
            return int((self.get_played_time() / self.total_time) * 100)
        return 0
        
    def check_state(self):
        """Check playback state and emit signals"""
        if self.is_playing and not self.paused:
            # Update position
            pos_percent = self.get_position_percentage()
            self.position_changed.emit(pos_percent * 1000)  # Convert to milliseconds equivalent
            
        if self.is_playing and not pygame.mixer.music.get_busy():
            self.track_ended.emit()
    
    def generate_viz_data(self):
        """Generate visualization data that simulates audio response"""
        if self.is_playing:
            # Generate data that simulates FFT-like response
            # Create a dynamic pattern that changes based on playback
            data = []
            time_factor = time.time() * 2  # Speed factor for animation
            
            for i in range(100):
                # Base frequency components
                val = 0
                
                # Create multiple frequency bands
                val += math.sin(i * 0.1 + time_factor) * 30
                val += math.sin(i * 0.25 + time_factor * 1.3) * 20
                val += math.sin(i * 0.5 + time_factor * 0.7) * 10
                
                # Add some randomness to make it more interesting
                import random
                val += random.randint(-5, 5)
                
                # Position-dependent dampening to create a more natural look
                pos_factor = math.sin(i * math.pi / 99)  # Center peaks higher
                val *= pos_factor
                
                # Ensure value is in valid range
                val = max(0, min(100, val))
                data.append(val)
            
            # Emit the visualization data
            self.audio_visualization_data.emit(data)
        else:
            # Send empty or minimal data when not playing
            self.audio_visualization_data.emit([0] * 100)


import numpy as np
import threading
import queue


class AudioVisualizer(QWidget):
    """
    Audio visualizer widget that creates visual effects based on audio
    """
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(400, 200)
        self.setMaximumSize(800, 300)
        
        # Initialize audio data for visualization
        self.audio_data = [0] * 100
        self.is_playing = False
        self.simulation_data = [0] * 100  # Simulated data when not playing
        
        # Timer for animation when not connected to real audio
        self.animation_timer = QTimer(self)
        self.animation_timer.timeout.connect(self.animate_simulation)
        self.animation_timer.start(50)  # Update every 50ms
        
    def set_audio_data(self, data):
        """Update audio data for visualization from real audio"""
        if len(data) > 0:
            # Normalize the audio data to 0-100 range
            max_val = max(abs(x) for x in data) if data else 1
            if max_val != 0:
                self.audio_data = [(abs(x) / max_val) * 100 for x in data[:100]]
            else:
                self.audio_data = [0] * min(100, len(data))
            self.is_playing = True
        else:
            self.is_playing = False
        self.update()
        
    def animate_simulation(self):
        """Animate the visualizer with simulation data when no audio input"""
        if not self.is_playing:
            # Create a moving bar effect to show the visualizer is active
            import random
            # Smooth random values to create a more natural effect
            new_vals = []
            for i in range(len(self.simulation_data)):
                # Add some randomness but maintain smoothness
                change = random.uniform(-15, 15)
                new_val = max(0, min(100, self.simulation_data[i] + change))
                new_vals.append(new_val)
            self.simulation_data = new_vals
            self.update()
        
    def paintEvent(self, event):
        """Paint the visualizer"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        width = self.width()
        height = self.height()
        
        # Determine which data to use
        data_to_use = self.audio_data if self.is_playing else self.simulation_data
        
        bar_count = len(data_to_use) if data_to_use else 100
        bar_width = width / bar_count if bar_count > 0 else 1
        
        for i, value in enumerate(data_to_use):
            x = i * bar_width
            bar_height = (value / 100) * height
            y = height - bar_height
            
            # Create gradient color based on value and position
            if self.is_playing:
                # More vibrant colors when playing
                r = min(255, int(150 + value * 1.05))
                g = min(255, int(100 + value * 0.5))
                b = min(255, int(50 + value * 0.3))
            else:
                # Muted colors when not playing
                intensity = min(150, int(value * 1.5))
                r = g = b = intensity
            
            color = QColor(r, g, b)
            painter.fillRect(x, y, bar_width - 1, bar_height, color)
            
            # Draw border around each bar
            painter.setPen(QColor(255, 255, 255, 50))  # Semi-transparent white
            painter.drawRect(x, y, bar_width - 1, bar_height)


class Ui_MainWindow(QMainWindow):
    def setupUi(self, MainWindow):
        if not MainWindow.objectName():
            MainWindow.setObjectName(u"MainWindow")
        MainWindow.resize(900, 700)
        
        # Initialize audio player
        self.player = AudioPlayer()
        
        # Central widget
        self.centralwidget = QWidget(MainWindow)
        self.centralwidget.setObjectName(u"centralwidget")
        self.main_layout = QVBoxLayout(self.centralwidget)
        
        # Create menu bar
        self.create_menu_bar(MainWindow)
        
        # Main content area
        self.content_frame = QFrame(self.centralwidget)
        self.content_layout = QVBoxLayout(self.content_frame)
        
        # Audio visualizer
        self.visualizer = AudioVisualizer(self.content_frame)
        self.content_layout.addWidget(self.visualizer)
        
        # Controls frame
        self.controls_frame = QFrame(self.content_frame)
        self.controls_frame.setFrameShape(QFrame.StyledPanel)
        self.controls_layout = QHBoxLayout(self.controls_frame)
        
        # Play controls
        self.play_button = QPushButton("Play", self.controls_frame)
        self.play_button.clicked.connect(self.play_track)
        self.controls_layout.addWidget(self.play_button)
        
        self.pause_button = QPushButton("Pause", self.controls_frame)
        self.pause_button.clicked.connect(self.pause_track)
        self.controls_layout.addWidget(self.pause_button)
        
        self.stop_button = QPushButton("Stop", self.controls_frame)
        self.stop_button.clicked.connect(self.stop_track)
        self.controls_layout.addWidget(self.stop_button)
        
        # Previous/Next buttons
        self.prev_button = QPushButton("Prev", self.controls_frame)
        self.prev_button.clicked.connect(self.previous_track)
        self.controls_layout.addWidget(self.prev_button)
        
        self.next_button = QPushButton("Next", self.controls_frame)
        self.next_button.clicked.connect(self.next_track)
        self.controls_layout.addWidget(self.next_button)
        
        # Volume control
        self.volume_label = QLabel("Vol:", self.controls_frame)
        self.controls_layout.addWidget(self.volume_label)
        
        self.volume_slider = QSlider(Qt.Horizontal, self.controls_frame)
        self.volume_slider.setRange(0, 100)
        self.volume_slider.setValue(70)
        self.volume_slider.valueChanged.connect(self.change_volume)
        self.controls_layout.addWidget(self.volume_slider)
        
        # Progress bar
        self.progress_bar = QProgressBar(self.controls_frame)
        self.progress_bar.setTextVisible(True)
        self.progress_bar.setRange(0, 100)
        self.controls_layout.addWidget(self.progress_bar)
        
        self.content_layout.addWidget(self.controls_frame)
        
        # Playlist section
        self.playlist_group = QGroupBox("Playlist", self.content_frame)
        self.playlist_layout = QVBoxLayout(self.playlist_group)
        
        self.playlist = QListWidget(self.playlist_group)
        self.playlist_layout.addWidget(self.playlist)
        
        self.content_layout.addWidget(self.playlist_group)
        
        self.main_layout.addWidget(self.content_frame)
        MainWindow.setCentralWidget(self.centralwidget)
        
        # Connect player signals
        self.player.position_changed.connect(self.update_progress)
        self.player.duration_changed.connect(self.update_duration)
        self.player.track_ended.connect(self.on_track_ended)
        self.player.audio_visualization_data.connect(self.visualizer.set_audio_data)
        
        # Set initial state
        self.change_volume(70)
        
    def create_menu_bar(self, MainWindow):
        """Create the menu bar with file options"""
        self.menubar = QMenuBar(MainWindow)
        self.menubar.setObjectName(u"menubar")
        MainWindow.setMenuBar(self.menubar)
        
        self.menuFile = QMenu("File", self.menubar)
        self.menubar.addAction(self.menuFile.menuAction())
        
        # Add "Load Directory" action
        self.action_load_directory = QAction("Load Directory", MainWindow)
        self.action_load_directory.triggered.connect(self.load_directory)
        self.menuFile.addAction(self.action_load_directory)
        
        # Status bar
        self.statusbar = QStatusBar(MainWindow)
        MainWindow.setStatusBar(self.statusbar)
    
    def load_directory(self):
        """Load MP3 files from selected directory and subdirectories"""
        directory = QFileDialog.getExistingDirectory(
            self, "Select Directory with MP3 Files", "")
        
        if directory:
            # Clear current playlist
            self.playlist.clear()
            
            # Scan directory and subdirectories for MP3 files
            mp3_files = []
            for root, dirs, files in os.walk(directory):
                for file in files:
                    if file.lower().endswith('.mp3'):
                        mp3_files.append(os.path.join(root, file))
            
            # Add files to playlist
            for file_path in sorted(mp3_files):
                self.playlist.addItem(file_path)
                
            # Update status
            self.statusbar.showMessage(f"Loaded {len(mp3_files)} MP3 files")
    
    def play_track(self):
        """Play the currently selected track"""
        current_item = self.playlist.currentItem()
        if current_item:
            filepath = current_item.text()
            if self.player.load_track(filepath):
                self.player.play()
                self.statusbar.showMessage(f"Playing: {os.path.basename(filepath)}")
    
    def pause_track(self):
        """Pause/resume the current track"""
        self.player.pause()
    
    def stop_track(self):
        """Stop playback"""
        self.player.stop()
        self.progress_bar.setValue(0)
        self.statusbar.showMessage("Stopped")
    
    def previous_track(self):
        """Play the previous track"""
        current_row = self.playlist.currentRow()
        if current_row > 0:
            self.playlist.setCurrentRow(current_row - 1)
            self.play_track()
    
    def next_track(self):
        """Play the next track"""
        current_row = self.playlist.currentRow()
        if current_row < self.playlist.count() - 1:
            self.playlist.setCurrentRow(current_row + 1)
            self.play_track()
    
    def change_volume(self, value):
        """Change playback volume"""
        self.player.set_volume(value)
    
    def update_progress(self, position_ms):
        """Update the progress bar with the current position in milliseconds"""
        # Calculate percentage based on duration
        if hasattr(self.player, 'total_time') and self.player.total_time > 0:
            percentage = (position_ms / 1000.0) / self.player.total_time
            if percentage > 1.0:
                percentage = 1.0
            value = int(percentage * 100)
            self.progress_bar.setValue(value)
            # Update label with time
            current_time = position_ms // 1000  # Convert to seconds
            mins = current_time // 60
            secs = current_time % 60
            duration = self.player.total_time
            d_mins = duration // 60
            d_secs = duration % 60
            self.progress_bar.setFormat(f"{mins:02d}:{secs:02d} / {d_mins:02d}:{d_secs:02d}")
        else:
            # If duration is not known, just update the progress bar value
            self.progress_bar.setValue(position_ms // 1000 if position_ms < 100 else 0)
    
    def update_duration(self, duration_ms):
        """Update the progress bar max value with duration in milliseconds"""
        # Duration in milliseconds, convert to seconds for internal use
        self.progress_bar.setMaximum(100)
        # Update label with duration
        duration = duration_ms // 1000  # Convert to seconds
        mins = duration // 60
        secs = duration % 60
        current_text = self.progress_bar.format().split(" / ")[0] if " / " in self.progress_bar.format() else "00:00"
        self.progress_bar.setFormat(f"{current_text} / {mins:02d}:{secs:02d}")
    
    def on_track_ended(self):
        """Handle when track ends"""
        self.statusbar.showMessage("Track ended")
        # Automatically play next track
        self.next_track()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    MainWindow = QMainWindow()
    ui = Ui_MainWindow()
    ui.setupUi(MainWindow)
    MainWindow.show()
    sys.exit(app.exec())