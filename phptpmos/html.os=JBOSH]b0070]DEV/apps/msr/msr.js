// apps/msr/msr.js

class MSRApp extends AppBase {
    constructor(jobId) {
        super(jobId);
        this.isInitialized = false;
        // Bind event handlers to the instance
        this.runGameTurn = this.runGameTurn.bind(this);
        this.toggleTicker = this.toggleTicker.bind(this);
        this.launchSubApp = this.launchSubApp.bind(this);
        this.updateGameInfo = this.updateGameInfo.bind(this);
        this.updateRealTimeDisplay = this.updateRealTimeDisplay.bind(this);
        this.updatePlayerData = this.updatePlayerData.bind(this);
        this.autoStartTicker = this.autoStartTicker.bind(this);
        this.updateNews = this.updateNews.bind(this);
        this.runGameTick = this.runGameTick.bind(this);
    }

    async createGUI() {
        const windowId = `${this.instanceId}-main-window`;
        let existingWindow = document.getElementById(windowId);
        
        if (typeof document !== 'undefined' && !existingWindow) {
            // Create new window if it doesn't exist
            const windowDiv = document.createElement('div');
            windowDiv.id = windowId;
            windowDiv.className = 'app-window terminal-window';
            windowDiv.style.display = 'none';
            
            // Set initial size
            const maxWidth = Math.min(this.getDefaultWidth(), window.innerWidth - 100);
            const maxHeight = Math.min(this.getDefaultHeight(), window.innerHeight - 100);
            windowDiv.style.width = maxWidth + 'px';
            windowDiv.style.height = maxHeight + 'px';
            
            windowDiv.innerHTML = `
                <div class="title-bar">
                    MSR :: Main Menu
                    <div class="window-buttons">
                        <button class="minimize-btn" onclick="window.appInstances['${this.instanceId}'].minimizeWindow()">-</button>
                        <button class="fullscreen-btn" onclick="window.appInstances['${this.instanceId}'].toggleFullscreen()">□</button>
                        <button class="close-btn" onclick="window.appInstances['${this.instanceId}'].closeGUI()">×</button>
                    </div>
                </div>
                <div class="terminal-content">
                    <div id="${this.instanceId}-main-content" class="app-content">
                        <div id="game-container">
                            <div class="menu-bar">
                                <span>Active Entity: <span class="info-value" id="${this.instanceId}-active-entity">N/A</span></span> |
                                <span>Date: <span class="info-value" id="${this.instanceId}-game-date">01/01/2025</span></span>
                            </div>
                            
                            <div class="separator">================================================</div>
                            
                            <div class="columns">
                                <!-- Left Column -->
                                <div class="column">
                                    <div class="balance-sheet">
                                        <div style="color: #ffff00; margin-bottom: 10px;">MY BALANCE SHEET:</div>
                                        <div class="info-line">Cash (CD's)..........<span id="${this.instanceId}-player-cash">0.00</span></div>
                                        <div class="info-line">Stock Portfolio......<span id="${this.instanceId}-portfolio-value">0.00</span></div>
                                        <div class="info-line">Total Assets.........<span id="${this.instanceId}-total-assets">0.00</span></div>
                                        <div class="info-line">Less Debt............<span id="${this.instanceId}-total-debt">-0.00</span></div>
                                        <div class="separator">---</div>
                                        <div class="info-line" style="color: #ffff00;">Net Worth............<span id="${this.instanceId}-net-worth">0.00</span></div>
                                    </div>
                                    <div class="news-ticker" style="margin-top: 20px;">
                                        <div style="color: #ff0000; margin-bottom: 5px;">FINANCIAL NEWS HEADLINES:</div>
                                        <div id="${this.instanceId}-news-content">Loading news...</div>
                                    </div>
                                </div>
                                
                                <!-- Right Column -->
                                <div class="column">
                                                     <div class="control-section">
                                                        <h3 style="color: #ffff00;">GAME ACTIONS</h3>
                                                        <button id="${this.instanceId}-next-turn-btn">Advance Game Turn (Next Day)</button>
                                                        <div id="${this.instanceId}-turn-output" class="output-box" style="height: 50px; margin-top: 10px;"></div>
                                                    </div>
                                            
                                                    <div class="separator">------------------------------------------------</div>
                                            
                                                    <div class="control-section">
                                                        <h3 style="color: #ffff00;">MAIN MENU</h3>
                                                         <a data-target-app="setup.html"><button>1. Game Setup</button></a>
                                                         <a data-target-app="game_options.html"><button>2. Game Options</button></a>
                                                         <a data-target-app="settings.html"><button>3. Settings</button></a>
                                                         <a data-target-app="help.html"><button>4. Help</button></a>
                                                    </div>
                                    
                                                    <div class="separator">------------------------------------------------</div>
                                                    
                                                    <div class="control-section">
                                                        <h3 style="color: #ffff00;">PLAYER & ENTITY</h3>
                                                        <button>5. Select Player</button> <!-- Handled by game_turn.php, no separate page -->
                                                        <a data-target-app="culture.html"><button>6. Culture</button></a>
                                                        <a data-target-app="entity_info.html"><button>7. Entity Info</button></a>
                                                        <a data-target-app="corporations.html"><button>8. Select Corp.</button></a>
                                                        <a data-target-app="history.html"><button>9. History</button></a>
                                                        <a data-target-app="general.html"><button>10. General</button></a>
                                                    </div>
                                    
                                                    <div class="separator">------------------------------------------------</div>
                                    
                                                    <div class="control-section">
                                                        <h3 style="color: #ffff00;">FINANCIAL ACTIONS</h3>
                                                        <a data-target-app="tools.html"><button>11. Tools</button></a>
                                                        <a data-target-app="buy_sell.html"><button>12. Buy/Sell</button></a>
                                                        <a data-target-app="financing.html"><button>13. Financing</button></a>
                                                        <a data-target-app="private.html"><button>14. Private</button></a>
                                                        <a data-target-app="management.html"><button>15. Management</button></a>
                                                        <a data-target-app="other_trans.html"><button>16. Other Trans</button></a>
                                                    </div>
                                    
                                                    <div class="separator">------------------------------------------------</div>
                                    
                                                    <div class="control-section">
                                                        <h3 style="color: #ffff00;">GAME FLOW & UTILITIES</h3>
                                                        <button id="${this.instanceId}-ticker-btn">22. TICKER <span id="${this.instanceId}-ticker-status">[OFF]</span></button> <!-- Toggle by JS -->
                                                    </div>
                                    
                                                    <div class="separator">------------------------------------------------</div>
                                    
                                                    <div class="control-section">
                                                        <h3 style="color: #ffff00;">QUICK SEARCH FUNCTIONS</h3>
                                                        <a data-target-app="research_report.html"><button>23. Research Report</button></a>
                                                        <a data-target-app="portfolio.html"><button>24. List Portfolio Holdings</button></a>
                                                        <a data-target-app="list_futures.html"><button>25. List Futures Contracts</button></a>
                                                        <a data-target-app="financial_profile.html"><button>26. Financial Profile</button></a>
                                                        <a data-target-app="list_options.html"><button>27. List Put and Call Options</button></a>
                                                        <a data-target-app="db_search.html"><button>28. Recall DB Search List</button></a>
                                                        <a data-target-app="earnings_report.html"><button>29. Earnings Report</button></a>
                                                        <a data-target-app="shareholder_list.html"><button>30. Shareholder List</button></a>
                                                        <a data-target-app="my_corporations.html"><button>31. My Corporations</button></a>
                                                    </div>
                                                    <div class="separator">----------------------------------</div>
                                                    <div class="control-section">
                                                        <h3 style="color: #ffff00;">COMMODITY PRICES/INDEXES/INDICATORS:</h3>
                                                        <div class="info-line">Stock Index: Market closed</div>
                                                        <div class="info-line">Prime Rate: 2.50%</div>
                                                        <div class="info-line">Long Bond: 7.25%</div>
                                                        <div class="info-line">Short Bond: 6.50%</div>
                                                        <div class="info-line">Spot Crude: $100.00</div>
                                                        <div class="info-line">Silver: $20.00</div>
                                                        <div class="info-line">Spot Gold: $1,200.00</div>
                                                        <div class="info-line">Spot Wheat: $5.00</div>
                                                        <div class="info-line">GDP Growth: 2.5%</div>
                                                        <div class="info-line">Spot Corn: $5.00</div>
                                                    </div>
                                                    <div class="separator">------------------------------------------</div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            `;
            
            document.body.appendChild(windowDiv);
            this.gameWindow = windowDiv;

            try {
                this.initMSR();
                this.showGUI(); // Make the window visible
            } catch (error) {
                const errorDiv = this.getEl('main-content');
                if (errorDiv) {
                    errorDiv.innerHTML = `<div class="error">Error loading MSR content: ${error.message}</div>`;
                }
                this.showGUI(); // Show the window to display the error
                console.error('Error loading MSR content:', error);
            }
        } else if (existingWindow) {
            this.gameWindow = existingWindow;
            this.showGUI(); // If window exists, just show it
        }
    }

