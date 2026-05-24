const terminals = [];
const maxTerminals = 10;
let currentZ = 10;

// Global variable to store current user
let currentUser = null;

// Import ProcessManager if not already available
if (typeof window.processManager === 'undefined') {
    // ProcessManager will be initialized in the HTML or before this script
    console.warn('ProcessManager not found. Make sure processManager.js is loaded before script.js');
}

// User authentication functions
function login(username, password) {
    // In a real implementation, you would verify credentials
    // For now, we'll just accept any non-empty username
    if (username && username.trim() !== '') {
        currentUser = username.trim();
        updateUserDisplay();
        return `Successfully logged in as ${currentUser}`;
    } else {
        return 'Username cannot be empty. Usage: login [username] [password]';
    }
}

function logout() {
    const prevUser = currentUser;
    currentUser = null;
    updateUserDisplay();
    return `Successfully logged out from ${prevUser}`;
}

function getCurrentUser() {
    return currentUser;
}

function updateUserDisplay() {
    const userDisplay = document.getElementById('user-display');
    if (userDisplay) {
        if (currentUser) {
            userDisplay.textContent = currentUser;
        } else {
            userDisplay.textContent = 'Guest';
        }
    }
}

function createTerminal() {
    if (terminals.length >= maxTerminals) {
        alert('Maximum of 10 terminals reached.');
        return;
    }

    const win = document.createElement('div');
    win.className = 'app-window terminal-window';
    
    // Calculate position to ensure it's within the visible area (not behind toolbar)
    const toolbarHeight = 40; // height of the toolbar at the bottom
    const minimizedTabHeight = 40; // height of the minimized tab bar
    const totalBottomOffset = toolbarHeight + minimizedTabHeight;
    
    // Position it randomly but within the visible area
    const maxLeft = Math.max(0, window.innerWidth - 400 - 50); // 400 is default width, 50 is margin
    const maxTop = Math.max(0, window.innerHeight - totalBottomOffset - 300 - 50); // 300 is default height, 50 is margin
    
    const leftPos = Math.min(Math.random() * 300 + 50, maxLeft);
    const topPos = Math.min(Math.random() * 300 + 50, maxTop);
    
    win.style.left = leftPos + 'px';
    win.style.top = topPos + 'px';
    win.style.zIndex = currentZ++;
    win.innerHTML = `
        <div class="title-bar">
            Terminal
            <div class="window-buttons">
                <button class="minimize-btn" onclick="minimizeTerminal(this)">-</button>
                <button class="fullscreen-btn" onclick="toggleTerminalFullscreen(this)">□</button>
                <button class="close-btn" onclick="closeTerminal(this)">×</button>
            </div>
        </div>
        <div class="terminal-content">
            <div class="output">
                <div class="terminal-prompt"><span class="prompt-symbol">></span> Welcome to JS CLI App! Type 'help' for available commands.</div>
                <div class="terminal-prompt"><span class="prompt-symbol">></span> Current user: ${currentUser || 'Guest'}</div>
            </div>
            <div class="input-area">
                <span class="prompt-symbol">$</span>
                <span id="cmd-input-span" contenteditable="true" class="cmd-input-span" tabindex="0"></span>
                <span id="cursor">|</span>
            </div>
        </div>
    `;
    document.body.appendChild(win);
    makeDraggable(win);
    addFocusHandler(win);
    setupTerminalInput(win);
    terminals.push(win);
    
    // Ensure the terminal is visible (scroll if necessary)
    win.scrollIntoView({ block: 'nearest', inline: 'nearest' });
    
    // Focus on the input span
    const inputSpan = win.querySelector('#cmd-input-span');
    if (inputSpan) {
        inputSpan.focus();
        placeCaretAtEnd(inputSpan);
    }
}

