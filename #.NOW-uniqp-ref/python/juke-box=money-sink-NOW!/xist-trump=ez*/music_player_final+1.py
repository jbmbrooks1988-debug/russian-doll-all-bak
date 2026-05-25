from PySide6.QtCore import (QCoreApplication, QMetaObject, QObject, 
                            QPoint, QRect, QSize, QTime, QUrl, Qt,
                            QTimer, Signal, Qt)
from PySide6.QtGui import (QAction, QBrush, QColor, QCursor, 
                          QFont, QPainter, QPalette)
from PySide6.QtWidgets import (QApplication, QFrame, QGroupBox,
                              QLabel, QLineEdit, QMainWindow, QMenu,
                              QMenuBar, QPushButton, QSizePolicy, QSlider,
                              QStatusBar, QVBoxLayout, QWidget, QFileDialog,
                              QListWidget, QListWidgetItem, QProgressBar, QHBoxLayout,
                              QTabWidget, QCheckBox)
import pygame
import os
import sys
import time
import math


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
        self.visualization_mode = "bars"  # Default mode
        
        # Initialize EQ settings
        self.highs_gain = 0  # in dB
        self.mids_gain = 0  # in dB
        self.lows_gain = 0  # in dB
        self.distortion_enabled = False
        
        # Timer for animation when not connected to real audio
        self.animation_timer = QTimer(self)
        self.animation_timer.timeout.connect(self.animate_simulation)
        self.animation_timer.start(50)  # Update every 50ms
        
    def set_visualization_mode(self, mode):
        """Set the current visualization mode"""
        self.visualization_mode = mode
        self.update()
    
    def set_eq_values(self, highs, mids, lows):
        """Set EQ values for visualization enhancement"""
        self.highs_gain = highs
        self.mids_gain = mids
        self.lows_gain = lows
        self.update()
    
    def set_distortion_enabled(self, enabled):
        """Enable or disable distortion effect in visualization"""
        self.distortion_enabled = enabled
        self.update()
        
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
        """Paint the visualizer based on the selected mode"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        width = self.width()
        height = self.height()
        
        # Determine which data to use
        data_to_use = self.audio_data if self.is_playing else self.simulation_data
        
        if self.visualization_mode == "bars":
            self._draw_bars(painter, width, height, data_to_use)
        elif self.visualization_mode == "waveform":
            self._draw_waveform(painter, width, height, data_to_use)
        elif self.visualization_mode == "spectrum":
            self._draw_spectrum(painter, width, height, data_to_use)
        elif self.visualization_mode == "particles":
            self._draw_particles(painter, width, height, data_to_use)
        else:  # Default to bars
            self._draw_bars(painter, width, height, data_to_use)
    
    def _draw_bars(self, painter, width, height, data):
        """Draw the standard bar visualization"""
        bar_count = len(data) if data else 100
        bar_width = width / bar_count if bar_count > 0 else 1
        
        for i, value in enumerate(data):
            x = i * bar_width
            # Apply EQ adjustments based on frequency band
            adjusted_value = value
            if self.is_playing:
                # Apply EQ based on frequency range
                freq_band = i / len(data)  # 0.0 to 1.0 representing low to high frequencies
                
                # Adjust gain based on which frequency band this represents
                if freq_band < 0.33:  # Low frequencies
                    adjustment = self.lows_gain / 10.0  # Lower divisor makes it more sensitive
                elif freq_band < 0.66:  # Mid frequencies
                    adjustment = self.mids_gain / 10.0
                else:  # High frequencies
                    adjustment = self.highs_gain / 10.0
                
                adjusted_value = max(0, min(100, value + adjustment))
            
            bar_height = (adjusted_value / 100) * height
            y = height - bar_height
            
            # Create gradient color based on value and position
            if self.is_playing:
                # More vibrant colors when playing - cycle through colors
                hue = (i * 3.6) % 360  # Cycle through colors
                # Apply some effects based on EQ settings
                sat_adjustment = 255
                val_adjustment = min(255, int(150 + adjusted_value * 1.05))
                
                if self.distortion_enabled:
                    # Distortion effect: more saturated colors, more extreme
                    sat_adjustment = min(255, int(sat_adjustment * 1.5))
                
                color = QColor.fromHsv(int(hue), sat_adjustment, val_adjustment)
            else:
                # Muted colors when not playing
                intensity = min(150, int(adjusted_value * 1.5))
                color = QColor(intensity, intensity, intensity)
            
            painter.fillRect(x, y, bar_width - 1, bar_height, color)
            
            # Draw border around each bar (more pronounced with distortion)
            border_alpha = 50
            if self.distortion_enabled and self.is_playing:
                border_alpha = 100
            painter.setPen(QColor(255, 255, 255, border_alpha))
            painter.drawRect(x, y, bar_width - 1, bar_height)
    
    def _draw_waveform(self, painter, width, height, data):
        """Draw the waveform visualization"""
        if not data:
            return
            
        points = []
        segment_width = width / (len(data) - 1) if len(data) > 1 else width
        
        for i, value in enumerate(data):
            x = i * segment_width
            # Apply EQ adjustments based on frequency band
            adjusted_value = value
            if self.is_playing:
                freq_band = i / len(data)
                
                if freq_band < 0.33:  # Low frequencies
                    adjustment = self.lows_gain / 10.0
                elif freq_band < 0.66:  # Mid frequencies
                    adjustment = self.mids_gain / 10.0
                else:  # High frequencies
                    adjustment = self.highs_gain / 10.0
                
                adjusted_value = max(-100, min(100, value + adjustment))
            
            y = height / 2 - (adjusted_value / 100) * (height / 2)
            points.append(QPoint(int(x), int(y)))
        
        if len(points) > 1:
            # Draw the waveform curve
            pen_thickness = 2
            if self.distortion_enabled:
                pen_thickness = 3  # Thicker line for distortion effect
            waveform_color = QColor(0, 200, 255)
            if self.distortion_enabled:
                waveform_color = QColor(255, 100, 100)  # Red color for distortion
                
            painter.setPen(QPen(waveform_color, pen_thickness))
            painter.drawPolyline(points)
            
            # Draw filled area under the curve
            bottom_point = QPoint(width, height)
            top_point = QPoint(0, height)
            points.append(bottom_point)
            points.insert(0, top_point)
            
            gradient = QLinearGradient(0, 0, 0, height)
            if self.is_playing:
                if self.distortion_enabled:
                    gradient.setColorAt(0, QColor(255, 50, 50, 100))
                    gradient.setColorAt(1, QColor(180, 50, 50, 50))
                else:
                    gradient.setColorAt(0, QColor(0, 150, 255, 100))
                    gradient.setColorAt(1, QColor(0, 50, 150, 50))
            else:
                gradient.setColorAt(0, QColor(100, 100, 100, 50))
                gradient.setColorAt(1, QColor(50, 50, 50, 25))
                
            painter.setPen(Qt.NoPen)
            painter.setBrush(gradient)
            painter.drawPolygon(points[:-2])  # Don't include the extra points added for fill
    
    def _draw_spectrum(self, painter, width, height, data):
        """Draw the spectrum analyzer visualization"""
        bar_count = len(data) if data else 100
        bar_width = width / bar_count if bar_count > 0 else 1
        
        for i, value in enumerate(data):
            x = i * bar_width
            # Apply EQ adjustments based on frequency band
            adjusted_value = value
            if self.is_playing:
                freq_band = i / len(data)
                
                if freq_band < 0.33:  # Low frequencies
                    adjustment = self.lows_gain / 10.0
                elif freq_band < 0.66:  # Mid frequencies
                    adjustment = self.mids_gain / 10.0
                else:  # High frequencies
                    adjustment = self.highs_gain / 10.0
                
                adjusted_value = max(0, min(100, value + adjustment))
            
            bar_height = (adjusted_value / 100) * height
            y = height - bar_height
            
            # Create a rainbow color effect
            hue = (i * 3.6) % 360  # Spread across the color spectrum
            saturation = 255
            value_brightness = min(255, int(100 + adjusted_value * 1.55))
            
            if self.is_playing:
                color = QColor.fromHsv(int(hue), saturation, value_brightness)
                if self.distortion_enabled:
                    # Distortion effect: increase saturation even more
                    color = QColor.fromHsv(int(hue), min(255, saturation + 50), value_brightness)
            else:
                color = QColor(100, 100, 100)
            
            painter.fillRect(x, y, bar_width - 1, bar_height, color)
            
            # Add glow effect when playing
            if self.is_playing:
                pen_color = color.lighter(150)
                if self.distortion_enabled:
                    pen_color = QColor(255, 255, 255)  # Bright white for distortion
                pen = QPen(pen_color, 1)
                painter.setPen(pen)
                painter.drawLine(x, y, x + bar_width - 1, y)  # Top line
    
    def _draw_particles(self, painter, width, height, data):
        """Draw the particle visualization"""
        import random
        from math import sin, cos, pi
        
        painter.setPen(Qt.NoPen)
        
        # Create particles based on audio data
        for i, value in enumerate(data):
            if value > 5:  # Only draw particles for significant values
                # Calculate position based on frequency band and magnitude
                angle = (i / len(data)) * 2 * pi
                # Apply EQ adjustments based on frequency band
                adjusted_value = value
                if self.is_playing:
                    freq_band = i / len(data)
                    
                    if freq_band < 0.33:  # Low frequencies
                        adjustment = self.lows_gain / 10.0
                    elif freq_band < 0.66:  # Mid frequencies
                        adjustment = self.mids_gain / 10.0
                    else:  # High frequencies
                        adjustment = self.highs_gain / 10.0
                    
                    adjusted_value = max(0, min(100, value + adjustment))
                
                distance = (adjusted_value / 100) * min(width, height) * 0.4
                center_x = width / 2
                center_y = height / 2
                x = center_x + cos(angle) * distance
                y = center_y + sin(angle) * distance
                
                # Particle size based on value
                size = max(2, adjusted_value / 15)
                if self.distortion_enabled:
                    size *= 1.5  # Larger particles for distortion effect
                
                # Color based on frequency band
                hue = (i * 3.6) % 360
                if self.is_playing:
                    if self.distortion_enabled:
                        color = QColor.fromHsv(int(hue), min(255, 255), min(255, int(200 + adjusted_value)))
                    else:
                        color = QColor.fromHsv(int(hue), 255, min(255, int(150 + adjusted_value * 1.05)))
                else:
                    color = QColor(100, 100, 100)
                
                painter.setBrush(color)
                painter.drawEllipse(int(x - size/2), int(y - size/2), int(size), int(size))
                
                # Draw connecting lines for a more dynamic effect
                if self.is_playing and i > 0 and data[i-1] > 5:
                    prev_angle = ((i-1) / len(data)) * 2 * pi
                    # Apply EQ to previous point as well
                    prev_freq_band = (i-1) / len(data)
                    prev_adjustment = 0
                    if prev_freq_band < 0.33:  # Low frequencies
                        prev_adjustment = self.lows_gain / 10.0
                    elif prev_freq_band < 0.66:  # Mid frequencies
                        prev_adjustment = self.mids_gain / 10.0
                    else:  # High frequencies
                        prev_adjustment = self.highs_gain / 10.0
                    
                    prev_adjusted_value = max(0, min(100, data[i-1] + prev_adjustment))
                    prev_distance = (prev_adjusted_value / 100) * min(width, height) * 0.4
                    prev_x = center_x + cos(prev_angle) * prev_distance
                    prev_y = center_y + sin(prev_angle) * prev_distance
                    
                    line_color = color.darker(200)
                    if self.distortion_enabled:
                        line_color = QColor(255, 255, 255)  # White lines for distortion
                    
                    painter.setPen(QPen(line_color, 1))
                    painter.drawLine(int(prev_x), int(prev_y), int(x), int(y))


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


class Ui_MainWindow(QMainWindow):
    def setupUi(self, MainWindow):
        if not MainWindow.objectName():
            MainWindow.setObjectName(u"MainWindow")
        MainWindow.resize(900, 700)
        
        # Initialize audio player
        self.player = AudioPlayer()
        
        # Initialize EQ settings
        self.eq_settings = {
            'highs': 0,
            'mids': 0,
            'lows': 0,
            'distortion': False
        }
        
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
        
        # Set the default visualization mode
        self.visualizer.set_visualization_mode("bars")
        
        # Create visualization options
        self.setup_visualization_options()
        
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
        
        # Create tab widget for playlist and EQ
        self.tab_widget = QTabWidget(self.content_frame)
        
        # Playlist tab
        self.playlist_tab = QWidget()
        self.playlist_layout = QVBoxLayout(self.playlist_tab)
        
        # Search bar for playlist
        self.search_layout = QHBoxLayout()
        self.search_label = QLabel("Search:", self.playlist_tab)
        self.search_input = QLineEdit(self.playlist_tab)
        self.search_input.setPlaceholderText("Enter song name to filter...")
        self.search_input.textChanged.connect(self.filter_playlist)
        
        self.search_layout.addWidget(self.search_label)
        self.search_layout.addWidget(self.search_input)
        
        # Shuffle button
        self.shuffle_button = QPushButton("Shuffle", self.playlist_tab)
        self.shuffle_button.clicked.connect(self.shuffle_playlist)
        self.search_layout.addWidget(self.shuffle_button)
        
        self.playlist_layout.addLayout(self.search_layout)
        
        self.playlist = QListWidget(self.playlist_tab)
        self.playlist_layout.addWidget(self.playlist)
        
        self.tab_widget.addTab(self.playlist_tab, "Playlist")
        
        # EQ Tab
        self.eq_tab = QWidget()
        self.eq_layout = QVBoxLayout(self.eq_tab)
        
        # Create sliders for EQ controls
        # High frequencies
        self.highs_layout = QHBoxLayout()
        self.highs_label = QLabel("Highs:", self.eq_tab)
        self.highs_slider = QSlider(Qt.Horizontal, self.eq_tab)
        self.highs_slider.setRange(-20, 20)  # Range from -20dB to +20dB
        self.highs_slider.setValue(0)
        self.highs_value_label = QLabel("0dB", self.eq_tab)
        self.highs_slider.valueChanged.connect(lambda v: self.highs_value_label.setText(f"{v}dB"))
        
        self.highs_layout.addWidget(self.highs_label)
        self.highs_layout.addWidget(self.highs_slider)
        self.highs_layout.addWidget(self.highs_value_label)
        
        # Mid frequencies
        self.mids_layout = QHBoxLayout()
        self.mids_label = QLabel("Mids:", self.eq_tab)
        self.mids_slider = QSlider(Qt.Horizontal, self.eq_tab)
        self.mids_slider.setRange(-20, 20)  # Range from -20dB to +20dB
        self.mids_slider.setValue(0)
        self.mids_value_label = QLabel("0dB", self.eq_tab)
        self.mids_slider.valueChanged.connect(lambda v: self.mids_value_label.setText(f"{v}dB"))
        
        self.mids_layout.addWidget(self.mids_label)
        self.mids_layout.addWidget(self.mids_slider)
        self.mids_layout.addWidget(self.mids_value_label)
        
        # Low frequencies
        self.lows_layout = QHBoxLayout()
        self.lows_label = QLabel("Lows:", self.eq_tab)
        self.lows_slider = QSlider(Qt.Horizontal, self.eq_tab)
        self.lows_slider.setRange(-20, 20)  # Range from -20dB to +20dB
        self.lows_slider.setValue(0)
        self.lows_value_label = QLabel("0dB", self.eq_tab)
        self.lows_slider.valueChanged.connect(lambda v: self.lows_value_label.setText(f"{v}dB"))
        
        self.lows_layout.addWidget(self.lows_label)
        self.lows_layout.addWidget(self.lows_slider)
        self.lows_layout.addWidget(self.lows_value_label)
        
        # Distortion/Overdrive checkbox
        self.distortion_checkbox = QCheckBox("Overdrive/Distortion", self.eq_tab)
        
        # Add all to EQ layout
        self.eq_layout.addLayout(self.highs_layout)
        self.eq_layout.addLayout(self.mids_layout)
        self.eq_layout.addLayout(self.lows_layout)
        self.eq_layout.addWidget(self.distortion_checkbox)
        
        # Add stretch to push everything up
        self.eq_layout.addStretch()
        
        self.tab_widget.addTab(self.eq_tab, "EQ")
        
        self.content_layout.addWidget(self.tab_widget)
        
        # Connect EQ controls to update function
        self.highs_slider.valueChanged.connect(self.update_eq_settings)
        self.mids_slider.valueChanged.connect(self.update_eq_settings)
        self.lows_slider.valueChanged.connect(self.update_eq_settings)
        self.distortion_checkbox.stateChanged.connect(self.update_distortion_setting)
        
        self.main_layout.addWidget(self.content_frame)
        MainWindow.setCentralWidget(self.centralwidget)
        
        # Connect player signals
        self.player.position_changed.connect(self.update_progress)
        self.player.duration_changed.connect(self.update_duration)
        self.player.track_ended.connect(self.on_track_ended)
        self.player.audio_visualization_data.connect(self.visualizer.set_audio_data)
        
        # Set the default visualization mode after everything is initialized
        self.visualizer.set_visualization_mode("bars")
        
    def setup_visualization_options(self):
        """Setup visualization options menu"""
        # Create visualization menu
        self.menuVisualization = QMenu("Visualization", self.menubar)
        self.menubar.addAction(self.menuVisualization.menuAction())
        
        # Add visualization mode actions
        self.visual_modes = [
            ("Bars", "bars"),
            ("Waveform", "waveform"),
            ("Spectrum", "spectrum"),
            ("Particles", "particles")
        ]
        
        self.visual_mode_actions = {}
        for mode_name, mode_id in self.visual_modes:
            action = QAction(mode_name, self)
            action.setCheckable(True)
            action.triggered.connect(lambda checked, m=mode_id: self.set_visualization_mode(m) if checked else None)
            self.visual_mode_actions[mode_id] = action
            self.menuVisualization.addAction(action)
            
        # Set default visualization mode
        self.visual_mode = "bars"
        self.visual_mode_actions["bars"].setChecked(True)
    
    def set_visualization_mode(self, mode):
        """Set the visualization mode"""
        self.visual_mode = mode
        # Uncheck all other modes
        for mode_id, action in self.visual_mode_actions.items():
            action.setChecked(mode_id == mode)
        # Update visualizer
        self.visualizer.set_visualization_mode(mode)
        
        # Set initial state
        self.change_volume(70)
        
        # Store original playlist for search functionality
        self.original_playlist_items = []
        
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
            self.original_playlist_items = []  # Reset stored items
            
            # Scan directory and subdirectories for MP3 files
            mp3_files = []
            for root, dirs, files in os.walk(directory):
                for file in files:
                    if file.lower().endswith('.mp3'):
                        mp3_files.append(os.path.join(root, file))
            
            # Add files to playlist and store in original list
            for file_path in sorted(mp3_files):
                self.playlist.addItem(file_path)
                self.original_playlist_items.append(file_path)
                
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
    
    def filter_playlist(self, text):
        """Filter playlist based on search text"""
        for i in range(self.playlist.count()):
            item = self.playlist.item(i)
            item.setHidden(text.lower() not in item.text().lower())
    
    def shuffle_playlist(self):
        """Shuffle the current playlist"""
        import random
        
        # Save the current search filter
        current_search = self.search_input.text()
        
        # Shuffle the original playlist items
        self.original_playlist_items = self.original_playlist_items[:]  # Make a copy
        random.shuffle(self.original_playlist_items)
        
        # Update the playlist with shuffled items, respecting current search filter
        self.playlist.clear()
        for item_text in self.original_playlist_items:
            list_item = QListWidgetItem(item_text)
            # Apply the current search filter
            list_item.setHidden(current_search.lower() not in item_text.lower())
            self.playlist.addItem(list_item)
        
        self.statusbar.showMessage(f"Playlist shuffled: {len(self.original_playlist_items)} tracks")
    
    def update_eq_settings(self):
        """Update the EQ settings from slider values"""
        self.eq_settings['highs'] = self.highs_slider.value()
        self.eq_settings['mids'] = self.mids_slider.value()
        self.eq_settings['lows'] = self.lows_slider.value()
        
        # Update visualization with new EQ settings
        self.visualizer.set_eq_values(
            self.eq_settings['highs'],
            self.eq_settings['mids'],
            self.eq_settings['lows']
        )
    
    def update_distortion_setting(self, state):
        """Update the distortion setting"""
        self.eq_settings['distortion'] = bool(state)
        
        # Update visualization to reflect distortion
        self.visualizer.set_distortion_enabled(self.eq_settings['distortion'])


if __name__ == "__main__":
    app = QApplication(sys.argv)
    MainWindow = QMainWindow()
    ui = Ui_MainWindow()
    ui.setupUi(MainWindow)
    MainWindow.show()
    sys.exit(app.exec())