    // Minimal showGUI override to break the recursive loop from AppBase
    showGUI() {
        if (this.gameWindow) {
            this.gameWindow.style.display = 'block';
            
            // Bring window to front
            if (window.currentZ === undefined) {
                window.currentZ = 100;
            }
            this.gameWindow.style.zIndex = window.currentZ++;
        }
    }
    initMSR() {
        if (this.isInitialized) return;
        this.isInitialized = true;

        this.playerName = localStorage.getItem('playerName') || 'Player1';
        
        this.activeEntityEl = this.getEl('active-entity');
        this.gameDateEl = this.getEl('game-date');
        this.playerCashEl = this.getEl('player-cash');
        this.portfolioValueEl = this.getEl('portfolio-value');
        this.totalAssetsEl = this.getEl('total-assets');
        this.totalDebtEl = this.getEl('total-debt');
        this.netWorthEl = this.getEl('net-worth');
        this.newsContentEl = this.getEl('news-content');
        this.nextTurnBtn = this.getEl('next-turn-btn');
        this.turnOutputEl = this.getEl('turn-output');
        this.tickerStatusEl = this.getEl('ticker-status');
        this.tickerBtn = this.getEl('ticker-btn');

        this.currentPlayerName = this.playerName;
        this.isTickerOn = false;

        this.lastKnownGameTime = null;
        this.lastFetchTime = null;
        this.tickerSpeedMultiplier = 3600;

        this._setupEventListeners();

        this.updateGameInfo();
        this.updatePlayerData();
        this.updateNews();
        this.autoStartTicker();

        this.gameInfoInterval = setInterval(this.updateGameInfo, 1000);
        this.realTimeDisplayInterval = setInterval(this.updateRealTimeDisplay, 1000);
        this.newsUpdateInterval = setInterval(this.updateNews, 10000);
    }