function addFocusHandler(win) {
    win.addEventListener('mousedown', function(e) {
        // Remove .active class from all terminal windows
        document.querySelectorAll('.terminal-window').forEach(w => {
            w.classList.remove('active');
        });
        // Add .active class to the clicked window
        win.classList.add('active');
        
        // Bring the clicked window to the front
        win.style.zIndex = currentZ++;
        
        // Update the active terminal in ProcessManager if needed
        if (window.processManager) {
            const input = win.querySelector('.cmd-input-span');
            if (input) {
                window.processManager.activeTerminal = win;
            }
        }
        
        // Focus on input if it exists and the click was not on the title bar
        if (!e.target.closest('.title-bar') && !e.target.closest('.close-btn')) {
            const input = win.querySelector('.cmd-input-span');
            if (input) {
                input.focus();
                placeCaretAtEnd(input);
            }
        } else {
            // If clicking on title bar or close button, still focus the input so key events work
            const input = win.querySelector('.cmd-input-span');
            if (input) {
                input.focus();
            }
        }
    });
}

function newTerminal() {
    createTerminal();
    hideContextMenu();
}

function closeTerminal(btn) {
    const win = btn.closest('.terminal-window');
    
    // Remove the keydown event listener if it exists
    if (win.keyHandler) {
        document.removeEventListener('keydown', win.keyHandler);
    }
    
    win.remove();
    terminals.splice(terminals.indexOf(win), 1);
}

function minimizeTerminal(btn) {
    const win = btn.closest('.terminal-window');
    if (win) {
        // Create a tab for the minimized window
        createMinimizedTab(win, 'Terminal');
    }
}

function toggleTerminalFullscreen(btn) {
    const win = btn.closest('.terminal-window');
    if (win) {
        if (!win.classList.contains('fullscreen')) {
            // Store original dimensions and position before going fullscreen
            win.originalWidth = win.style.width;
            win.originalHeight = win.style.height;
            win.originalLeft = win.style.left;
            win.originalTop = win.style.top;
            
            // Apply fullscreen styles
            win.style.width = '100%';
            win.style.height = 'calc(100vh - 40px)'; // Subtract toolbar height
            win.style.top = '0px';
            win.style.left = '0px';
            win.classList.add('fullscreen');
        } else {
            // Restore original dimensions and position
            win.style.width = win.originalWidth || '400px';
            win.style.height = win.originalHeight || '300px';
            win.style.top = win.originalTop || '50px';
            win.style.left = win.originalLeft || '50px';
            win.classList.remove('fullscreen');
        }
    }
}

// Function to create a minimized tab
function createMinimizedTab(windowElement, title) {
    // Hide the actual window
    windowElement.style.display = 'none';
    
    // Create a unique ID for the tab
    const tabId = 'minimized-tab-' + Date.now() + Math.floor(Math.random() * 1000);
    
    // Create the tab element
    const tab = document.createElement('div');
    tab.id = tabId;
    tab.className = 'minimized-tab';
    
    // Create content with title and close button
    tab.innerHTML = `
        <span class="tab-title" style="flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; margin-right: 5px;">${title.substring(0, 15) + (title.length > 15 ? '...' : '')}</span>
        <span class="tab-close" style="cursor: pointer; font-weight: bold; padding: 0 3px;">×</span>
    `;
    
    // Add click event to restore the window
    tab.querySelector('.tab-title').addEventListener('click', function() {
        windowElement.style.display = 'block'; // Show the window again
        tab.remove(); // Remove the tab from the minimized bar
        
        // Make sure the terminal is properly focused and interactive again
        const inputSpan = windowElement.querySelector('.cmd-input-span');
        if (inputSpan) {
            inputSpan.focus();
            placeCaretAtEnd(inputSpan);
        }
        
        // Ensure the window has the proper active state
        document.querySelectorAll('.terminal-window').forEach(w => {
            w.classList.remove('active');
        });
        windowElement.classList.add('active');
        windowElement.style.zIndex = currentZ++;
        
        // Bring the terminal to front and ensure it's the active terminal
        if (window.processManager) {
            window.processManager.activeTerminal = windowElement;
        }
    });
    
    // Add event to close the tab (close the actual window)
    tab.querySelector('.tab-close').addEventListener('click', function(e) {
        e.stopPropagation(); // Prevent triggering the restore event
        windowElement.remove(); // Remove the actual window element
        tab.remove(); // Remove the tab from the minimized bar
    });
    
    // Add the tab to the minimized tab container
    document.getElementById('minimized-tabs-container').appendChild(tab);
    
    // Store reference to the tab in the window element for later use
    windowElement.minimizedTabId = tabId;
}

