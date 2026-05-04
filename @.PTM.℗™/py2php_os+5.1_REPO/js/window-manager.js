// window-manager.js - Core Window Management System
// Uses innerHTML pattern for dynamic window creation

class WindowManager {
    constructor() {
        this.windows = new Map();
        this.currentZ = 10;
        this.desktop = null;
        this.taskbarWindows = null;
        this.draggingWindow = null;
        this.resizingWindow = null;
        this.dragOffsetX = 0;
        this.dragOffsetY = 0;
    }
    
    init() {
        this.desktop = document.getElementById('desktop');
        this.taskbarWindows = document.getElementById('taskbar-windows');
        this.setupGlobalEvents();
    }
    
    createWindow(title, contentHTML, width = 400, height = 300, options = {}) {
        const windowId = 'win_' + Date.now() + '_' + Math.floor(Math.random() * 1000);
        
        const win = document.createElement('div');
        win.className = 'window' + (options.className ? ' ' + options.className : '');
        win.id = windowId;
        win.style.width = width + 'px';
        win.style.height = height + 'px';
        win.style.left = (options.left || Math.random() * 300 + 50) + 'px';
        win.style.top = (options.top || Math.random() * 200 + 50) + 'px';
        win.style.zIndex = ++this.currentZ;
        
        // Use innerHTML to set window content
        win.innerHTML = this.buildWindowHTML(title, contentHTML, windowId);
        
        this.desktop.appendChild(win);
        this.windows.set(windowId, win);
        
        // Add to taskbar
        this.addToTaskbar(windowId, title);
        
        // Make interactive
        this.makeDraggable(win);
        this.makeResizable(win);
        this.setupWindowEvents(win);
        
        // Execute any scripts in the content
        this.executeWindowScripts(win);
        
        // Log to ledger
        if (typeof masterLedger !== 'undefined') {
            masterLedger.log('StateChange', `window "${title}" created`, 'window_manager');
        }
        
        return win;
    }
    
    buildWindowHTML(title, contentHTML, windowId) {
        return `
            <div class="title-bar" data-window-id="${windowId}">
                <span class="title-text">${title}</span>
                <div class="window-controls">
                    <button class="control-btn minimize-btn" onclick="windowManager.minimizeWindow('${windowId}')">−</button>
                    <button class="control-btn maximize-btn" onclick="windowManager.maximizeWindow('${windowId}')">□</button>
                    <button class="control-btn close-btn" onclick="windowManager.closeWindow('${windowId}')">×</button>
                </div>
            </div>
            <div class="content-area">
                ${contentHTML}
            </div>
            <div class="resize-handle"></div>
        `;
    }
    
    closeWindow(windowId) {
        const win = this.windows.get(windowId);
        if (win) {
            win.classList.add('closing');
            
            setTimeout(() => {
                win.remove();
                this.windows.delete(windowId);
                this.removeFromTaskbar(windowId);
                
                if (typeof masterLedger !== 'undefined') {
                    masterLedger.log('StateChange', `window ${windowId} closed`, 'window_manager');
                }
            }, 200);
        }
    }
    
    minimizeWindow(windowId) {
        const win = this.windows.get(windowId);
        if (win) {
            win.classList.add('minimized');
            win.style.display = 'none';
            
            // Update taskbar
            const taskbarBtn = document.querySelector(`.taskbar-window[data-window-id="${windowId}"]`);
            if (taskbarBtn) {
                taskbarBtn.classList.remove('active');
            }
            
            if (typeof masterLedger !== 'undefined') {
                masterLedger.log('StateChange', `window ${windowId} minimized`, 'window_manager');
            }
        }
    }
    
    maximizeWindow(windowId) {
        const win = this.windows.get(windowId);
        if (win) {
            if (win.classList.contains('maximized')) {
                // Restore
                win.classList.remove('maximized');
                win.style.width = win.dataset.prevWidth;
                win.style.height = win.dataset.prevHeight;
                win.style.left = win.dataset.prevLeft;
                win.style.top = win.dataset.prevTop;
            } else {
                // Maximize
                win.dataset.prevWidth = win.style.width;
                win.dataset.prevHeight = win.style.height;
                win.dataset.prevLeft = win.style.left;
                win.dataset.prevTop = win.style.top;
                
                win.classList.add('maximized');
                win.style.width = 'calc(100% - 0px)';
                win.style.height = 'calc(100vh - 85px)';
                win.style.left = '0';
                win.style.top = '40px';
            }
            
            if (typeof masterLedger !== 'undefined') {
                masterLedger.log('StateChange', `window ${windowId} maximized`, 'window_manager');
            }
        }
    }
    