    _setupEventListeners() {
        this.nextTurnBtn?.addEventListener('click', this.runGameTurn);
        this.tickerBtn?.addEventListener('click', this.toggleTicker);

        const contentDiv = this.getEl('main-content');
        if (contentDiv) {
            contentDiv.querySelectorAll('a[data-target-app]').forEach(a => {
                a.addEventListener('click', this.launchSubApp);
                // Also remove the href attribute to prevent default browser navigation
                a.removeAttribute('href'); 
            });
        }

        const managementLink = contentDiv?.querySelector('a[data-target-app="management.html"]');
        if (managementLink) {
            managementLink.removeEventListener('click', this.launchSubApp);
            managementLink.addEventListener('click', (e) => {
                e.preventDefault();
                const tacticsApp = new TacticsGameApp(null);
                tacticsApp.createGUI();
            });
        }
    }

    async launchSubApp(event) {
        event.preventDefault();
        const targetAppFile = event.currentTarget.dataset.targetApp;
        const appName = targetAppFile.replace('.html', ''); 
        
        // Find the terminal this app is running in to pass the output context
        const terminalWindow = this.gameWindow.closest('.terminal-window');
        const outputDiv = terminalWindow.querySelector('.output');

        if (window.processCommand) {
            window.processCommand(appName, outputDiv);
        } else {
            console.error('processCommand function not found.');
        }
    }