// Function to handle minimizing AppBase instances (craft, cube, etc.)
function minimizeApp(appInstance, title) {
    if (appInstance.gameWindow) {
        // Hide the actual window
        appInstance.gameWindow.style.display = 'none';
        
        // Create a tab for the minimized window
        createMinimizedTab(appInstance.gameWindow, title);
    }
}

// Add command history functionality
let commandHistory = [];
let historyIndex = -1;

function setupTerminalInput(terminalWindow) {
    const inputSpan = terminalWindow.querySelector('.cmd-input-span');
    const outputDiv = terminalWindow.querySelector('.output');
    
    // Function to add a line to the output
    function addOutputLine(content) {
        const outputLine = document.createElement('div');
        outputLine.className = 'terminal-prompt';
        outputLine.innerHTML = content;
        outputDiv.appendChild(outputLine);
        
        // Scroll to bottom
        outputDiv.scrollTop = outputDiv.scrollHeight;
    }
    
    // Add keydown event listener for the entire terminal window to handle Ctrl+C and Ctrl+Z
    // This event listener will be triggered when the terminal window is focused
    const keyHandler = function(e) {
        // Handle Ctrl+C for interrupting foreground processes
        if (e.ctrlKey && e.key === 'c') {
            // Check if this terminal is currently focused
            const activeTerminal = window.processManager ? window.processManager.getActiveTerminal() : null;
            const thisTerminalElement = terminalWindow.querySelector('.cmd-input-span');
            const isThisTerminalActive = activeTerminal === terminalWindow || 
                                         (document.activeElement && 
                                          document.activeElement.closest('.terminal-window') === terminalWindow);
            
            if (isThisTerminalActive) {
                e.preventDefault();
                const fgJob = window.processManager ? window.processManager.getForegroundJob() : null;
                if (fgJob) {
                    // Add interrupt indicator to output
                    addOutputLine(`<span class="prompt-symbol">></span> ^C`);
                    
                    // Terminate the foreground job
                    if (window.processManager) {
                        window.processManager.terminateProcess(fgJob.jobId);
                    }
                    
                    // Clear foreground job to return control to terminal
                    if (window.processManager) {
                        window.processManager.clearForegroundJob();
                    }
                    
                    // Add prompt to show terminal is ready for input
                    const promptLine = document.createElement('div');
                    promptLine.className = 'terminal-prompt';
                    promptLine.innerHTML = `<span class="prompt-symbol">$</span>&nbsp;`;
                    outputDiv.appendChild(promptLine);
                    
                    // Focus on the input
                    inputSpan.focus();
                }
            }
        }
        // Handle Ctrl+Z for suspending foreground processes
        else if (e.ctrlKey && e.key === 'z') {
            // Check if this terminal is currently focused
            const activeTerminal = window.processManager ? window.processManager.getActiveTerminal() : null;
            const thisTerminalElement = terminalWindow.querySelector('.cmd-input-span');
            const isThisTerminalActive = activeTerminal === terminalWindow || 
                                         (document.activeElement && 
                                          document.activeElement.closest('.terminal-window') === terminalWindow);
            
            if (isThisTerminalActive) {
                e.preventDefault();
                const fgJob = window.processManager ? window.processManager.getForegroundJob() : null;
                if (fgJob) {
                    // Add suspend indicator to output
                    addOutputLine(`<span class="prompt-symbol">></span> ^Z`);
                    
                    // Suspend the foreground job
                    if (window.processManager) {
                        window.processManager.suspendProcess(fgJob.jobId);
                    }
                    
                    // Clear foreground job to return control to terminal
                    if (window.processManager) {
                        window.processManager.clearForegroundJob();
                    }
                    
                    // Add message about suspended job
                    const suspendLine = document.createElement('div');
                    suspendLine.className = 'terminal-prompt';
                    suspendLine.innerHTML = `<span class="prompt-symbol">></span> [${fgJob.jobId}]+ Stopped ${fgJob.command}`;
                    outputDiv.appendChild(suspendLine);
                    
                    // Add prompt to show terminal is ready for input
                    const promptLine = document.createElement('div');
                    promptLine.className = 'terminal-prompt';
                    promptLine.innerHTML = `<span class="prompt-symbol">$</span>&nbsp;`;
                    outputDiv.appendChild(promptLine);
                    
                    // Focus on the input
                    inputSpan.focus();
                }
            }
        }
    };
    
    // Attach event to document so it works even when the terminal isn't focused
    document.addEventListener('keydown', keyHandler);
    
    // Store the handler so we can remove it later if needed
    terminalWindow.keyHandler = keyHandler;
    
    inputSpan.addEventListener('keydown', function(e) {
        if (e.key === 'Enter') {
            e.preventDefault();
            
            const command = this.innerText.trim();
            
            // Add to command history
            if (command) {
                commandHistory.push(command);
                historyIndex = commandHistory.length; // Set to end of history
            }
            
            // Add command to output as input echo
            addOutputLine(`<span class="prompt-symbol">$</span> ${command}`);
            
            // Process the command
            processCommand(command, outputDiv);
            
            // Clear the input for the next command
            this.innerText = '';
            
            // Ensure cursor is at the end
            placeCaretAtEnd(this);
        }
        else if (e.key === 'ArrowUp') {
            e.preventDefault();
            // Navigate up in command history
            if (commandHistory.length > 0) {
                if (historyIndex <= 0) {
                    historyIndex = commandHistory.length;
                }
                historyIndex--;
                this.innerText = commandHistory[historyIndex];
                placeCaretAtEnd(this);
            }
        }
        else if (e.key === 'ArrowDown') {
            e.preventDefault();
            // Navigate down in command history
            if (commandHistory.length > 0 && historyIndex < commandHistory.length - 1) {
                historyIndex++;
                this.innerText = commandHistory[historyIndex];
            } else {
                historyIndex = commandHistory.length;
                this.innerText = ''; // Clear if at the end of history
            }
            placeCaretAtEnd(this);
        }
    });
}

