// recursive-desktop.js - Recursive desktop functionality

function createRecursiveDesktopWindow() {
    const recursiveContent = `
        <div style="padding: 15px; height: 100%; display: flex; flex-direction: column;">
            <h3>Recursive Desktop</h3>
            <p>Welcome to the recursive desktop environment!</p>
            <p>Current recursion level: <span id="recursion-level">1</span></p>
            <p>Current time: ${new Date().toLocaleString()}</p>
            
            <div style="border: 1px solid #ccc; padding: 10px; margin: 10px 0; background: #f5f5f5;">
                <h4>Recursion Controls</h4>
                <button onclick="createRecursiveDesktopWindow()">Create Nested Desktop</button>
                <button onclick="createTerminal()">Open Terminal</button>
                <button onclick="openSampleApp()">Open Sample App</button>
                <button onclick="createMoreWindows()" style="margin-top: 5px;">Create Multiple Windows</button>
            </div>
            
            <div style="border: 1px solid #ccc; padding: 10px; margin: 10px 0; background: #eef5ee;">
                <h4>System Info</h4>
                <p>Total windows open: <span id="window-count">${windows.length}</span></p>
                <p>Current Z-index: <span id="z-index-value">${currentZ}</span></p>
                <p>JavaScript execution context: Shared</p>
            </div>
            
            <div style="flex: 1; overflow-y: auto; margin: 10px 0;">
                <h4>Features</h4>
                <ul>
                    <li>Self-replicating desktop windows</li>
                    <li>Shared JavaScript context</li>
                    <li>Dynamic window creation</li>
                    <li>Drag and resize functionality</li>
                    <li>Context menu support</li>
                    <li>Toolbar with application launcher</li>
                </ul>
            </div>
            
            <div style="text-align: center; padding: 10px; color: #666; font-size: 0.9em;">
                <p>This desktop can open itself, creating infinite recursion!</p>
                <p>Maximize your browser to see all windows.</p>
            </div>
        </div>
    `;
    
    const window = createWindow('Recursive Desktop', recursiveContent, 600, 500);
    
    // Update window count display
    const windowCountInterval = setInterval(() => {
        const countDisplay = window.querySelector('#window-count');
        if (countDisplay) {
            countDisplay.textContent = windows.length;
        }
        
        const zIndexDisplay = window.querySelector('#z-index-value');
        if (zIndexDisplay) {
            zIndexDisplay.textContent = currentZ;
        }
    }, 1000);
    
    // Clean up interval when window is closed
    const originalClose = window.querySelector('.close-btn').onclick;
    window.querySelector('.close-btn').onclick = function() {
        clearInterval(windowCountInterval);
        if (originalClose) originalClose();
        closeWindow(window.id);
    };
    
    return window;
}

// Function to create multiple windows at once
function createMoreWindows() {
    // Create multiple sample apps
    for (let i = 0; i < 2; i++) {
        setTimeout(() => {
            createSampleAppWindow();
        }, i * 300); // Stagger creation
    }
    
    // Create a terminal window
    setTimeout(() => {
        createTerminal();
    }, 600);
}

// Override the openDesktopWindow function to use the recursive version
function openDesktopWindow() {
    createRecursiveDesktopWindow();
}