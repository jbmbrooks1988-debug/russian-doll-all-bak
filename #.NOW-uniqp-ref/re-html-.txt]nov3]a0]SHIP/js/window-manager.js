// window-manager.js - Core window management system

let windows = [];
let currentZ = 10;
let draggingWindow = null;
let resizingWindow = null;
let offsetX, offsetY;

// Create a new window with specified title and content
function createWindow(title, content, width = 400, height = 300) {
    const windowId = 'window_' + Date.now() + '_' + Math.floor(Math.random() * 1000);
    
    const win = document.createElement('div');
    win.className = 'window';
    win.id = windowId;
    win.style.width = width + 'px';
    win.style.height = height + 'px';
    win.style.left = (Math.random() * 300 + 50) + 'px';
    win.style.top = (Math.random() * 200 + 50) + 'px';
    
    win.innerHTML = `
        <div class="title-bar">
            <span class="title-text">${title}</span>
            <button class="close-btn" onclick="closeWindow('${windowId}')">×</button>
        </div>
        <div class="content-area">
            ${content}
        </div>
    `;
    
    document.getElementById('desktop').appendChild(win);
    makeWindowInteractive(win);
    windows.push(win);
    
    // Bring to front when clicked
    win.addEventListener('mousedown', function() {
        win.style.zIndex = ++currentZ;
    });
    
    return win;
}

// Close a window
function closeWindow(windowId) {
    const win = document.getElementById(windowId);
    if (win) {
        win.remove();
        windows = windows.filter(w => w.id !== windowId);
    }
}

// Make window draggable and resizable
function makeWindowInteractive(win) {
    const titleBar = win.querySelector('.title-bar');
    const contentArea = win.querySelector('.content-area');
    
    // Draggable
    titleBar.addEventListener('mousedown', startDrag);
    
    // Resizable - we'll make the entire window resizable
    win.addEventListener('mousedown', function(e) {
        if (e.target === win || e.target === contentArea) {
            // Only start resize if clicking near the edge
            const rect = win.getBoundingClientRect();
            const tolerance = 10; // pixels from edge
            
            if (e.clientX > rect.right - tolerance || 
                e.clientY > rect.bottom - tolerance) {
                startResize(e, win);
            }
        }
    });
}

// Drag functionality
function startDrag(e) {
    draggingWindow = e.currentTarget.parentElement;
    
    if (draggingWindow.classList.contains('window')) {
        const rect = draggingWindow.getBoundingClientRect();
        offsetX = e.clientX - rect.left;
        offsetY = e.clientY - rect.top;
        
        draggingWindow.classList.add('dragging');
        draggingWindow.style.zIndex = ++currentZ;
        
        document.addEventListener('mousemove', dragWindow);
        document.addEventListener('mouseup', stopDrag);
        
        e.preventDefault(); // Prevent text selection
    }
}

function dragWindow(e) {
    if (draggingWindow) {
        draggingWindow.style.left = (e.clientX - offsetX) + 'px';
        draggingWindow.style.top = (e.clientY - offsetY) + 'px';
    }
}

function stopDrag() {
    if (draggingWindow) {
        draggingWindow.classList.remove('dragging');
        document.removeEventListener('mousemove', dragWindow);
        document.removeEventListener('mouseup', stopDrag);
        draggingWindow = null;
    }
}

// Resize functionality
function startResize(e, win) {
    resizingWindow = win;
    const startX = e.clientX;
    const startY = e.clientY;
    const startWidth = parseInt(document.defaultView.getComputedStyle(resizingWindow).width, 10);
    const startHeight = parseInt(document.defaultView.getComputedStyle(resizingWindow).height, 10);
    
    resizingWindow.classList.add('resizing');
    
    function doResize(e) {
        const width = startWidth + (e.clientX - startX);
        const height = startHeight + (e.clientY - startY);
        
        if (width > 200) resizingWindow.style.width = width + 'px';
        if (height > 150) resizingWindow.style.height = height + 'px';
    }
    
    function stopResize() {
        document.removeEventListener('mousemove', doResize);
        document.removeEventListener('mouseup', stopResize);
        resizingWindow.classList.remove('resizing');
        resizingWindow = null;
    }
    
    document.addEventListener('mousemove', doResize);
    document.addEventListener('mouseup', stopResize);
    e.preventDefault();
}

// Create a simple terminal window
function createTerminal() {
    const terminalContent = `
        <div style="height: 100%; display: flex; flex-direction: column; background: #222; color: #00ff00; font-family: monospace; padding: 10px; overflow: auto;">
            <div style="margin-bottom: 10px;">> Welcome to HTML Terminal! Type 'help' for commands.</div>
            <div>> Current user: Guest</div>
            <div style="flex: 1; overflow-y: auto;" id="terminal-output"></div>
            <div style="display: flex; align-items: center; margin-top: 10px;">
                <span style="color: #00ff00; margin-right: 5px;">$</span>
                <input type="text" id="terminal-input" style="flex: 1; background: #222; color: #00ff00; border: 1px solid #00ff00; padding: 3px; outline: none;" placeholder="Type command...">
            </div>
        </div>
    `;
    
    const terminalWindow = createWindow('Terminal', terminalContent, 600, 400);
    
    // Add terminal functionality
    setTimeout(() => {
        const input = terminalWindow.querySelector('#terminal-input');
        if (input) {
            input.focus();
            
            input.addEventListener('keypress', function(e) {
                if (e.key === 'Enter') {
                    const output = terminalWindow.querySelector('#terminal-output');
                    if (output) {
                        output.innerHTML += `<div>$ ${input.value}</div>`;
                        
                        // Process command
                        const cmd = input.value.trim().toLowerCase();
                        if (cmd === 'help') {
                            output.innerHTML += '<div>Available commands: help, clear, date, echo [text]</div>';
                        } else if (cmd === 'clear') {
                            output.innerHTML = '';
                        } else if (cmd === 'date') {
                            output.innerHTML += `<div>${new Date().toString()}</div>`;
                        } else if (cmd.startsWith('echo ')) {
                            output.innerHTML += `<div>${cmd.substring(5)}</div>`;
                        } else if (cmd !== '') {
                            output.innerHTML += `<div>Command not found: ${cmd}</div>`;
                        }
                    }
                    input.value = '';
                    output.scrollTop = output.scrollHeight; // Auto-scroll to bottom
                }
            });
        }
    }, 100);
}