function processCommand(command, outputDiv) {
    // Split command and potential arguments, checking for background execution
    let isBackground = false;
    let commandStr = command.trim();
    
    // Check if command ends with & for background execution
    if (commandStr.endsWith(' &')) {
        isBackground = true;
        commandStr = commandStr.slice(0, -2).trim();
    }
    
    const parts = commandStr.split(/\s+/);
    const cmd = parts[0].toLowerCase();
    const args = parts.slice(1);
    
    let response = '';
    
    // Get the terminal window that initiated this command
    const terminalWindow = outputDiv.closest('.terminal-window');
    
    // Process job control commands first
    switch (cmd) {
        case 'jobs':
            response = listJobsCommand();
            break;
        case 'fg':
            response = fgCommand(args);
            break;
        case 'bg':
            response = bgCommand(args);
            break;
        case 'kill':
            response = killCommand(args);
            break;
        case 'list':
            response = 'Available apps:\n- settings\n- profiles\n- wallet\n- store\n- msr\n- craft\n- cube\n- 3dtactics';
            break;
        case 'help':
            response = 'Available commands:\n- list: Show available apps\n- help: Show this help message\n- login: Login with username [usage: login username]\n- logout: Logout from current session\n- settings: Open settings panel\n- profiles: Manage profiles\n- wallet: Access wallet\n- store: Access store\n- msr: Run MSR app\n- craft: Open 3D crafting game\n- cube: Open cube app\n\nJob Control Commands:\n- jobs: List all running jobs\n- fg [job_id]: Bring job to foreground\n- bg [job_id]: Resume suspended job in background\n- kill [job_id]: Terminate a job\n- [command] &: Run command in background';
            break;
        case 'login':
            if (args.length >= 1) {
                response = login(args[0], args[1] || ''); // password is optional in this simple implementation
            } else {
                response = 'Usage: login [username] [password]';
            }
            break;
        case 'logout':
            response = logout();
            break;
        case 'settings':
            if (window.settingsApp) {
                window.settingsApp.openCustomizePanel();
                response = 'Opening settings panel...';
            } else {
                response = 'Settings app not loaded. Try refreshing the page.';
            }
            break;
        case 'profiles':
            response = 'Opening profiles app...';
            // In a real implementation, this would load profiles.js
            break;
        case 'wallet':
            response = 'Opening wallet app...';
            // In a real implementation, this would load wallet.js
            break;
        case 'store':
            response = 'Opening store app...';
            // In a real implementation, this would load store.js
            break;
        case 'setup':
            if (typeof window.SetupApp === 'function') {
                const newInstance = new SetupApp();
                newInstance.createGUI();
                response = 'Opening MSR Game Setup...';
            } else {
                response = 'Setup app not loaded. Try refreshing the page.';
            }
            break;
        case 'msr':
            if (typeof window.MSRApp === 'function') {
                const newInstance = new MSRApp();
                newInstance.createGUI();
                response = 'MSR application opened.';
            } else {
                response = 'MSR app not loaded. Try refreshing the page.';
            }
            break;
        case 'craft':
            if (typeof window.CraftApp === 'function') {
                // Use ProcessManager to start the craft app
                const terminalElement = outputDiv.closest('.terminal-window');
                // We'll set the windowRef later after creating the instance
                const processInfo = window.processManager.startProcess('craft', args, terminalElement, isBackground);
                
                // Create new CraftApp instance with the process ID
                const newInstance = new CraftApp(processInfo.jobId);
                newInstance.init(); // Pass the job ID so the app can register itself
                newInstance.jobId = processInfo.jobId; // Also directly set the jobId
                
                // Register the app instance as the windowRef in the process manager
                window.processManager.registerAppWindow(processInfo.jobId, newInstance);
                
                if (isBackground) {
                    response = `[${processInfo.jobId}]+ Running craft (PID: ${processInfo.pid}) [background]`;
                } else {
                    response = `[${processInfo.jobId}]+ Running craft (PID: ${processInfo.pid}) ... [foreground]`;
                }
            } else {
                response = 'Craft app class not loaded. Try refreshing the page.';
            }
            break;
        case 'cube':
            if (typeof window.CubeApp === 'function') {
                // Use ProcessManager to start the cube app
                const terminalElement = outputDiv.closest('.terminal-window');
                // We'll set the windowRef later after creating the instance
                const processInfo = window.processManager.startProcess('cube', args, terminalElement, isBackground);
                
                // Create new CubeApp instance with the process ID
                const newInstance = new CubeApp(processInfo.jobId);
                newInstance.init(); // Pass the job ID so the app can register itself
                newInstance.jobId = processInfo.jobId; // Also directly set the jobId
                
                // Register the app instance as the windowRef in the process manager
                window.processManager.registerAppWindow(processInfo.jobId, newInstance);
                
                if (isBackground) {
                    response = `[${processInfo.jobId}]+ Running cube (PID: ${processInfo.pid}) [background]`;
                } else {
                    response = `[${processInfo.jobId}]+ Running cube (PID: ${processInfo.pid}) ... [foreground]`;
                }
            } else {
                response = 'Cube app class not loaded. Try refreshing the page.';
            }
            break;
        case '3dtactics':
        case 'tactics':
            if (typeof window.TacticsGameApp === 'function') {
                // Use ProcessManager to start the 3D tactics app
                const terminalElement = outputDiv.closest('.terminal-window');
                // We'll set the windowRef later after creating the instance
                const processInfo = window.processManager.startProcess('3dtactics', args, terminalElement, isBackground);
                
                // Create new TacticsGameApp instance with the process ID
                const newInstance = new TacticsGameApp(processInfo.jobId);
                newInstance.init(); // Pass the job ID so the app can register itself
                newInstance.jobId = processInfo.jobId; // Also directly set the jobId
                
                // Register the app instance as the windowRef in the process manager
                window.processManager.registerAppWindow(processInfo.jobId, newInstance);
                
                if (isBackground) {
                    response = `[${processInfo.jobId}]+ Running 3dtactics (PID: ${processInfo.pid}) [background]`;
                } else {
                    response = `[${processInfo.jobId}]+ Running 3dtactics (PID: ${processInfo.pid}) ... [foreground]`;
                }
            } else {
                response = '3D Tactics app class not loaded. Try refreshing the page.';
            }
            break;
        default:
            response = `Command '${command}' not recognized. Type 'help' for available commands.`;
    }
    
    // Update active terminal in ProcessManager after command execution, if this was a foreground job
    if (window.processManager && terminalWindow && !isBackground && 
        (cmd === 'craft' || cmd === 'cube') && 
        typeof window[cmd.charAt(0).toUpperCase() + cmd.slice(1) + 'App'] !== 'undefined') {
        // Set this terminal as active for any foreground process launched from it
        window.processManager.activeTerminal = terminalWindow;
    }
    
    // Add the response to the output
    const responseLines = response.split('\n');
    responseLines.forEach(line => {
        if (line.trim() !== '') {  // Only add non-empty lines
            const responseLine = document.createElement('div');
            responseLine.className = 'terminal-prompt';
            responseLine.innerHTML = `<span class="prompt-symbol">></span> ${line}`;
            outputDiv.appendChild(responseLine);
        }
    });
    
    // Scroll to bottom after adding all content
    outputDiv.scrollTop = outputDiv.scrollHeight;
    
    // Ensure cursor is visible at the bottom
    const inputArea = terminalWindow.querySelector('.input-area');
    if (inputArea) {
        inputArea.scrollIntoView({ block: 'nearest' });
    }
}