    async updateGameInfo() {
        try {
            const response = await fetch('apps/msr]PHP]dec27]c3/php/get_gamestate.php'); // Corrected path
            if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
            const gamestate = await response.json();

            const currentTime = Date.now();
            let gameSecondsElapsed = 0;
            
            if (this.lastKnownGameTime && this.lastFetchTime) {
                const realTimeElapsed = currentTime - this.lastFetchTime;
                gameSecondsElapsed = (realTimeElapsed / 1000) * this.tickerSpeedMultiplier;
            }
            
            const newServerTime = {
                year: gamestate.year,
                month: gamestate.month,
                day: gamestate.day,
                hour: gamestate.hour,
                minute: gamestate.minute,
                second: gamestate.second
            };
            
            const speedSettings = { 'sec': 3600, 'min': 60, 'hour': 1, 'day': 86400 };
            this.tickerSpeedMultiplier = speedSettings[gamestate.ticker_speed] || 3600;

            let displayTime = new Date(
                newServerTime.year, newServerTime.month - 1, newServerTime.day,
                newServerTime.hour, newServerTime.minute, newServerTime.second
            );
            
            displayTime = new Date(displayTime.getTime() + (gameSecondsElapsed * 1000));
            
            this.lastKnownGameTime = {
                year: displayTime.getFullYear(),
                month: displayTime.getMonth() + 1,
                day: displayTime.getDate(),
                hour: displayTime.getHours(),
                minute: displayTime.getMinutes(),
                second: displayTime.getSeconds()
            };
            this.lastFetchTime = currentTime;

            if (this.gameDateEl) {
                this.gameDateEl.textContent = `${String(this.lastKnownGameTime.month).padStart(2, '0')}/${String(this.lastKnownGameTime.day).padStart(2, '0')}/${this.lastKnownGameTime.year} ${String(this.lastKnownGameTime.hour).padStart(2, '0')}:${String(this.lastKnownGameTime.minute).padStart(2, '0')}:${String(this.lastKnownGameTime.second).padStart(2, '0')}`;
            }
            
            if (gamestate.players && gamestate.players.length > 0) {
                this.currentPlayerName = gamestate.players[gamestate.current_player_index];
                if (this.activeEntityEl) this.activeEntityEl.textContent = this.currentPlayerName;
            } else {
                if (this.activeEntityEl) this.activeEntityEl.textContent = 'N/A';
            }
            this.updatePlayerData();
        } catch (error) {
            console.error("Error fetching game state:", error);
        }
    }

    updateRealTimeDisplay() {
        if (this.lastKnownGameTime && this.lastFetchTime && this.gameDateEl) {
            const currentTime = Date.now();
            const timeElapsed = currentTime - this.lastFetchTime;
            
            const gameSecondsElapsed = (timeElapsed / 1000) * this.tickerSpeedMultiplier;
            
            let displayTime = new Date(
                this.lastKnownGameTime.year, this.lastKnownGameTime.month - 1, this.lastKnownGameTime.day,
                this.lastKnownGameTime.hour, this.lastKnownGameTime.minute, this.lastKnownGameTime.second
            );
            
            displayTime = new Date(displayTime.getTime() + (gameSecondsElapsed * 1000));
            
            this.gameDateEl.textContent = `${String(displayTime.getMonth() + 1).padStart(2, '0')}/${String(displayTime.getDate()).padStart(2, '0')}/${displayTime.getFullYear()} ${String(displayTime.getHours()).padStart(2, '0')}:${String(displayTime.getMinutes()).padStart(2, '0')}:${String(displayTime.getSeconds()).padStart(2, '0')}`;
        }
    }

    updatePlayerData() {
        fetch(`apps/msr]PHP]dec27]c3/php/get_player_data.php?player=${this.currentPlayerName}`) // Corrected path
            .then(response => response.json())
            .then(data => {
                if (this.playerCashEl) this.playerCashEl.textContent = data.cash.toFixed(2);
                if (this.portfolioValueEl) this.portfolioValueEl.textContent = data.portfolio_value.toFixed(2);
                const totalAssets = data.cash + data.portfolio_value;
                if (this.totalAssetsEl) this.totalAssetsEl.textContent = totalAssets.toFixed(2);
                if (this.totalDebtEl) this.totalDebtEl.textContent = `-${data.debt.toFixed(2)}`;
                if (this.netWorthEl) this.netWorthEl.textContent = (totalAssets - data.debt).toFixed(2);

                this.isTickerOn = data.ticker_on || false;
                if (this.tickerStatusEl) this.tickerStatusEl.textContent = this.isTickerOn ? '[ON]' : '[OFF]';
                this.manageGameTickInterval();
            })
            .catch(error => {
                console.error("Error fetching player data:", error);
            });
    }
    
    gameTickIntervalId = null; 

    manageGameTickInterval() {
        if (this.gameTickIntervalId) {
            clearInterval(this.gameTickIntervalId);
            this.gameTickIntervalId = null;
        }
        if (this.isTickerOn) {
            this.gameTickIntervalId = setInterval(this.runGameTick, 5000);
        }
    }

