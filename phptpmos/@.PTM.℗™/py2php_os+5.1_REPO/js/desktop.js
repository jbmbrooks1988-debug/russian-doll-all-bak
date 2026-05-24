// desktop.js - Main Desktop Initialization and Event Handling

// Initialize the desktop environment when DOM is ready
document.addEventListener('DOMContentLoaded', async function() {
    console.log('PyOS-TPM Web Edition - Initializing...');
    
    try {
        // Initialize core systems
        await masterLedger.init();
        await pieceManager.loadAllPieces();
        windowManager.init();
        
        // Setup UI components
        setupToolbar();
        setupContextMenu();
        setupClock();
        setupEventListeners();
        
        // Log startup
        masterLedger.log('StateChange', 'Desktop initialized', 'desktop');
        
        // Show welcome message
        showWelcomeScreen();
        
        console.log('PyOS-TPM Web Edition - Ready!');
    } catch (error) {
        console.error('Error during initialization:', error);
    }
});

// Setup toolbar buttons and dropdowns
function setupToolbar() {
    const fileBtn = document.getElementById('fileBtn');
    const appsBtn = document.getElementById('appsBtn');
    const terminalBtn = document.getElementById('terminalBtn');
    const fileDropdown = document.getElementById('fileDropdown');
    const appsDropdown = document.getElementById('appsDropdown');
    
    // File button toggle
    fileBtn.addEventListener('click', function(e) {
        e.stopPropagation();
        toggleDropdown(fileDropdown, appsDropdown);
    });
    
    // Apps button toggle
    appsBtn.addEventListener('click', function(e) {
        e.stopPropagation();
        toggleDropdown(appsDropdown, fileDropdown);
    });
    
    // Terminal button
    terminalBtn.addEventListener('click', function() {
        createTerminal();
    });
    
    // File dropdown actions
    document.getElementById('newWindowBtn').addEventListener('click', function() {
        createDemoWindow();
        hideAllDropdowns();
    });
    
    document.getElementById('openTerminalBtn').addEventListener('click', function() {
        createTerminal();
        hideAllDropdowns();
    });
    
    document.getElementById('settingsBtn').addEventListener('click', function() {
        createSettingsApp();
        hideAllDropdowns();
    });
    
    // Apps dropdown actions
    appsDropdown.querySelectorAll('button[data-app]').forEach(btn => {
        btn.addEventListener('click', function() {
            const appName = this.dataset.app;
            openApp(appName);
            hideAllDropdowns();
        });
    });
    
    // Hide dropdowns when clicking elsewhere
    document.addEventListener('click', function() {
        hideAllDropdowns();
    });
}

// Toggle between two dropdowns
function toggleDropdown(showDropdown, hideDropdown) {
    if (showDropdown.style.display === 'block') {
        showDropdown.style.display = 'none';
    } else {
        hideDropdown.style.display = 'none';
        showDropdown.style.display = 'block';
    }
}

// Hide all dropdowns
function hideAllDropdowns() {
    document.getElementById('fileDropdown').style.display = 'none';
    document.getElementById('appsDropdown').style.display = 'none';
}

// Setup right-click context menu
function setupContextMenu() {
    const contextMenu = document.getElementById('context-menu');
    
    // Add context menu listener to the whole document
    document.addEventListener('contextmenu', function(e) {
        // Don't show context menu if clicking on a window, toolbar, or taskbar
        if (e.target.closest('.window') || 
            e.target.closest('#toolbar') || 
            e.target.closest('#taskbar') ||
            e.target.closest('#fileDropdown') ||
            e.target.closest('#appsDropdown')) {
            return;
        }
        
        e.preventDefault();
        
        // Position context menu
        const x = e.clientX;
        const y = e.clientY;
        
        // Ensure menu stays within viewport
        const menuWidth = 200;
        const menuHeight = 280;
        const maxX = window.innerWidth - menuWidth;
        const maxY = window.innerHeight - menuHeight;
        
        contextMenu.style.display = 'block';
        contextMenu.style.left = Math.min(x, maxX) + 'px';
        contextMenu.style.top = Math.min(y, maxY) + 'px';
        
        // Update context menu piece state (TPM pattern)
        if (typeof pieceManager !== 'undefined') {
            pieceManager.updatePieceState('context_menu', {
                visible: true,
                position_x: x,
                position_y: y
            });
        }
        
        // Log event
        if (typeof pieceManager !== 'undefined') {
            pieceManager.fireEvent('context_menu', 'context_menu_shown', { x, y });
        }
    });
    
    // Handle context menu item clicks
    contextMenu.querySelectorAll('.context-item').forEach(item => {
        item.addEventListener('click', function() {
            const action = this.dataset.action;
            handleContextAction(action);
            contextMenu.style.display = 'none';
            
            // Update context menu piece state
            if (typeof pieceManager !== 'undefined') {
                pieceManager.updatePieceState('context_menu', {
                    visible: false,
                    selected_option: action
                });
                pieceManager.fireEvent('context_menu', 'option_selected', { action });
            }
        });
    });
    
    // Hide context menu when clicking elsewhere
    document.addEventListener('click', function(e) {
        if (!e.target.closest('#context-menu')) {
            contextMenu.style.display = 'none';
            
            // Update context menu piece state
            if (typeof pieceManager !== 'undefined') {
                pieceManager.updatePieceState('context_menu', {
                    visible: false
                });
                pieceManager.fireEvent('context_menu', 'context_menu_hidden', {});
            }
        }
    });
}