// Job control command implementations
function listJobsCommand() {
    const jobs = window.processManager.listJobs();
    if (jobs.length === 0) {
        return 'No jobs running.';
    }
    
    let result = '';
    jobs.forEach((job, index) => {
        const indicator = index === jobs.length - 1 ? '+' : '-'; // Mark most recent job with +
        result += `[${job.jobId}]${indicator} ${job.status} ${job.command} (PID: ${job.pid})\n`;
    });
    
    return result.trim();
}

function fgCommand(args) {
    if (args.length === 0) {
        // If no argument provided, bring the most recent job to foreground
        const jobs = window.processManager.listJobs();
        if (jobs.length === 0) {
            return 'No jobs to bring to foreground.';
        }
        
        // Find the most recent job
        const mostRecentJob = jobs.reduce((prev, current) => 
            (prev.startTime > current.startTime) ? prev : current
        );
        
        if (window.processManager.resumeProcess(mostRecentJob.jobId, false)) {
            return `[${mostRecentJob.jobId}]+ Running ${mostRecentJob.command} (PID: ${mostRecentJob.pid}) ... [foreground]`;
        } else {
            return `Failed to bring job ${mostRecentJob.jobId} to foreground.`;
        }
    } else {
        // Job ID provided
        const jobIdStr = args[0];
        let jobId;
        
        // Parse job ID (could be %1, %2, etc. or just 1, 2, etc.)
        if (jobIdStr.startsWith('%')) {
            jobId = parseInt(jobIdStr.substring(1));
        } else {
            jobId = parseInt(jobIdStr);
        }
        
        if (isNaN(jobId)) {
            return `Invalid job ID: ${args[0]}`;
        }
        
        if (window.processManager.resumeProcess(jobId, false)) {
            const job = window.processManager.getJobById(jobId);
            if (job) {
                return `[${jobId}]+ Running ${job.command} (PID: ${job.pid}) ... [foreground]`;
            } else {
                return `Job ${jobId} not found.`;
            }
        } else {
            return `Failed to bring job ${jobId} to foreground.`;
        }
    }
}

