// fuzzpet.js - FuzzPet App for PyOS-TPM Web
// A virtual pet game following TPM architecture

function createFuzzPetApp() {
    const content = `
        <div class="fuzzpet-container" style="padding: 15px; font-family: 'Courier New', monospace;">
            <div style="text-align: center; margin-bottom: 10px;">
                <h2 style="color: #e67e22; margin: 0;">🐾 FuzzPet</h2>
                <p style="color: #7f8c8d; font-size: 11px; margin: 5px 0;">A TPM Virtual Pet Game</p>
            </div>
            
            <!-- Game Display -->
            <div id="fuzzpet-display" style="
                background: #0d0d1a;
                border: 3px solid #e67e22;
                border-radius: 8px;
                padding: 10px;
                margin-bottom: 10px;
                font-family: 'Courier New', monospace;
                font-size: 14px;
                line-height: 1.4;
                color: #00ff00;
                white-space: pre;
                overflow: hidden;
            "></div>
            
            <!-- Stats Bar -->
            <div style="
                display: grid;
                grid-template-columns: repeat(4, 1fr);
                gap: 5px;
                margin-bottom: 10px;
                font-size: 11px;
            ">
                <div style="background: #e74c3c; padding: 5px; border-radius: 4px; text-align: center; color: white;">
                    ❤️ Hunger: <span id="stat-hunger">50</span>
                </div>
                <div style="background: #9b59b6; padding: 5px; border-radius: 4px; text-align: center; color: white;">
                    😊 Happy: <span id="stat-happiness">50</span>
                </div>
                <div style="background: #3498db; padding: 5px; border-radius: 4px; text-align: center; color: white;">
                    ⚡ Energy: <span id="stat-energy">50</span>
                </div>
                <div style="background: #f1c40f; padding: 5px; border-radius: 4px; text-align: center; color: black;">
                    ⭐ Level: <span id="stat-level">1</span>
                </div>
            </div>
            
            <!-- Turn/Time Display -->
            <div style="
                display: flex;
                justify-content: space-between;
                background: #2c3e50;
                padding: 8px 12px;
                border-radius: 4px;
                margin-bottom: 10px;
                font-size: 12px;
                color: #ecf0f1;
            ">
                <span>📅 Turn: <strong id="turn-display" style="color: #3498db;">0</strong></span>
                <span>🕐 Time: <strong id="time-display" style="color: #27ae60;">08:00:00</strong></span>
            </div>
            
            <!-- Controls -->
            <div style="
                display: grid;
                grid-template-columns: repeat(3, 1fr);
                gap: 5px;
                margin-bottom: 10px;
            ">
                <button onclick="fuzzpetGame.move('up')" style="
                    padding: 8px;
                    background: linear-gradient(to bottom, #27ae60, #1e8449);
                    color: white;
                    border: none;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 16px;
                " title="Move Up (W)">⬆️ W</button>
                <button onclick="fuzzpetGame.move('down')" style="
                    padding: 8px;
                    background: linear-gradient(to bottom, #27ae60, #1e8449);
                    color: white;
                    border: none;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 16px;
                " title="Move Down (S)">⬇️ S</button>
                <button onclick="fuzzpetGame.move('left')" style="
                    padding: 8px;
                    background: linear-gradient(to bottom, #27ae60, #1e8449);
                    color: white;
                    border: none;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 16px;
                " title="Move Left (A)">⬅️ A</button>
                
                <button onclick="fuzzpetGame.move('right')" style="
                    padding: 8px;
                    background: linear-gradient(to bottom, #27ae60, #1e8449);
                    color: white;
                    border: none;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 16px;
                " title="Move Right (D)">➡️ D</button>
                <button onclick="fuzzpetGame.feed()" style="
                    padding: 8px;
                    background: linear-gradient(to bottom, #e67e22, #d35400);
                    color: white;
                    border: none;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 12px;
                " title="Feed Pet (1)">🍖 Feed (1)</button>
                <button onclick="fuzzpetGame.play()" style="
                    padding: 8px;
                    background: linear-gradient(to bottom, #9b59b6, #8e44ad);
                    color: white;
                    border: none;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 12px;
                " title="Play with Pet (2)">🎾 Play (2)</button>
                
                <button onclick="fuzzpetGame.sleep()" style="
                    padding: 8px;
                    background: linear-gradient(to bottom, #3498db, #2980b9);
                    color: white;
                    border: none;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 12px;
                " title="Pet Sleeps (3)">💤 Sleep (3)</button>
                <button onclick="fuzzpetGame.evolve()" style="
                    padding: 8px;
                    background: linear-gradient(to bottom, #f1c40f, #f39c12);
                    color: black;
                    border: none;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 12px;
                " title="Evolve Pet (4)">✨ Evolve (4)</button>
                <button onclick="fuzzpetGame.endTurn()" style="
                    padding: 8px;
                    background: linear-gradient(to bottom, #e74c3c, #c0392b);
                    color: white;
                    border: none;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 12px;
                " title="End Turn (5/6)">⏭️ Turn (5)</button>
            </div>
            
            <!-- Help Text -->
            <div style="
                background: #34495e;
                padding: 8px;
                border-radius: 4px;
                font-size: 10px;
                color: #bdc3c7;
                text-align: center;
            ">
                <strong>Controls:</strong> WASD/Arrows move | 1-5 actions | Auto-saves every action
            </div>
            
            <!-- Message Area -->
            <div id="fuzzpet-message" style="
                margin-top: 10px;
                padding: 8px;
                background: #1a1a2e;
                border-radius: 4px;
                font-size: 11px;
                color: #00ffff;
                min-height: 35px;
            ">Welcome to FuzzPet! Use WASD to move your pet (@) around.</div>
        </div>
    `;
    
    const win = windowManager.createWindow('FuzzPet', content, 500, 650, {
        className: 'fuzzpet-window'
    });
    
    // Initialize game after window is added
    setTimeout(() => {
        fuzzpetGame.init(win);
    }, 50);
    
    masterLedger.log('EventFire', 'FuzzPet app launched', 'desktop');
}

