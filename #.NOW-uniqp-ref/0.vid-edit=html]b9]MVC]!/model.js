// model.js - Data and business logic

class VideoEditorModel {
    constructor() {
        this.clips = [];
        this.currentTime = 0;
        this.isPlaying = false;
        this.SCALE = 10; // pixels per second
        this.highestZIndex = 10;
    }

    addClip(media, file, duration) {
        const url = URL.createObjectURL(file);
        const mediaElement = media;
        
        const clip = {
            media: mediaElement,
            start: 0,
            dur: duration,
            url: url,
            isSelected: false,
            zIndex: ++this.highestZIndex
        };
        
        this.clips.push(clip);
        return clip;
    }

    removeClip(clip) {
        const index = this.clips.indexOf(clip);
        if (index > -1) {
            this.clips.splice(index, 1);
            // Clean up the object URL
            URL.revokeObjectURL(clip.url);
            // Remove media element from DOM if needed
            if (clip.media.parentNode) {
                clip.media.parentNode.removeChild(clip.media);
            }
        }
    }

    deleteSelectedClip() {
        const selectedClip = this.clips.find(clip => clip.isSelected);
        if (selectedClip) {
            this.removeClip(selectedClip);
            return true;
        }
        return false;
    }

    selectClip(clipToSelect) {
        // Deselect all clips
        this.clips.forEach(clip => clip.isSelected = false);
        // Select the specified clip
        clipToSelect.isSelected = true;
    }

    bringToFront(clip) {
        this.highestZIndex++;
        clip.zIndex = this.highestZIndex;
        return this.highestZIndex;
    }

    getActiveClipsAtTime(time) {
        return this.clips.filter(clip => {
            const rel = time - clip.start;
            return rel >= 0 && rel < clip.dur;
        });
    }

    getTopVideoAtTime(time) {
        const activeClips = this.getActiveClipsAtTime(time);
        
        // Filter for video clips only and sort by z-index (highest first)
        const videoClips = activeClips
            .filter(clip => clip.media.tagName === 'VIDEO')
            .sort((a, b) => (b.zIndex || 0) - (a.zIndex || 0));
            
        return videoClips.length > 0 ? videoClips[0] : null;
    }

    updateClipPosition(clipId, newStart) {
        const clip = this.clips.find(c => c.id === clipId);
        if (clip) {
            clip.start = newStart;
        }
    }

    updateIsPlaying(state) {
        this.isPlaying = state;
    }

    setCurrentTime(time) {
        this.currentTime = time;
    }

    // Method to handle media state updates for playback
    updateMediaStates() {
        const activeClips = this.getActiveClipsAtTime(this.currentTime);
        
        // Update all clips
        this.clips.forEach(clip => {
            const rel = this.currentTime - clip.start;
            
            if (rel >= 0 && rel < clip.dur && clip.media.readyState >= 2) {
                // Update the time for active clips
                if (Math.abs(clip.media.currentTime - rel) > 0.2) {
                    clip.media.currentTime = rel;
                }
                
                // Handle playback based on isPlaying state
                if (this.isPlaying) {
                    // For videos, only the top video should play
                    if (clip.media.tagName === 'VIDEO') {
                        const topVideo = this.getTopVideoAtTime(this.currentTime);
                        if (topVideo && topVideo === clip && clip.media.paused) {
                            clip.media.play().catch(e => console.warn('Could not play video:', e));
                        } else if (topVideo && topVideo !== clip && !clip.media.paused) {
                            clip.media.pause();
                        }
                    } 
                    // For audio, play if not already playing
                    else if (clip.media.tagName === 'AUDIO' && clip.media.paused) {
                        clip.media.play().catch(e => console.warn('Could not play audio:', e));
                    }
                }
            } else {
                // Pause media if outside its time range
                if (!clip.media.paused) {
                    clip.media.pause();
                }
                if (Math.abs(clip.media.currentTime) > 0.1) {
                    clip.media.currentTime = 0;
                }
            }
        });
    }
}