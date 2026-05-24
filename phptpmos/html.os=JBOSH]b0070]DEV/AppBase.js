// Base class for all applications in the multi-window system
class AppBase {
    constructor(jobId = null) {
        this.gameWindow = null;  // Each instance needs its own window reference
        this.isInitialized = false;
        this.focusHandlerAdded = false;
        this.jobId = jobId; // Store the job ID if provided
        
        // Unique ID for this instance
        this.instanceId = this.constructor.name.toLowerCase() + '_instance_' + Date.now() + Math.floor(Math.random() * 1000);

        // Register instance in a global map
        if (!window.appInstances) {
            window.appInstances = {};
        }
        window.appInstances[this.instanceId] = this;
    }

    // Common initialization method
    init() {
        console.log(`${this.constructor.name} Initialized`);
        this.createGUI();
        this.showGUI();
        this.isInitialized = true;
    }
    
    // Common method to create the GUI window
    createGUI() {
        // This should be overridden by subclasses
        throw new Error('createGUI must be implemented by subclass');
    }
    
    // Common method to show the GUI window
    showGUI() {
        // This should be overridden by subclasses, but with common functionality
        this.createGUI();
        
        if (this.gameWindow) {
            this.gameWindow.style.display = 'block';
            this.gameWindow.style.zIndex = 100; // Ensure it appears on top
            this.gameWindow.style.position = 'absolute';
            
            // Default position in center of browser window
            const maxWidth = Math.min(this.getDefaultWidth(), window.innerWidth - 100);
            const maxHeight = Math.min(this.getDefaultHeight(), window.innerHeight - 100);
            const width = maxWidth;
            const height = maxHeight;
            
            this.gameWindow.style.width = width + 'px';
            this.gameWindow.style.height = height + 'px';
            
            this.gameWindow.style.left = (window.innerWidth/2 - width/2) + 'px';
            this.gameWindow.style.top = (window.innerHeight/2 - height/2) + 'px';
            
            // Add focus handler to this window if not already added
            if (!this.focusHandlerAdded) {
                this.addFocusHandlerToWindow();
                this.focusHandlerAdded = true;
            }
        }
    }
    
    // Default dimensions, can be overridden
    getDefaultWidth() {
        return 800; // Default width
    }
    
    getDefaultHeight() {
        return 600; // Default height
    }
    
    // Common focus handler for all app windows
    addFocusHandlerToWindow() {
        if (this.gameWindow) {
            const instanceId = this.instanceId; // Capture instanceId in closure
            this.gameWindow.addEventListener('mousedown', function(e) {
                // Remove .active class from all terminal windows
                document.querySelectorAll('.terminal-window').forEach(w => {
                    w.classList.remove('active');
                });
                // Add .active class to the clicked window
                this.gameWindow.classList.add('active');
                
                // Bring the clicked window to the front
                if (window.currentZ === undefined) {
                    window.currentZ = 100; // Initialize if not already set
                }
                this.gameWindow.style.zIndex = window.currentZ++;
                
                // Focus on input if it exists and the click was not on the title bar
                if (!e.target.closest('.title-bar') && !e.target.closest('.close-btn')) {
                    const input = this.gameWindow.querySelector('.cmd-input-span') || 
                                 this.gameWindow.querySelector(`#${instanceId}-canvasContainer`);
                    if (input) {
                        input.focus();
                    }
                }
            }.bind(this)); // Use bind to maintain reference to the current instance
            
            // Make the window draggable via its title bar (same as regular terminals)
            if (typeof window.makeDraggable === 'function') {
                window.makeDraggable(this.gameWindow);
            }
        }
    }
    
    // Common method to close the GUI window
    closeGUI(shouldNotifyProcessManager = true) {
        // Prevent multiple close attempts
        if (this.isClosing) {
            return; // Already in the process of closing
        }
        this.isClosing = true;
        
        // Remove any corresponding minimized tab if it exists before closing the window
        if (this.gameWindow && this.gameWindow.minimizedTabId) {
            const tabElement = document.getElementById(this.gameWindow.minimizedTabId);
            if (tabElement) {
                tabElement.remove();
            }
        }
        
        // Always perform the window cleanup
        if (this.gameWindow) {
            this.gameWindow.remove();
        }
        if (window.appInstances && window.appInstances[this.instanceId]) {
            delete window.appInstances[this.instanceId];
        }
        
        // If this instance has a job ID and we should notify the ProcessManager, do so
        if (shouldNotifyProcessManager && this.jobId !== undefined && this.jobId !== null && 
            typeof window !== 'undefined' && window.processManager) {
            // This will trigger cleanup in the ProcessManager (removing from job table)
            window.processManager.handleWindowClose(this.jobId);
        }
        
        // Reset the closing flag after finishing
        this.isClosing = false;
    }
    
    // Helper function to get element by ID with instance prefix
    getEl(id) {
        return document.getElementById(`${this.instanceId}-${id}`);
    }
    
    // Common method to minimize the window
    minimizeWindow() {
        if (this.gameWindow) {
            // Hide the actual window
            this.gameWindow.style.display = 'none';
            
            // Create a tab for the minimized window using global function if available
            if (window.createMinimizedTab) {
                const title = this.getTitle();
                window.createMinimizedTab(this.gameWindow, title);
            } else {
                // Fallback to just hiding if the function is not available
                this.gameWindow.style.display = 'none';
            }
        }
    }
    
    // Get title for the minimized tab
    getTitle() {
        // Default implementation - can be overridden by subclasses
        return this.constructor.name;
    }
    
    // Common method to toggle fullscreen
    toggleFullscreen() {
        if (this.gameWindow) {
            if (!this.gameWindow.classList.contains('fullscreen')) {
                // Store original dimensions and position before going fullscreen
                this.originalWidth = this.gameWindow.style.width;
                this.originalHeight = this.gameWindow.style.height;
                this.originalLeft = this.gameWindow.style.left;
                this.originalTop = this.gameWindow.style.top;
                
                // Apply fullscreen styles
                this.gameWindow.style.width = '100%';
                this.gameWindow.style.height = 'calc(100vh - 40px)'; // Subtract toolbar height
                this.gameWindow.style.top = '0px';
                this.gameWindow.style.left = '0px';
                this.gameWindow.classList.add('fullscreen');
            } else {
                // Restore original dimensions and position
                this.gameWindow.style.width = this.originalWidth || '800px';
                this.gameWindow.style.height = this.originalHeight || '600px';
                this.gameWindow.style.top = this.originalTop || '50px';
                this.gameWindow.style.left = this.originalLeft || '50px';
                this.gameWindow.classList.remove('fullscreen');
            }
        }
    }
}

// Export the base class
if (typeof window !== 'undefined') {
    window.AppBase = AppBase;
}