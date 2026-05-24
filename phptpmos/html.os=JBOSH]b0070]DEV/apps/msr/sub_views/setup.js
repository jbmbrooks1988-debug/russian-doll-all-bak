// apps/msr/sub_views/setup.js

class SetupApp extends AppBase {
    constructor(jobId) {
        super(jobId);
    }

    async createGUI() {
        const windowId = `${this.instanceId}-setup-window`;
        if (document.getElementById(windowId)) {
            this.bringToFront();
            return;
        }

        const windowDiv = this.createWindow(windowId, 'MSR :: Game Setup');
        
        try {
            const response = await fetch('apps/msr]PHP]dec27]c3/setup.html');
            if (!response.ok) {
                throw new Error(`Failed to fetch setup.html: ${response.statusText}`);
            }
            let html = await response.text();
            
            const bodyMatch = html.match(/<body[^>]*>([\s\S]*)<\/body>/i);
            if (bodyMatch && bodyMatch[1]) {
                this.setContent(bodyMatch[1]);
                this.initSetup();
            } else {
                throw new Error("Could not parse body content from setup.html");
            }
            
        } catch (error) {
            this.setContent(`<div class="error">Error loading Setup content: ${error.message}</div>`);
            console.error('Error loading Setup content:', error);
        }
    }

    initSetup() {
        this.startGameBtn = this.getElementById('start-new-game-btn');
        this.outputDiv = this.getElementById('setup-output');
        this.continueSection = this.getElementById('continue-section');

        this.startGameBtn.addEventListener('click', () => this.runSetup());
        
        const continueButton = this.getElementById('continue-section').querySelector('button');
        if (continueButton) {
            continueButton.parentElement.href = "#"; // Disable navigation
            continueButton.addEventListener('click', (e) => {
                e.preventDefault();
                this.closeGUI();
            });
        }
    }

    addToOutput(message, isError = false) {
        const line = document.createElement('div');
        line.textContent = '> ' + message;
        if (isError) {
            line.style.color = '#ff4d4d';
        }
        this.outputDiv.appendChild(line);
        this.outputDiv.scrollTop = this.outputDiv.scrollHeight;
    }

    async runSetup() {
        this.startGameBtn.disabled = true;
        this.addToOutput('Starting new game setup...');
        
        this.addToOutput('Clearing old game data...');
        try {
            const clearResponse = await fetch('apps/msr]PHP]dec27]c3/php/clear_temp.php', { method: 'POST' });
            const clearData = await clearResponse.json();
            if(clearData.status !== 'success') throw new Error(clearData.message);
            this.addToOutput(clearData.message);
        } catch (error) {
            this.addToOutput(`Error clearing old data: ${error.message}`, true);
            this.startGameBtn.disabled = false;
            return;
        }

        this.addToOutput('Setting up corporations...');
        try {
            const corpResponse = await fetch('apps/msr]PHP]dec27]c3/php/setup_corporations.php', { method: 'POST' });
            const corpData = await corpResponse.json();
            if (corpData.status !== 'success') throw new Error(corpData.message);
            this.addToOutput(corpData.message);
        } catch (error) {
            this.addToOutput(`Error setting up corporations: ${error.message}`, true);
            this.startGameBtn.disabled = false;
            return;
        }

        this.addToOutput('Setting up governments...');
        try {
            const govResponse = await fetch('apps/msr]PHP]dec27]c3/php/setup_governments.php', { method: 'POST' });
            const govData = await govResponse.json();
            if (govData.status !== 'success') throw new Error(govData.message);
            this.addToOutput(govData.message);
        } catch (error) {
            this.addToOutput(`Error setting up governments: ${error.message}`, true);
            this.startGameBtn.disabled = false;
            return;
        }
        
        const playerName = this.getElementById('player-name').value;
        const startingCash = this.getElementById('starting-cash').value;
        const tickerSpeed = this.getElementById('ticker-speed').value;

        localStorage.setItem('playerName', playerName);
        
        this.addToOutput(`Creating player: ${playerName}...`);
        const formData = new FormData();
        formData.append('playerName', playerName);
        formData.append('startingCash', startingCash);
        formData.append('tickerSpeed', tickerSpeed);

        try {
            const playerResponse = await fetch('apps/msr]PHP]dec27]c3/php/create_player.php', { method: 'POST', body: formData });
            const playerData = await playerResponse.json();
            if (playerData.status !== 'success') throw new Error(playerData.message);
            this.addToOutput(playerData.message);
            
            this.addToOutput('Setup complete! You can now close this window and start the game.');
            this.continueSection.style.display = 'block';
        } catch (error) {
            this.addToOutput(`Error creating player: ${error.message}`, true);
            this.startGameBtn.disabled = false;
        }
    }
}

if (typeof window !== 'undefined') {
    window.SetupApp = SetupApp;
}