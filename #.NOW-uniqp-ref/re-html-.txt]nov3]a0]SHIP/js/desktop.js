// desktop.js - Main desktop functionality

// Initialize the desktop environment
document.addEventListener('DOMContentLoaded', function() {
    setupToolbar();
    setupContextMenu();
    setupEventListeners();
});

// Set up toolbar buttons
function setupToolbar() {
    const fileBtn = document.getElementById('fileBtn');
    const fileDropdown = document.getElementById('fileDropdown');
    
    // File button to toggle dropdown
    fileBtn.addEventListener('click', function(e) {
        e.stopPropagation();
        if (fileDropdown.style.display === 'none') {
            fileDropdown.style.display = 'flex';
        } else {
            fileDropdown.style.display = 'none';
        }
    });
    
    // Hide dropdown when clicking elsewhere
    document.addEventListener('click', function() {
        fileDropdown.style.display = 'none';
    });
    
    // File dropdown buttons
    document.getElementById('openDesktop').addEventListener('click', openDesktopWindow);
    document.getElementById('openSample').addEventListener('click', openSampleApp);
    
    // Other toolbar buttons
    document.getElementById('terminalBtn').addEventListener('click', createTerminal);
    document.getElementById('craftBtn').addEventListener('click', openCraftApp);
    document.getElementById('customizeBtn').addEventListener('click', openCustomizePanel);
}

// Set up right-click context menu
function setupContextMenu() {
    document.getElementById('desktop').addEventListener('contextmenu', function(e) {
        e.preventDefault();
        
        const contextMenu = document.getElementById('context-menu');
        contextMenu.style.display = 'block';
        contextMenu.style.left = e.clientX + 'px';
        contextMenu.style.top = e.clientY + 'px';
        
        // Hide context menu when clicking elsewhere
        function hideMenu() {
            contextMenu.style.display = 'none';
            document.removeEventListener('click', hideMenu);
        }
        
        setTimeout(() => {
            document.addEventListener('click', hideMenu);
        }, 100);
    });
}

// Set up general event listeners
function setupEventListeners() {
    // Close all dropdowns when clicking anywhere
    document.addEventListener('click', function(e) {
        if (!e.target.closest('#fileBtn') && !e.target.closest('#fileDropdown')) {
            document.getElementById('fileDropdown').style.display = 'none';
        }
    });
    
    // Allow clicking on desktop to focus (bring to front) any window
    document.getElementById('desktop').addEventListener('click', function(e) {
        if (e.target.id === 'desktop') {
            // Clicked on empty desktop area
            // Bring all windows to a reasonable z-index if needed
        }
    });
}

// Open the desktop inside itself (recursion)
function openDesktopWindow() {
    const desktopContent = `
        <div style="padding: 10px;">
            <h3>Nested Desktop Environment</h3>
            <p>You've opened the desktop inside itself!</p>
            <p>This demonstrates the recursive capability of the HTML desktop.</p>
            <p>Current time: ${new Date().toLocaleString()}</p>
            <button onclick="createTerminal()">New Terminal</button>
            <button onclick="openSampleApp()">Open Sample App</button>
            <button onclick="openDesktopWindow()">Open Another Desktop</button>
        </div>
    `;
    
    createWindow('Nested Desktop', desktopContent, 500, 400);
}

// Open a sample application
function openSampleApp() {
    // This will be handled by the sample-app.js file
    if (typeof createSampleAppWindow === 'function') {
        createSampleAppWindow();
    } else {
        console.log('Sample app module not loaded');
    }
}

// Open a simple craft application
function openCraftApp() {
    // This will be handled by a craft app module
    if (typeof createCraftWindow === 'function') {
        createCraftWindow();
    } else {
        // Fallback: create a basic craft-like window
        const craftContent = `
            <div style="padding: 10px;">
                <h3>Craft Application</h3>
                <p>This is a placeholder for the Craft 3D application.</p>
                <p>Current time: ${new Date().toLocaleString()}</p>
                <div style="border: 1px solid #ccc; padding: 10px; margin-top: 10px;">
                    <p>3D Canvas Area (Placeholder)</p>
                    <div style="background: #eee; height: 200px; display: flex; align-items: center; justify-content: center;">
                        <i>3D rendering would go here</i>
                    </div>
                </div>
                <div style="margin-top: 10px;">
                    <button onclick="alert('New world created!')">New</button>
                    <button onclick="alert('World saved!')">Save</button>
                    <button onclick="alert('World loaded!')">Load</button>
                </div>
            </div>
        `;
        
        createWindow('Craft - 3D Builder', craftContent, 700, 500);
    }
}

// Open customize panel
function openCustomizePanel() {
    const customizeContent = `
        <div style="padding: 15px;">
            <h3>Customization Panel</h3>
            <p>Customize your desktop environment</p>
            
            <div style="margin: 10px 0;">
                <label>Background Color:</label>
                <input type="color" id="bgColorPicker" value="#1a2a6c" style="margin-left: 10px;">
            </div>
            
            <div style="margin: 10px 0;">
                <label>Accent Color:</label>
                <input type="color" id="accentColorPicker" value="#b21f1f" style="margin-left: 15px;">
            </div>
            
            <div style="margin: 10px 0;">
                <label>Font Size:</label>
                <input type="range" id="fontSizeSlider" min="10" max="24" value="14" style="margin-left: 20px;">
                <span id="fontSizeValue">14px</span>
            </div>
            
            <button onclick="applyCustomizations()" style="margin-top: 15px;">Apply Changes</button>
            <button onclick="resetCustomizations()" style="margin-top: 15px; margin-left: 10px;">Reset</button>
        </div>
    `;
    
    createWindow('Customize Desktop', customizeContent, 400, 300);
    
    // Add event listeners for the color pickers and slider
    setTimeout(() => {
        const fontSizeSlider = document.querySelector(`#${getActiveWindowId()} #fontSizeSlider`);
        const fontSizeValue = document.querySelector(`#${getActiveWindowId()} #fontSizeValue`);
        
        if (fontSizeSlider && fontSizeValue) {
            fontSizeSlider.addEventListener('input', function() {
                fontSizeValue.textContent = this.value + 'px';
            });
        }
    }, 100);
}

// Helper function to get the currently focused window ID
function getActiveWindowId() {
    // Return the ID of the window with the highest z-index
    let topWindow = null;
    let highestZ = 0;
    
    for (const win of windows) {
        const z = parseInt(win.style.zIndex) || 0;
        if (z > highestZ) {
            highestZ = z;
            topWindow = win;
        }
    }
    
    return topWindow ? topWindow.id : null;
}

// Apply customizations to the desktop
function applyCustomizations() {
    const bgColorPicker = document.getElementById('bgColorPicker');
    const accentColorPicker = document.getElementById('accentColorPicker');
    const fontSizeSlider = document.getElementById('fontSizeSlider');
    
    if (bgColorPicker) {
        document.body.style.background = `linear-gradient(135deg, ${bgColorPicker.value}, ${accentColorPicker.value}, ${bgColorPicker.value})`;
    }
    
    if (fontSizeSlider) {
        document.body.style.fontSize = fontSizeSlider.value + 'px';
    }
    
    alert('Customizations applied!');
}

// Reset to default settings
function resetCustomizations() {
    document.body.style.background = 'linear-gradient(135deg, #1a2a6c, #b21f1f, #1a2a6c)';
    document.body.style.fontSize = '';
    alert('Settings reset to defaults!');
}