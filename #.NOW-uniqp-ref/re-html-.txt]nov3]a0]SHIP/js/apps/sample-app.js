// sample-app.js - Sample application using innerHTML approach

function createSampleAppWindow() {
    const sampleContent = `
        <div style="padding: 15px; height: 100%; display: flex; flex-direction: column;">
            <h3>Sample Application</h3>
            <p>This is a sample application running in a desktop window.</p>
            <p>Current time: <span id="time-display">${new Date().toLocaleString()}</span></p>
            
            <div style="border: 1px solid #ccc; padding: 10px; margin: 10px 0; background: #f9f9f9;">
                <h4>Interactive Elements</h4>
                <button onclick="updateTime()">Update Time</button>
                <button onclick="showAlert()">Show Alert</button>
                <button onclick="createCounter()">Create Counter</button>
                <div id="counter-container" style="margin-top: 10px;"></div>
            </div>
            
            <div style="border: 1px solid #ccc; padding: 10px; margin: 10px 0; background: #f0f0f0;">
                <h4>Information</h4>
                <p>This application demonstrates:</p>
                <ul>
                    <li>Dynamic HTML content in windows</li>
                    <li>Interactive JavaScript functionality</li>
                    <li>Embedded scripts within innerHTML</li>
                    <li>Multiple instances support</li>
                </ul>
            </div>
            
            <div style="margin-top: auto; text-align: center; color: #666; font-style: italic;">
                <p>Powered by HTML Desktop Environment</p>
            </div>
        </div>
    `;
    
    const window = createWindow('Sample Application', sampleContent, 550, 450);
    
    // Set up interval to update time display (if this element exists)
    const timeInterval = setInterval(() => {
        const timeDisplay = window.querySelector('#time-display');
        if (timeDisplay) {
            timeDisplay.textContent = new Date().toLocaleString();
        }
    }, 1000);
    
    // Clean up interval when window is closed
    const originalClose = window.querySelector('.close-btn').onclick;
    window.querySelector('.close-btn').onclick = function() {
        clearInterval(timeInterval);
        if (originalClose) originalClose();
        closeWindow(window.id);
    };
    
    return window;
}

// Function to update the time display
function updateTime() {
    // We'll update the parent window's time display
    const activeWindow = document.querySelector('.window[style*="z-index: ' + currentZ + '"]') || 
                        document.querySelector('.window:hover') || 
                        document.querySelector('.window:last-child');
    
    if (activeWindow) {
        const timeDisplay = activeWindow.querySelector('#time-display');
        if (timeDisplay) {
            timeDisplay.textContent = new Date().toLocaleString();
        }
    }
}

// Function to show an alert
function showAlert() {
    alert('Hello from the Sample Application!');
}

// Function to create a counter
function createCounter() {
    const activeWindow = document.querySelector('.window[style*="z-index: ' + currentZ + '"]') || 
                        document.querySelector('.window:hover') || 
                        document.querySelector('.window:last-child');
    
    if (activeWindow) {
        const container = activeWindow.querySelector('#counter-container');
        if (container) {
            // Create a counter element if one doesn't already exist
            if (!container.querySelector('.counter')) {
                const counterDiv = document.createElement('div');
                counterDiv.className = 'counter';
                counterDiv.style.marginTop = '10px';
                counterDiv.style.padding = '10px';
                counterDiv.style.border = '1px solid #999';
                counterDiv.style.display = 'inline-block';
                
                let count = 0;
                
                counterDiv.innerHTML = `
                    <span>Count: <span id="count-value">${count}</span></span>
                    <button onclick="incrementCounter('${activeWindow.id}')" style="margin-left: 10px;">+</button>
                    <button onclick="decrementCounter('${activeWindow.id}')" style="margin-left: 5px;">-</button>
                    <button onclick="resetCounter('${activeWindow.id}')" style="margin-left: 5px;">Reset</button>
                `;
                
                container.appendChild(counterDiv);
            }
        }
    }
}

// Counter functions
function incrementCounter(windowId) {
    const win = document.getElementById(windowId);
    if (win) {
        const countValue = win.querySelector('#count-value');
        if (countValue) {
            countValue.textContent = parseInt(countValue.textContent) + 1;
        }
    }
}

function decrementCounter(windowId) {
    const win = document.getElementById(windowId);
    if (win) {
        const countValue = win.querySelector('#count-value');
        if (countValue) {
            countValue.textContent = Math.max(0, parseInt(countValue.textContent) - 1);
        }
    }
}

function resetCounter(windowId) {
    const win = document.getElementById(windowId);
    if (win) {
        const countValue = win.querySelector('#count-value');
        if (countValue) {
            countValue.textContent = 0;
        }
    }
}