function bgCommand(args) {
    if (args.length === 0) {
        return 'Usage: bg [job_id]';
    }
    
    const jobIdStr = args[0];
    let jobId;
    
    // Parse job ID (could be %1, %2, etc. or just 1, 2, etc.)
    if (jobIdStr.startsWith('%')) {
        jobId = parseInt(jobIdStr.substring(1));
    } else {
        jobId = parseInt(jobIdStr);
    }
    
    if (isNaN(jobId)) {
        return `Invalid job ID: ${args[0]}`;
    }
    
    if (window.processManager.resumeProcess(jobId, true)) {
        const job = window.processManager.getJobById(jobId);
        if (job) {
            return `[${jobId}]+ Running ${job.command} (PID: ${job.pid}) [background]`;
        } else {
            return `Job ${jobId} not found.`;
        }
    } else {
        return `Failed to resume job ${jobId} in background.`;
    }
}

function killCommand(args) {
    if (args.length === 0) {
        return 'Usage: kill [job_id]';
    }
    
    const jobIdStr = args[0];
    let jobId;
    
    // Parse job ID (could be %1, %2, etc. or just 1, 2, etc.)
    if (jobIdStr.startsWith('%')) {
        jobId = parseInt(jobIdStr.substring(1));
    } else {
        jobId = parseInt(jobIdStr);
    }
    
    if (isNaN(jobId)) {
        return `Invalid job ID: ${args[0]}`;
    }
    
    if (window.processManager.terminateProcess(jobId)) {
        return `[${jobId}]+ Terminated`;
    } else {
        return `Failed to terminate job ${jobId}.`;
    }
}