    restoreWindow(windowId) {
        const win = this.windows.get(windowId);
        if (win) {
            win.style.display = 'flex';
            win.classList.remove('minimized');
            this.bringToFront(win);
            
            if (typeof masterLedger !== 'undefined') {
                masterLedger.log('StateChange', `window ${windowId} restored`, 'window_manager');
            }
        }
    }
    
    bringToFront(win) {
        win.style.zIndex = ++this.currentZ;
        
        // Update taskbar active state
        document.querySelectorAll('.taskbar-window').forEach(btn => {
            btn.classList.remove('active');
        });
        const taskbarBtn = document.querySelector(`.taskbar-window[data-window-id="${win.id}"]`);
        if (taskbarBtn) {
            taskbarBtn.classList.add('active');
        }
    }
    
    addToTaskbar(windowId, title) {
        const btn = document.createElement('button');
        btn.className = 'taskbar-window active';
        btn.dataset.windowId = windowId;
        btn.textContent = title.length > 20 ? title.substring(0, 17) + '...' : title;
        btn.onclick = () => {
            const win = this.windows.get(windowId);
            if (win.classList.contains('minimized')) {
                this.restoreWindow(windowId);
            } else if (win.style.zIndex < this.currentZ) {
                this.bringToFront(win);
            } else {
                this.minimizeWindow(windowId);
            }
        };
        
        // Update other taskbar buttons
        document.querySelectorAll('.taskbar-window').forEach(b => {
            b.classList.remove('active');
        });
        
        this.taskbarWindows.appendChild(btn);
    }
    
    removeFromTaskbar(windowId) {
        const btn = document.querySelector(`.taskbar-window[data-window-id="${windowId}"]`);
        if (btn) {
            btn.remove();
        }
    }
    
    makeDraggable(win) {
        const titleBar = win.querySelector('.title-bar');
        
        titleBar.addEventListener('mousedown', (e) => {
            if (e.target.classList.contains('control-btn')) return;
            
            this.draggingWindow = win;
            this.dragOffsetX = e.clientX - win.offsetLeft;
            this.dragOffsetY = e.clientY - win.offsetTop;
            
            this.bringToFront(win);
            win.classList.add('dragging');
        });
    }
    
    makeResizable(win) {
        const resizeHandle = win.querySelector('.resize-handle');
        
        resizeHandle.addEventListener('mousedown', (e) => {
            this.resizingWindow = win;
            e.preventDefault();
            this.bringToFront(win);
        });
    }
    
    setupWindowEvents(win) {
        win.addEventListener('mousedown', () => {
            this.bringToFront(win);
        });
    }
    
    setupGlobalEvents() {
        document.addEventListener('mousemove', (e) => {
            if (this.draggingWindow) {
                this.draggingWindow.style.left = (e.clientX - this.dragOffsetX) + 'px';
                this.draggingWindow.style.top = (e.clientY - this.dragOffsetY) + 'px';
            }
            
            if (this.resizingWindow) {
                const width = e.clientX - this.resizingWindow.offsetLeft;
                const height = e.clientY - this.resizingWindow.offsetTop;
                
                if (width > 200) this.resizingWindow.style.width = width + 'px';
                if (height > 150) this.resizingWindow.style.height = height + 'px';
            }
        });
        
        document.addEventListener('mouseup', () => {
            if (this.draggingWindow) {
                this.draggingWindow.classList.remove('dragging');
                if (typeof masterLedger !== 'undefined') {
                    masterLedger.log('EventFire', `window ${this.draggingWindow.id} dragged`, 'window_manager');
                }
                this.draggingWindow = null;
            }
            
            if (this.resizingWindow) {
                if (typeof masterLedger !== 'undefined') {
                    masterLedger.log('EventFire', `window ${this.resizingWindow.id} resized`, 'window_manager');
                }
                this.resizingWindow = null;
            }
        });
    }
    
    executeWindowScripts(win) {
        const scripts = win.querySelectorAll('script');
        scripts.forEach(script => {
            if (script.src) {
                // External script - already loaded
                return;
            } else {
                // Inline script - execute it
                try {
                    eval(script.textContent);
                } catch (e) {
                    console.error('Error executing window script:', e);
                }
            }
        });
    }
    
    closeAllWindows() {
        this.windows.forEach((win, id) => {
            this.closeWindow(id);
        });
    }
    
    getWindowCount() {
        return this.windows.size;
    }
}

// Global instance
const windowManager = new WindowManager();
