// view.js - UI and DOM manipulation

class VideoEditorView {
    constructor(model) {
        this.model = model;
        this.clipElements = new Map(); // Maps clip objects to their DOM elements
        
        // Get DOM elements
        this.dropzone = document.getElementById('dropzone');
        this.timeline = document.getElementById('timeline');
        this.playhead = document.getElementById('playhead');
        this.preview = document.getElementById('preview');
        this.ctx = this.preview.getContext('2d');
        this.thumbnailCanvas = document.getElementById('thumbnailCanvas');
        this.thumbnailCtx = this.thumbnailCanvas.getContext('2d');
        this.thumbnailPreview = document.getElementById('thumbnailPreview');
        this.playBtn = document.getElementById('playBtn');
        this.stopBtn = document.getElementById('stopBtn');
        this.deleteSelectedBtn = document.getElementById('deleteSelectedBtn');
        this.exportBtn = document.getElementById('exportBtn');
        this.selectFilesBtn = document.getElementById('selectFilesBtn');
        this.fileInput = document.getElementById('fileInput');
        
        // Initialize the UI - event listeners will be set up by controller after it's connected
        this.drawInitialCanvas();
    }

    setupEventListeners() {
        // Play button
        this.playBtn.addEventListener('click', () => {
            this.controller.handlePlayPause();
        });

        // Stop button
        this.stopBtn.addEventListener('click', () => {
            this.controller.handleStop();
        });

        // Delete selected button
        this.deleteSelectedBtn.addEventListener('click', () => {
            this.controller.handleDeleteSelected();
        });

        // Export button
        this.exportBtn.addEventListener('click', () => {
            this.controller.handleExport();
        });

        // File selection
        this.selectFilesBtn.addEventListener('click', () => {
            this.fileInput.click();
        });

        this.fileInput.addEventListener('change', (e) => {
            this.handleFileSelection(e.target.files);
        });

        // Setup drop zones
        this.setupDrop(this.dropzone);
        // Note: Removed timeline drop to prevent duplication
    }

    setupController(controller) {
        this.controller = controller;
    }

    setupDrop(target) {
        ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
            target.addEventListener(eventName, e => {
                e.preventDefault();
                e.stopPropagation();
            }, false);
        });

        target.addEventListener('dragenter', () => target.classList.add('dragover'));
        target.addEventListener('dragleave', () => target.classList.remove('dragover'));
        target.addEventListener('drop', (e) => {
            target.classList.remove('dragover');
            
            const files = Array.from(e.dataTransfer.files);
            this.handleFileSelection(files);
        });
    }

    handleFileSelection(fileList) {
        Array.from(fileList).forEach(file => {
            if (file.type.startsWith('video/') || file.type.startsWith('audio/')) {
                this.controller.handleFileDrop(file);
            }
        });
    }

    addClipToTimeline(clip, file) {
        const clipDiv = document.createElement('div');
        clipDiv.className = 'clip';
        clipDiv.textContent = file.name.substring(0, 10) + '...';
        clipDiv.style.width = (clip.dur * this.model.SCALE) + 'px';
        clipDiv.style.left = '0px';
        clipDiv.style.top = '10px';
        clipDiv.style.position = 'absolute';
        clipDiv.style.zIndex = clip.zIndex;
        
        // Store the clip ID in the element for reference
        clipDiv.dataset.clipId = this.clipElements.size;
        
        // Make it draggable
        this.makeDraggable(clipDiv, clip);
        
        // Make it selectable
        this.makeSelectable(clipDiv, clip);
        
        this.timeline.appendChild(clipDiv);
        
        // Store the mapping
        this.clipElements.set(clip, clipDiv);
        
        // Update UI to reflect current selection state
        this.updateClipSelectionUI();
    }

    makeDraggable(clipDiv, clip) {
        let dragOffset = 0;
        
        clipDiv.addEventListener('mousedown', e => {
            e.stopPropagation();
            dragOffset = e.clientX - parseFloat(clipDiv.style.left);
            
            const handleMouseMove = (e) => {
                const rect = this.timeline.getBoundingClientRect();
                const x = e.clientX - rect.left - dragOffset;
                clipDiv.style.left = Math.max(0, x) + 'px';
                clip.start = x / this.model.SCALE;
            };
            
            const handleMouseUp = () => {
                document.removeEventListener('mousemove', handleMouseMove);
                document.removeEventListener('mouseup', handleMouseUp);
                // Update model with new position
                this.controller.updateClipPosition(clip, clip.start);
            };
            
            document.addEventListener('mousemove', handleMouseMove);
            document.addEventListener('mouseup', handleMouseUp);
            
            // Bring this clip to the front when dragging
            const newZIndex = this.model.bringToFront(clip);
            clipDiv.style.zIndex = newZIndex;
        });
    }

    makeSelectable(clipDiv, clip) {
        clipDiv.addEventListener('click', e => {
            e.stopPropagation();
            
            // Update selection in model
            this.model.selectClip(clip);
            
            // Update UI for all clips
            this.updateClipSelectionUI();
        });
    }

    updateClipSelectionUI() {
        // Update UI for all clips based on their selection state
        this.model.clips.forEach(clip => {
            const clipDiv = this.clipElements.get(clip);
            if (clipDiv) {
                if (clip.isSelected) {
                    clipDiv.classList.add('selected');
                } else {
                    clipDiv.classList.remove('selected');
                }
            }
        });
    }

    updatePlayhead(time) {
        const position = time * this.model.SCALE;
        this.playhead.style.left = position + 'px';
    }

    updatePlayButton(isPlaying) {
        this.playBtn.textContent = isPlaying ? 'Pause' : 'Play';
    }

    updateMediaDisplay() {
        // Clear main canvas first
        this.ctx.fillStyle = 'black';
        this.ctx.fillRect(0, 0, this.preview.width, this.preview.height);

        // Get the top video at current time
        const topClip = this.model.getTopVideoAtTime(this.model.currentTime);
        
        if (topClip && topClip.media.readyState >= 3 && 
            topClip.media.videoWidth > 0 && topClip.media.videoHeight > 0) {
            
            try {
                // Draw the video to main canvas
                this.ctx.drawImage(topClip.media, 0, 0, this.preview.width, this.preview.height);
                
                // Draw thumbnail preview
                this.thumbnailPreview.style.display = 'block';
                this.thumbnailCtx.fillStyle = 'black';
                this.thumbnailCtx.fillRect(0, 0, this.thumbnailCanvas.width, this.thumbnailCanvas.height);
                this.thumbnailCtx.drawImage(topClip.media, 0, 0, this.thumbnailCanvas.width, this.thumbnailCanvas.height);
            } catch (e) {
                console.error('Error drawing video to canvas:', e);
                this.thumbnailPreview.style.display = 'none';
            }
        } else {
            this.thumbnailPreview.style.display = 'none';
        }
    }

    drawInitialCanvas() {
        this.ctx.fillStyle = 'black';
        this.ctx.fillRect(0, 0, this.preview.width, this.preview.height);
    }

    // Method to handle playhead dragging
    setupPlayheadDragging() {
        this.playhead.addEventListener('mousedown', (e) => {
            e.preventDefault();
            this.controller.handlePlayheadDragStart(e);
        });
    }
}