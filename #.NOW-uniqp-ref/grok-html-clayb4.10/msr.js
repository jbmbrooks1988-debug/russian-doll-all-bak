// msr.js - MSR App for JS CLI (matching GCC C version setup)

class MSRApp {
    constructor() {
        this.config = {};
        this.windows = {};
        this.isInitialized = false;
        
        // Defer GUI creation until DOM is ready
        if (typeof document !== 'undefined' && document.readyState !== 'loading') {
            this.createGUI();
        } else if (typeof document !== 'undefined') {
            document.addEventListener('DOMContentLoaded', () => {
                this.createGUI();
            });
        }
    }
    
    createGUI() {
        // Create MSR main window if it doesn't exist
        if (typeof document !== 'undefined' && !document.getElementById('msr-main-window')) {
            const msrWindow = document.createElement('div');
            msrWindow.id = 'msr-main-window';
            msrWindow.className = 'terminal-window';
            msrWindow.style.display = 'none';
            msrWindow.style.width = '600px';
            msrWindow.style.height = '400px';
            msrWindow.innerHTML = `
                <div class="title-bar">
                    MSR Application
                    <button class="close-btn" onclick="window.msrApp.closeGUI()">×</button>
                </div>
                <div class="terminal-content">
                    <div style="padding: 10px; height: calc(100% - 40px); display: flex; flex-direction: column;">
                        <div style="margin-bottom: 10px;">
                            <button onclick="window.msrApp.compileCode()">Compile</button>
                            <button onclick="window.msrApp.uploadCode()">Upload</button>
                            <button onclick="window.msrApp.runAnalysis()">Analyze</button>
                        </div>
                        <div id="msr-output" style="flex: 1; overflow-y: auto; background: #222; color: #00ff00; padding: 8px; font-family: monospace; font-size: 12px;"></div>
                    </div>
                </div>
            `;
            document.body.appendChild(msrWindow);
        }
    }
    
    init() {
        console.log('MSR App Initialized');
        this.isInitialized = true;
        
        // Initialize MSR configuration
        this.config = {
            compiler: 'gcc',
            target: 'microcontroller',
            optimization: '-O2'
        };
    }
    
    showGUI() {
        // Ensure GUI is created before showing
        if (typeof document !== 'undefined' && !document.getElementById('msr-main-window')) {
            this.createGUI();
        }
        
        const window = document.getElementById('msr-main-window');
        if (window) {
            window.style.display = 'block';
            window.style.zIndex = 100; // Ensure it appears on top
            window.style.position = 'absolute';
            window.style.left = (window.innerWidth/2 - 300) + 'px';
            window.style.top = (window.innerHeight/2 - 200) + 'px';
        }
    }
    
    closeGUI() {
        const window = document.getElementById('msr-main-window');
        if (window) {
            window.style.display = 'none';
        }
    }
    
    compileCode() {
        this.addOutput('Compiling code with GCC...');
        // Simulate compilation process
        setTimeout(() => {
            this.addOutput('Compilation successful!');
            this.addOutput('Generated: output.hex');
        }, 1000);
    }
    
    uploadCode() {
        this.addOutput('Uploading code to target device...');
        // Simulate upload process
        setTimeout(() => {
            this.addOutput('Upload successful!');
            this.addOutput('Device programmed at: ' + new Date().toLocaleTimeString());
        }, 1500);
    }
    
    runAnalysis() {
        this.addOutput('Running MSR analysis...');
        // Simulate analysis process
        setTimeout(() => {
            this.addOutput('Analysis complete!');
            this.addOutput('Found potential issues: 2');
            this.addOutput('Optimization suggestions: 3');
        }, 1200);
    }
    
    addOutput(text) {
        const output = document.getElementById('msr-output');
        if (output) {
            const line = document.createElement('div');
            line.textContent = `> ${text}`;
            output.appendChild(line);
            output.scrollTop = output.scrollHeight;
        }
    }
    
    clearOutput() {
        const output = document.getElementById('msr-output');
        if (output) {
            output.innerHTML = '';
        }
    }
}

// Export or initialize depending on environment
if (typeof module !== 'undefined' && module.exports) {
    module.exports = MSRApp;
} else if (typeof window !== 'undefined') {
    window.MSRApp = MSRApp;
    
    // Initialize MSR app if running in browser
    if (!window.msrApp) {
        // Create the MSR app instance but defer GUI creation
        window.msrApp = new MSRApp();
        window.msrApp.init();
    }
}