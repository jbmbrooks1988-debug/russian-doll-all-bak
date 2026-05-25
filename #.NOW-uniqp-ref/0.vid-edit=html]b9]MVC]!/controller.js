// controller.js - Handles user interactions and coordinates between model and view

class VideoEditorController {
    constructor(model, view) {
        this.model = model;
        this.view = view;
        
        // Animation control
        this.animationId = null;
        this.startTime = 0;
        
        // Initialize the view with this controller
        this.view.setupController(this);
        this.view.setupEventListeners();
        this.view.setupPlayheadDragging();
    }

    handleFileDrop(file) {
        console.log('Processing file:', file.name, file.type);
        
        const media = document.createElement(file.type.startsWith('video/') ? 'video' : 'audio');
        const fileUrl = URL.createObjectURL(file);
        media.src = fileUrl;
        media.preload = 'metadata';
        media.muted = true; // Mute for preview to avoid overlap noise
        media.playsInline = true; // For better mobile compatibility
        document.body.appendChild(media); // Append to DOM (hidden via CSS) for proper decoding/playback

        media.onloadedmetadata = () => {
            const duration = media.duration;
            if (isNaN(duration) || duration <= 0) {
                console.warn('Invalid duration for', file.name);
                // Clean up on error
                if (media.parentNode) {
                    media.parentNode.removeChild(media);
                }
                URL.revokeObjectURL(fileUrl);
                return;
            }
            
            // Add clip to model
            const clip = this.model.addClip(media, file, duration);
            
            // Add to view
            this.view.addClipToTimeline(clip, file);
            
            console.log('Successfully added clip:', file.name);
        };
        
        media.onerror = (err) => {
            console.error('Error loading media:', file.name, err);
            // Clean up on error
            if (media.parentNode) {
                media.parentNode.removeChild(media);
            }
            URL.revokeObjectURL(fileUrl);
        };
    }

    handlePlayPause() {
        if (this.model.isPlaying) {
            this.pause();
        } else {
            this.play();
        }
    }

    handleStop() {
        this.stop();
    }

    handleDeleteSelected() {
        const deleted = this.model.deleteSelectedClip();
        if (deleted) {
            // TODO: Update the view to reflect the deletion
            console.log('Clip deleted');
        } else {
            console.log('No clip selected to delete');
        }
    }

    handleExport() {
        if (this.model.clips.length === 0) {
            alert('No clips to export!');
            return;
        }

        // Basic export using MediaRecorder on canvas (simulates playback)
        const recordedChunks = [];
        try {
            const stream = this.view.preview.captureStream(30); // 30 FPS
            const mediaRecorder = new MediaRecorder(stream, { mimeType: 'video/webm' });

            mediaRecorder.ondataavailable = e => {
                if (e.data.size > 0) recordedChunks.push(e.data);
            };

            mediaRecorder.onstop = () => {
                const blob = new Blob(recordedChunks, { type: 'video/webm' });
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = 'edited-video.webm';
                a.click();
                
                // Cleanup
                this.model.clips.forEach(clip => URL.revokeObjectURL(clip.url));
            };

            mediaRecorder.start();
            // Simulate playback for recording
            const originalIsPlaying = this.model.isPlaying;
            const originalTime = this.model.currentTime;
            this.model.currentTime = 0;
            this.view.updatePlayhead(0);
            this.play(); // Start playback
            setTimeout(() => {
                this.pause();
                this.model.currentTime = originalTime;
                this.view.updatePlayhead(originalTime);
                if (originalIsPlaying) this.play(); // Restore if was playing
                mediaRecorder.stop();
            }, (Math.max(...this.model.clips.map(c => c.start + c.dur)) * 1000) + 1000); // +1s buffer
        } catch (err) {
            console.error('Error starting MediaRecorder:', err);
            alert('Failed to start recording: ' + err.message);
        }
    }

    handlePlayheadDragStart(e) {
        this.model.updateIsPlaying(false); // Stop playback when dragging
        this.view.updatePlayButton(false);
        
        const handleDrag = (e) => {
            const rect = this.view.timeline.getBoundingClientRect();
            let x = e.clientX - rect.left;
            x = Math.max(0, Math.min(x, rect.width)); // Constrain within timeline
            this.view.playhead.style.left = x + 'px';
            
            // Update current time based on position
            this.model.setCurrentTime(x / this.model.SCALE);
            this.view.updateMediaDisplay();
        };

        const handleEnd = () => {
            document.removeEventListener('mousemove', handleDrag);
            document.removeEventListener('mouseup', handleEnd);
        };

        document.addEventListener('mousemove', handleDrag);
        document.addEventListener('mouseup', handleEnd);
    }

    updateClipPosition(clip, newStart) {
        // Just update the start time directly on the clip
        clip.start = newStart;
    }

    play() {
        if (this.model.isPlaying) return; // Prevent multiple simultaneous plays
        
        this.model.updateIsPlaying(true);
        this.startTime = performance.now() - (this.model.currentTime * 1000);
        this.view.updatePlayButton(true);
        
        // Cancel any existing animation frame to prevent multiple loops
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
        }
        
        this.animate();
    }

    pause() {
        this.model.updateIsPlaying(false);
        this.view.updatePlayButton(false);
        
        // Cancel the animation frame when pausing
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
            this.animationId = null;
        }
        
        this.view.updateMediaDisplay();
    }

    stop() {
        this.model.updateIsPlaying(false);
        this.model.setCurrentTime(0);
        this.view.updatePlayButton(false);
        
        // Cancel the animation frame when stopping
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
            this.animationId = null;
        }
        
        // Pause all media elements
        this.model.clips.forEach(clip => {
            if (!clip.media.paused) {
                clip.media.pause();
            }
            clip.media.currentTime = 0;
        });
        
        this.view.updateMediaDisplay();
        this.view.updatePlayhead(0);
    }

    animate() {
        if (!this.model.isPlaying) {
            this.animationId = null; // Clear the animation ID when not playing
            return;
        }
        
        const now = performance.now();
        this.model.setCurrentTime((now - this.startTime) / 1000);
        
        // Constrain currentTime to prevent exceeding total duration
        const maxEnd = Math.max(...this.model.clips.map(c => c.start + c.dur));
        this.model.currentTime = Math.min(this.model.currentTime, maxEnd);
        
        this.view.updatePlayhead(this.model.currentTime);
        
        // Update media states in the model
        this.model.updateMediaStates();
        
        // Update display in the view
        this.view.updateMediaDisplay();

        // Check if end of all clips
        if (this.model.currentTime >= maxEnd) {
            this.stop();
        } else {
            this.animationId = requestAnimationFrame(this.animate.bind(this));
        }
    }
}