// Handle context menu actions
function handleContextAction(action) {
    switch(action) {
        case 'open_terminal':
            createTerminal();
            break;
        case 'show_apps':
            document.getElementById('appsBtn').click();
            break;
        case 'counter':
            createDemoCounterApp();
            break;
        case 'fuzzpet':
            createFuzzPetApp();
            break;
        case 'hangman':
            openApp('hangman');
            break;
        case 'pyboard2d':
            openApp('pyboard2d');
            break;
        case 'pyboard3d':
            openApp('pyboard3d');
            break;
        case 'settings':
            createSettingsApp();
            break;
        case 'refresh':
            location.reload();
            break;
    }
}

// Setup clock
function setupClock() {
    const clockEl = document.getElementById('clock');
    
    function updateClock() {
        const now = new Date();
        const hours = String(now.getHours()).padStart(2, '0');
        const minutes = String(now.getMinutes()).padStart(2, '0');
        const seconds = String(now.getSeconds()).padStart(2, '0');
        clockEl.textContent = `${hours}:${minutes}:${seconds}`;
    }
    
    updateClock();
    setInterval(updateClock, 1000);
}

// Setup general event listeners
function setupEventListeners() {
    // Window resize handler
    window.addEventListener('resize', function() {
        pieceManager.fireEvent('desktop', 'desktop_resized', {
            width: window.innerWidth,
            height: window.innerHeight
        });
    });
    
    // Keyboard shortcuts
    document.addEventListener('keydown', function(e) {
        // Ctrl+T or Cmd+T - Open terminal
        if ((e.ctrlKey || e.metaKey) && e.key === 't') {
            e.preventDefault();
            createTerminal();
        }
        
        // Ctrl+W or Cmd+W - Close active window
        if ((e.ctrlKey || e.metaKey) && e.key === 'w') {
            e.preventDefault();
            const activeWindow = document.querySelector('.window:last-of-type');
            if (activeWindow) {
                windowManager.closeWindow(activeWindow.id);
            }
        }
        
        // Escape - Hide all dropdowns and context menu
        if (e.key === 'Escape') {
            hideAllDropdowns();
            document.getElementById('context-menu').style.display = 'none';
        }
    });
}

// Show welcome screen on first load
function showWelcomeScreen() {
    const welcomeHTML = `
        <div style="padding: 20px; text-align: center;">
            <h2 style="color: #3498db; margin-bottom: 15px;">🖥️ Welcome to PyOS-TPM Web!</h2>
            <p style="color: #666; margin-bottom: 20px;">
                A True Piece Method desktop environment built with HTML, CSS, and JavaScript.
            </p>
            <div style="text-align: left; background: #f5f6fa; padding: 15px; border-radius: 8px; margin-bottom: 20px;">
                <strong style="color: #2c3e50;">Quick Start:</strong>
                <ul style="margin: 10px 0; padding-left: 20px; color: #555;">
                    <li>Right-click on desktop for context menu</li>
                    <li>Click "Terminal" to open a command terminal</li>
                    <li>Click "Apps" to browse applications</li>
                    <li>Drag windows by their title bar</li>
                    <li>Resize windows from the bottom-right corner</li>
                </ul>
            </div>
            <button onclick="this.closest('.window').remove()" style="background: #3498db; color: white; border: none; padding: 10px 25px; border-radius: 5px; cursor: pointer; font-size: 14px;">
                Get Started!
            </button>
        </div>
    `;
    
    setTimeout(() => {
        windowManager.createWindow('Welcome', welcomeHTML, 450, 350);
    }, 500);
}