// FuzzPet Game Logic
const fuzzpetGame = {
    window: null,
    sessionId: 'default', // Can be changed for multiple save slots
    petState: {
        pos_x: 5,
        pos_y: 5,
        name: 'FuzzPet',
        hunger: 50,
        happiness: 50,
        energy: 50,
        level: 1
    },
    clockState: {
        turn: 0,
        time: '08:00:00'
    },
    worldState: {
        width: 10,
        height: 10,
        boundaries: { min_x: 1, max_x: 8, min_y: 1, max_y: 8 }
    },
    
    init(win) {
        this.window = win;
        this.loadState();
        this.render();
        this.updateStats();
        this.setupKeyboard();
        masterLedger.log('StateChange', 'FuzzPet game initialized', 'fuzzpet');
    },
    
    loadState() {
        // Load from pieces/*.json via piece manager
        if (typeof pieceManager !== 'undefined') {
            const petState = pieceManager.getPieceState('fuzzpet');
            if (petState) {
                this.petState = { ...this.petState, ...petState };
            }
            
            const clockState = pieceManager.getPieceState('clock');
            if (clockState) {
                this.clockState = { ...this.clockState, ...clockState };
            }
            
            console.log('[FuzzPet] Loaded state from pieces/');
        }
    },
    
    saveState() {
        // Save to pieces/*.json via piece manager (TPM pattern)
        if (typeof pieceManager !== 'undefined') {
            pieceManager.updatePieceState('fuzzpet', this.petState);
            pieceManager.updatePieceState('clock', this.clockState);
        }
    },
    
    startAutoSave() {
        // Auto-save every 30 seconds
        this.autoSaveTimer = setInterval(() => {
            this.saveGame();
        }, 30000);
        console.log('[FuzzPet] Auto-save started (every 30s)');
    },
    
    stopAutoSave() {
        if (this.autoSaveTimer) {
            clearInterval(this.autoSaveTimer);
            this.autoSaveTimer = null;
            console.log('[FuzzPet] Auto-save stopped');
        }
    },
    
    render() {
        const display = this.window.querySelector('#fuzzpet-display');
        if (!display) return;
        
        // Create the map grid
        let grid = [];
        for (let y = 0; y < this.worldState.height; y++) {
            let row = '';
            for (let x = 0; x < this.worldState.width; x++) {
                if (y === 0 || y === this.worldState.height - 1 || 
                    x === 0 || x === this.worldState.width - 1) {
                    row += '#';
                } else if (x === this.petState.pos_x && y === this.petState.pos_y) {
                    row += '@';
                } else {
                    row += ' ';
                }
            }
            grid.push(row);
        }
        
        // Display as ASCII art
        display.textContent = grid.join('\n');
        
        // Update position display
        if (this.window.querySelector('#pet-position')) {
            this.window.querySelector('#pet-position').textContent = 
                `(${this.petState.pos_x}, ${this.petState.pos_y})`;
        }
    },
    
    updateStats() {
        if (this.window) {
            this.window.querySelector('#stat-hunger').textContent = this.petState.hunger;
            this.window.querySelector('#stat-happiness').textContent = this.petState.happiness;
            this.window.querySelector('#stat-energy').textContent = this.petState.energy;
            this.window.querySelector('#stat-level').textContent = this.petState.level;
            this.window.querySelector('#turn-display').textContent = this.clockState.turn;
            this.window.querySelector('#time-display').textContent = this.clockState.time;
        }
    },
    
    showMessage(msg) {
        const msgEl = this.window.querySelector('#fuzzpet-message');
        if (msgEl) {
            msgEl.textContent = msg;
        }
    },
    
    move(direction) {
        let newX = this.petState.pos_x;
        let newY = this.petState.pos_y;
        
        switch(direction) {
            case 'up': newY--; break;
            case 'down': newY++; break;
            case 'left': newX--; break;
            case 'right': newX++; break;
        }
        
        // Check boundaries
        if (newX >= this.worldState.boundaries.min_x && 
            newX <= this.worldState.boundaries.max_x &&
            newY >= this.worldState.boundaries.min_y && 
            newY <= this.worldState.boundaries.max_y) {
            this.petState.pos_x = newX;
            this.petState.pos_y = newY;
            this.showMessage(`Moved ${direction} to (${newX}, ${newY})`);
            this.render();
            this.saveState(); // TPM: Save on EVERY state change
            
            // Log to ledger
            masterLedger.log('MovementEvent', `Move ${direction} to (${newX}, ${newY})`, 'fuzzpet', true);
        } else {
            this.showMessage("Can't move there! There's a wall.");
        }
    },
    
    feed() {
        if (this.petState.hunger > 20) {
            this.petState.hunger -= 20;
            this.petState.happiness += 5;
            this.showMessage('Yum! FuzzPet enjoyed the meal. 🍖');
        } else {
            this.showMessage("FuzzPet isn't hungry right now.");
        }
        this.clampStats();
        this.updateStats();
        this.saveState(); // TPM: Save on EVERY state change
        masterLedger.log('CommandExecuted', 'Feed pet', 'fuzzpet', true);
    },
    
    play() {
        if (this.petState.energy > 15) {
            this.petState.happiness += 15;
            this.petState.energy -= 10;
            this.petState.hunger += 5;
            this.showMessage('Wheee! FuzzPet loves playing! 🎾');
        } else {
            this.showMessage("FuzzPet is too tired to play.");
        }
        this.clampStats();
        this.updateStats();
        this.saveState(); // TPM: Save on EVERY state change
        masterLedger.log('CommandExecuted', 'Play with pet', 'fuzzpet', true);
    },
    
    sleep() {
        if (this.petState.energy < 80) {
            this.petState.energy += 30;
            this.petState.hunger += 10;
            this.showMessage('Zzz... FuzzPet is resting. 💤');
        } else {
            this.showMessage("FuzzPet isn't tired yet.");
        }
        this.clampStats();
        this.updateStats();
        this.saveState(); // TPM: Save on EVERY state change
        masterLedger.log('CommandExecuted', 'Pet sleeps', 'fuzzpet', true);
    },
    
    evolve() {
        const reqHunger = this.petState.hunger <= 30;
        const reqHappy = this.petState.happiness >= 70;
        const reqEnergy = this.petState.energy >= 50;
        
        if (reqHunger && reqHappy && reqEnergy) {
            this.petState.level++;
            this.petState.happiness = 100;
            this.showMessage(`🎉 Amazing! FuzzPet evolved to Level ${this.petState.level}!`);
        } else {
            this.showMessage(`Need: Hunger≤30, Happy≥70, Energy≥50\nCurrent: H:${this.petState.hunger} H:${this.petState.happiness} E:${this.petState.energy}`);
        }
        this.clampStats();
        this.updateStats();
        this.saveState(); // TPM: Save on EVERY state change
        masterLedger.log('CommandExecuted', 'Pet evolve attempt', 'fuzzpet', true);
    },
    
    endTurn() {
        // Increment turn
        this.clockState.turn++;
        
        // Increment time by 5 minutes
        const [hours, minutes, seconds] = this.clockState.time.split(':').map(Number);
        let newMinutes = minutes + 5;
        let newHours = hours;
        
        if (newMinutes >= 60) {
            newMinutes -= 60;
            newHours++;
            if (newHours >= 24) {
                newHours = 0;
            }
        }
        
        this.clockState.time = `${String(newHours).padStart(2, '0')}:${String(newMinutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
        
        // Natural stat changes
        this.petState.hunger += 2;
        this.petState.energy -= 1;
        
        this.showMessage(`Turn ${this.clockState.turn} | Time: ${this.clockState.time}`);
        this.clampStats();
        this.updateStats();
        this.saveState(); // TPM: Save on EVERY state change
        
        masterLedger.log('StateChange', `Turn ended: ${this.clockState.turn} | ${this.clockState.time}`, 'fuzzpet', true);
    },
    
    clampStats() {
        this.petState.hunger = Math.max(0, Math.min(100, this.petState.hunger));
        this.petState.happiness = Math.max(0, Math.min(100, this.petState.happiness));
        this.petState.energy = Math.max(0, Math.min(100, this.petState.energy));
    },
    
    setupKeyboard() {
        document.addEventListener('keydown', (e) => {
            // Only handle if FuzzPet window is focused/active
            if (!this.window || this.window.style.display === 'none') return;
            
            const key = e.key.toLowerCase();
            
            switch(key) {
                case 'w':
                case 'arrowup':
                    e.preventDefault();
                    this.move('up');
                    break;
                case 's':
                case 'arrowdown':
                    e.preventDefault();
                    this.move('down');
                    break;
                case 'a':
                case 'arrowleft':
                    e.preventDefault();
                    this.move('left');
                    break;
                case 'd':
                case 'arrowright':
                    e.preventDefault();
                    this.move('right');
                    break;
                case '1':
                    e.preventDefault();
                    this.feed();
                    break;
                case '2':
                    e.preventDefault();
                    this.play();
                    break;
                case '3':
                    e.preventDefault();
                    this.sleep();
                    break;
                case '4':
                    e.preventDefault();
                    this.evolve();
                    break;
                case '5':
                case '6':
                    e.preventDefault();
                    this.endTurn();
                    break;
            }
        });
    }
};