function placeCaretAtEnd(el) {
    el.focus();
    if (typeof window.getSelection != "undefined" && typeof document.createRange != "undefined") {
        const range = document.createRange();
        range.selectNodeContents(el);
        range.collapse(false);
        const sel = window.getSelection();
        sel.removeAllRanges();
        sel.addRange(range);
    }
}

function handleCmd(e, input) {
    // This function is kept for compatibility but might not be used with the new interface
    if (e.key === 'Enter') {
        const cmd = input.value.trim();
        const output = input.parentElement.querySelector('.output');
        output.innerHTML += `<div>$${cmd}</div><div>> ${cmd || '(empty command executed)'}</div>`;
        output.scrollTop = output.scrollHeight;
        input.value = '';
    }
}

function makeDraggable(el) {
    let pos1 = 0, pos2 = 0, pos3 = 0, pos4 = 0;
    const titleBar = el.querySelector('.title-bar');
    titleBar.onmousedown = dragMouseDown;

    function dragMouseDown(e) {
        e.preventDefault();
        pos3 = e.clientX;
        pos4 = e.clientY;
        document.onmouseup = closeDrag;
        document.onmousemove = elementDrag;
    }

    function elementDrag(e) {
        e.preventDefault();
        pos1 = pos3 - e.clientX;
        pos2 = pos4 - e.clientY;
        pos3 = e.clientX;
        pos4 = e.clientY;
        el.style.top = (el.offsetTop - pos2) + 'px';
        el.style.left = (el.offsetLeft - pos1) + 'px';
    }

    function closeDrag() {
        document.onmouseup = null;
        document.onmousemove = null;
    }
}

function dummyOption1() {
    console.log('Dummy Option 1 clicked');
    hideContextMenu();
}

function dummyOption2() {
    console.log('Dummy Option 2 clicked');
    hideContextMenu();
}

function customize() {
    document.getElementById('customize-panel').style.display = 'flex';
    hideContextMenu();
    
    // Call the settings app if it exists
    if (window.settingsApp) {
        window.settingsApp.openCustomizePanel();
    }
}

function closeCustomize() {
    document.getElementById('customize-panel').style.display = 'none';
}

function openTestPanel() {
    document.getElementById('test-panel').style.display = 'flex';
}