// Create a demo window
function createDemoWindow() {
    const demoContent = `
        <div style="padding: 20px;">
            <h3 style="color: #2c3e50; margin-bottom: 10px;">Demo Window</h3>
            <p style="color: #666; margin-bottom: 15px;">
                This is a demonstration of the window management system.
            </p>
            <p style="color: #888; font-size: 13px;">
                Created at: ${new Date().toLocaleString()}
            </p>
            <button onclick="alert('Hello from PyOS-TPM!')" style="margin-top: 15px; padding: 8px 16px; background: #3498db; color: white; border: none; border-radius: 4px; cursor: pointer;">
                Click Me
            </button>
        </div>
    `;
    
    windowManager.createWindow('Demo Window', demoContent, 350, 250);
}

// Open application by name
function openApp(appName) {
    const appFunctions = {
        'terminal': createTerminal,
        'counter': createDemoCounterApp,
        'fuzzpet': createFuzzPetApp,
        'hangman': createHangmanApp,
        'pyboard2d': createPyBoard2D,
        'pyboard3d': createPyBoard3D,
        'cube': createCubeApp,
        'settings': createSettingsApp,
        'store': createStoreApp,
        'wallet': createWalletApp
    };
    
    const fn = appFunctions[appName];
    if (fn && typeof fn === 'function') {
        fn();
        masterLedger.log('EventFire', `App launched: ${appName}`, 'desktop');
    } else {
        alert(`App "${appName}" is not yet implemented.`);
    }
}

// Create Terminal Application
function createTerminal() {
    const terminalContent = `
        <div class="terminal-container">
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 5px;">
                <span style="color: #00ffff; font-size: 11px;">PyOS-TPM Terminal v1.0</span>
                <button id="term-copy-btn" style="
                    padding: 3px 8px;
                    font-size: 11px;
                    background: #3498db;
                    color: white;
                    border: none;
                    border-radius: 3px;
                    cursor: pointer;
                " title="Copy terminal output to clipboard">📋 Copy</button>
            </div>
            <div class="terminal-output" id="terminal-output">
                <div class="terminal-line">PyOS-TPM Terminal v1.0</div>
                <div class="terminal-line">Type 'help' for available commands.</div>
                <div class="terminal-line">Select text with mouse, then click Copy or use Ctrl+C.</div>
                <div class="terminal-line"></div>
            </div>
            <div class="terminal-input-line">
                <span class="prompt">$</span>
                <input type="text" class="terminal-input" id="terminal-input">
            </div>
        </div>
    `;
    
    const win = windowManager.createWindow('Terminal', terminalContent, 650, 450, {
        className: 'terminal-window'
    });
    
    // Initialize terminal after window is added to DOM
    setTimeout(() => {
        initTerminalWindow(win);
    }, 50);
    
    pieceManager.fireEvent('terminal', 'terminal_opened');
}