    autoStartTicker() {
        fetch(`apps/msr]PHP]dec27]c3/php/get_player_data.php?player=${this.currentPlayerName}`) // Corrected path
            .then(response => response.json())
            .then(data => {
                this.isTickerOn = data.ticker_on || false;
                if (this.tickerStatusEl) this.tickerStatusEl.textContent = this.isTickerOn ? '[ON]' : '[OFF]';
                
                if (!this.isTickerOn) {
                    this.isTickerOn = true;
                    if (this.tickerStatusEl) this.tickerStatusEl.textContent = '[ON]';
                    this.manageGameTickInterval();
                    
                    fetch('apps/msr]PHP]dec27]c3/php/toggle_ticker.php', { // Corrected path
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ player: this.currentPlayerName })
                    }).catch(error => console.error("Error auto-enabling ticker:", error));
                } else {
                    this.manageGameTickInterval();
                }
            })
            .catch(error => {
                console.error("Error in autoStartTicker:", error);
                this.isTickerOn = true;
                if (this.tickerStatusEl) this.tickerStatusEl.textContent = '[ON]';
                this.manageGameTickInterval();
            });
    }

    updateNews() {
        fetch('apps/msr]PHP]dec27]c3/php/get_news.php') // Corrected path
            .then(response => response.json())
            .then(data => {
                if (this.newsContentEl) {
                    this.newsContentEl.innerHTML = '';
                    if (data.headlines && data.headlines.length > 0) {
                        data.headlines.forEach(line => {
                            const p = document.createElement('p');
                            p.textContent = line;
                            this.newsContentEl.appendChild(p);
                        });
                    } else {
                        this.newsContentEl.textContent = "No news to report.";
                    }
                }
            })
             .catch(error => {
                if (this.newsContentEl) this.newsContentEl.textContent = "Error loading news.";
                console.error("Error fetching news:", error);
            });
    }

    async runGameTurn() {
        if (this.turnOutputEl) this.turnOutputEl.textContent = `Advancing turn for ${this.currentPlayerName}...`;
        if (this.nextTurnBtn) this.nextTurnBtn.disabled = true;
        try {
            const response = await fetch('apps/msr]PHP]dec27]c3/php/game_turn.php', { method: 'POST' }); // Corrected path
            if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
            const data = await response.json();

            if (data.status === 'success') {
                if (this.turnOutputEl) this.turnOutputEl.textContent = `Turn advanced. Now ${data.next_player_name}'s turn.`;
                await this.updateGameInfo();
                await this.updatePlayerData();
            } else {
                throw new Error(data.message);
            }
        } catch (error) {
            if (this.turnOutputEl) this.turnOutputEl.textContent = `Error advancing turn: ${error.message}`;
            console.error("Error advancing turn:", error);
        } finally {
            if (this.nextTurnBtn) this.nextTurnBtn.disabled = false;
        }
    }

    async runGameTick() {
        try {
            const response = await fetch('apps/msr]PHP]dec27]c3/php/game_tick.php', { method: 'POST' }); // Corrected path
            const data = await response.json();
            
            if (data.status === 'success' && data.message.includes('New day processed')) {
                this.updateNews();
                this.updateGameInfo();
            }
        } catch (error) {
            console.error("Error calling game tick:", error);
        }
    }

    async toggleTicker() {
        try {
            const response = await fetch('apps/msr]PHP]dec27]c3/php/toggle_ticker.php', { // Corrected path
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ player: this.currentPlayerName })
            });
            const data = await response.json();

            if (data.status === 'success') {
                this.isTickerOn = data.new_ticker_status;
                if (this.tickerStatusEl) this.tickerStatusEl.textContent = this.isTickerOn ? '[ON]' : '[OFF]';
                this.manageGameTickInterval();
            }
        } catch (error) {
            console.error("Error toggling ticker:", error);
        }
    }
    
    closeGUI() {
        super.closeGUI();
        clearInterval(this.gameInfoInterval);
        clearInterval(this.realTimeDisplayInterval);
        clearInterval(this.newsUpdateInterval);
        if (this.gameTickIntervalId) {
            clearInterval(this.gameTickIntervalId);
        }
    }
    
    getDefaultWidth() {
        return 800; // MSR app specific width
    }
    
    getDefaultHeight() {
        return 600; // MSR app specific height
    }
}

// Make MSRApp available globally
window.MSRApp = MSRApp;