function closeTestPanel() {
    document.getElementById('test-panel').style.display = 'none';
}

// Initialize the settings app when the page loads
document.addEventListener('DOMContentLoaded', function() {
    if (typeof SettingsApp !== 'undefined') {
        if (!window.settingsApp) {
            window.settingsApp = new SettingsApp();
            window.settingsApp.init();
        }
    }
    
    // Initialize user display
    updateUserDisplay();
});

function updateTheme() {
    document.documentElement.style.setProperty('--bg-color', document.getElementById('bg-color').value);
    document.documentElement.style.setProperty('--text-color', document.getElementById('text-color').value);
    document.documentElement.style.setProperty('--font-family', document.getElementById('font-family').value);
    document.documentElement.style.setProperty('--terminal-bg', document.getElementById('terminal-bg').value);
    document.documentElement.style.setProperty('--terminal-text', document.getElementById('terminal-text').value);
    document.documentElement.style.setProperty('--title-bg', document.getElementById('title-bg').value);
    document.documentElement.style.setProperty('--title-text', document.getElementById('title-text').value);
    document.documentElement.style.setProperty('--toolbar-bg', document.getElementById('toolbar-bg').value);
    document.documentElement.style.setProperty('--toolbar-text', document.getElementById('toolbar-text').value);
    document.documentElement.style.setProperty('--context-bg', document.getElementById('context-bg').value);
    document.documentElement.style.setProperty('--context-text', document.getElementById('context-text').value);
    document.documentElement.style.setProperty('--context-hover', document.getElementById('context-hover').value);
}

function hideContextMenu() {
    document.getElementById('context-menu').style.display = 'none';
}

// Event listeners
document.addEventListener('DOMContentLoaded', function() {
    console.log('DOMContentLoaded fired');
    
    document.getElementById('term-btn').onclick = createTerminal;

    // Add event listener for Settings button
    const settingsBtn = document.getElementById('settings-btn');
    console.log('Settings button element:', settingsBtn);
    if (settingsBtn) {
        settingsBtn.addEventListener('click', function() {
            console.log('Settings button clicked!');
            customize();
        });
        console.log('Event listener added to Settings button');
    } else {
        console.log('Settings button not found!');
    }

    // Add event listener for Files button
    const filesBtn = document.getElementById('files-btn');
    console.log('Files button element:', filesBtn);
    if (filesBtn) {
        filesBtn.addEventListener('click', function() {
            // You can add files functionality here if needed
            console.log('Files button clicked');
        });
        console.log('Event listener added to Files button');
    }

    // Add event listener for Test button
    const testBtn = document.getElementById('test-btn');
    console.log('Test button element:', testBtn);
    if (testBtn) {
        testBtn.addEventListener('click', function() {
            console.log('Test button clicked!');
            openTestPanel();
        });
        console.log('Event listener added to Test button');
    } else {
        console.log('Test button not found!');
    }

    document.addEventListener('contextmenu', (e) => {
        e.preventDefault();
        const menu = document.getElementById('context-menu');
        menu.style.display = 'block';
        menu.style.left = e.pageX + 'px';
        menu.style.top = e.pageY + 'px';
    });

    document.addEventListener('click', function(e) {
        const contextMenu = document.getElementById('context-menu');
        const customizePanel = document.getElementById('customize-panel');
        const testPanel = document.getElementById('test-panel');
        
        // Check if the click is outside the context menu and the menu is visible
        if (contextMenu.offsetParent !== null && !contextMenu.contains(e.target)) {
            hideContextMenu();
        }
        
        // Check if the click is outside the customize panel and the panel is visible
        if (customizePanel.offsetParent !== null && !customizePanel.contains(e.target)) {
            closeCustomize();
        }
        
        // Check if the click is outside the test panel and the panel is visible
        if (testPanel.offsetParent !== null && !testPanel.contains(e.target)) {
            closeTestPanel();
        }
    });
});

// Make functions available globally for use by AppBase instances
window.createMinimizedTab = createMinimizedTab;