// Initialize terminal functionality in a window
function initTerminalWindow(win) {
    const output = win.querySelector('#terminal-output');
    const input = win.querySelector('#terminal-input');
    const copyBtn = win.querySelector('#term-copy-btn');
    
    if (!output || !input) {
        console.error('Terminal elements not found');
        return;
    }
    
    const history = [];
    let historyIndex = -1;
    
    // Focus the input
    input.focus();
    
    // Keep focus on input when clicking in terminal (but not on output text)
    win.querySelector('.content-area').addEventListener('click', (e) => {
        if (e.target !== output && !output.contains(e.target)) {
            input.focus();
        }
    });
    
    // Copy button functionality
    if (copyBtn) {
        copyBtn.addEventListener('click', () => {
            const selectedText = window.getSelection().toString();
            const textToCopy = selectedText || output.innerText;
            
            navigator.clipboard.writeText(textToCopy).then(() => {
                const originalText = copyBtn.textContent;
                copyBtn.textContent = '✓ Copied!';
                copyBtn.style.background = '#27ae60';
                setTimeout(() => {
                    copyBtn.textContent = originalText;
                    copyBtn.style.background = '#3498db';
                }, 2000);
            }).catch(err => {
                // Fallback for older browsers
                const textarea = document.createElement('textarea');
                textarea.value = textToCopy;
                document.body.appendChild(textarea);
                textarea.select();
                document.execCommand('copy');
                document.body.removeChild(textarea);
                
                const originalText = copyBtn.textContent;
                copyBtn.textContent = '✓ Copied!';
                copyBtn.style.background = '#27ae60';
                setTimeout(() => {
                    copyBtn.textContent = originalText;
                    copyBtn.style.background = '#3498db';
                }, 2000);
            });
        });
    }
    
    function escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    function processCommand(cmd) {
        const args = cmd.split(' ').filter(a => a);
        const command = args[0].toLowerCase();
        
        // Direct app launch commands
        const directApps = ['counter', 'hangman', 'pyboard2d', 'pyboard3d', 'cube', 'settings', 'store', 'wallet', 'fuzzpet'];
        if (directApps.includes(command) && window.openApp) {
            window.openApp(command);
            return;
        }
        
        switch(command) {
            case 'help':
                output.innerHTML += '<div class="terminal-response">Available commands: help, clear, date, echo, ls, pwd, whoami, apps, open [app], version</div>';
                output.innerHTML += '<div class="terminal-response">Quick launch: counter, fuzzpet</div>';
                output.innerHTML += '<div class="terminal-response">Game controls: save, load (FuzzPet)</div>';
                break;
            case 'clear':
                output.innerHTML = '<div class="terminal-line">Terminal cleared.</div>';
                return;
            case 'date':
                output.innerHTML += '<div class="terminal-response">' + new Date().toString() + '</div>';
                break;
            case 'echo':
                output.innerHTML += '<div class="terminal-response">' + escapeHtml(args.slice(1).join(' ')) + '</div>';
                break;
            case 'ls':
                output.innerHTML += '<div class="terminal-response">desktop  terminal  keyboard  pieces  plugins  data  saves</div>';
                break;
            case 'pwd':
                output.innerHTML += '<div class="terminal-response">/pyos</div>';
                break;
            case 'whoami':
                output.innerHTML += '<div class="terminal-response">guest</div>';
                break;
            case 'apps':
                output.innerHTML += '<div class="terminal-response">counter  hangman  pyboard2d  pyboard3d  cube  settings  store  wallet  fuzzpet</div>';
                break;
            case 'open':
                if (args[1]) {
                    output.innerHTML += '<div class="terminal-response">Opening ' + args[1] + '...</div>';
                    if (window.openApp) window.openApp(args[1]);
                } else {
                    output.innerHTML += '<div class="terminal-error">Usage: open [app_name]</div>';
                }
                break;
            case 'save':
                if (typeof fuzzpetGame !== 'undefined' && fuzzpetGame.window) {
                    fuzzpetGame.saveGame();
                    output.innerHTML += '<div class="terminal-response">Saving FuzzPet game...</div>';
                } else {
                    output.innerHTML += '<div class="terminal-response">Open FuzzPet first, then use save command</div>';
                }
                break;
            case 'load':
                if (typeof fuzzpetGame !== 'undefined' && fuzzpetGame.window) {
                    fuzzpetGame.loadGame();
                    output.innerHTML += '<div class="terminal-response">Loading FuzzPet game...</div>';
                } else {
                    output.innerHTML += '<div class="terminal-response">Open FuzzPet first, then use load command</div>';
                }
                break;
            case 'version':
                output.innerHTML += '<div class="terminal-response">PyOS-TPM Web v1.0.0</div>';
                break;
            case '':
                break;
            default:
                output.innerHTML += '<div class="terminal-error">Command not found: ' + escapeHtml(command) + '. Type "help" for commands.</div>';
        }
        output.scrollTop = output.scrollHeight;
    }
    
    input.addEventListener('keypress', function(e) {
        if (e.key === 'Enter') {
            const command = input.value.trim();
            output.innerHTML += '<div class="terminal-line"><span class="prompt">$</span> ' + escapeHtml(command) + '</div>';
            
            if (command) {
                history.push(command);
                historyIndex = history.length;
                processCommand(command);
            }
            
            input.value = '';
            input.focus();
        }
    });
    
    input.addEventListener('keydown', function(e) {
        if (e.key === 'ArrowUp' && historyIndex > 0) {
            historyIndex--;
            input.value = history[historyIndex];
        } else if (e.key === 'ArrowDown' && historyIndex < history.length - 1) {
            historyIndex++;
            input.value = history[historyIndex];
        }
    });
    
    // Initial focus
    input.focus();
}

// Placeholder apps - to be implemented in Phase 2
function createHangmanApp() {
    const content = `
        <div style="padding: 20px; text-align: center;">
            <h2 style="color: #2c3e50;">Hangman</h2>
            <p style="color: #666; margin: 20px 0;">Coming soon...</p>
            <div style="font-size: 48px; margin: 20px 0;">🎮</div>
        </div>
    `;
    windowManager.createWindow('Hangman', content, 400, 300, { className: 'hangman-window' });
}

function createPyBoard2D() {
    const content = `
        <div style="padding: 20px; text-align: center;">
            <h2 style="color: #2c3e50;">2D PyBoard</h2>
            <p style="color: #666; margin: 20px 0;">2D voxel viewer - Coming soon...</p>
            <div style="font-size: 48px; margin: 20px 0;">🗺️</div>
        </div>
    `;
    windowManager.createWindow('2D PyBoard', content, 600, 450, { className: 'pyboard2d-window' });
}

function createPyBoard3D() {
    const content = `
        <div style="padding: 20px; text-align: center;">
            <h2 style="color: #2c3e50;">3D PyBoard</h2>
            <p style="color: #666; margin: 20px 0;">3D voxel viewer - Coming soon...</p>
            <div style="font-size: 48px; margin: 20px 0;">🌍</div>
        </div>
    `;
    windowManager.createWindow('3D PyBoard', content, 700, 550, { className: 'pyboard3d-window' });
}

function createCubeApp() {
    const content = `
        <div style="padding: 20px; text-align: center;">
            <h2 style="color: #2c3e50;">3D Cube</h2>
            <p style="color: #666; margin: 20px 0;">OpenGL cube visualization - Coming soon...</p>
            <div style="font-size: 48px; margin: 20px 0;">🎲</div>
        </div>
    `;
    windowManager.createWindow('3D Cube', content, 500, 400, { className: 'cube-window' });
}

function createSettingsApp() {
    const content = `
        <div style="padding: 20px;">
            <h2 style="color: #2c3e50; margin-bottom: 20px;">Settings</h2>
            
            <div style="margin-bottom: 20px;">
                <label style="display: block; color: #555; margin-bottom: 8px;">Desktop Background</label>
                <select style="width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px;">
                    <option>Default Gradient</option>
                    <option>Solid Color</option>
                    <option>Custom Image</option>
                </select>
            </div>
            
            <div style="margin-bottom: 20px;">
                <label style="display: block; color: #555; margin-bottom: 8px;">Font Size</label>
                <input type="range" min="10" max="24" value="14" style="width: 100%;">
            </div>
            
            <div style="margin-bottom: 20px;">
                <label style="display: flex; align-items: center; gap: 10px; color: #555;">
                    <input type="checkbox" checked> Enable animations
                </label>
            </div>
            
            <div style="margin-bottom: 20px;">
                <label style="display: flex; align-items: center; gap: 10px; color: #555;">
                    <input type="checkbox" checked> Show taskbar
                </label>
            </div>
            
            <button onclick="alert('Settings saved!')" style="background: #3498db; color: white; border: none; padding: 10px 20px; border-radius: 4px; cursor: pointer; width: 100%;">
                Save Settings
            </button>
        </div>
    `;
    windowManager.createWindow('Settings', content, 350, 400, { className: 'settings-window' });
}

function createStoreApp() {
    const content = `
        <div style="padding: 20px; text-align: center;">
            <h2 style="color: #2c3e50;">App Store</h2>
            <p style="color: #666; margin: 20px 0;">Browse and install apps - Coming soon...</p>
            <div style="font-size: 48px; margin: 20px 0;">🏪</div>
        </div>
    `;
    windowManager.createWindow('Store', content, 500, 400);
}

function createWalletApp() {
    const content = `
        <div style="padding: 20px; text-align: center;">
            <h2 style="color: #2c3e50;">Wallet</h2>
            <p style="color: #666; margin: 20px 0;">Manage your credentials - Coming soon...</p>
            <div style="font-size: 48px; margin: 20px 0;">👛</div>
        </div>
    `;
    windowManager.createWindow('Wallet', content, 400, 300);
}
