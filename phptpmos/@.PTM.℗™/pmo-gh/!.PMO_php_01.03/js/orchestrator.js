/**
 * orchestrator.js - The PMO Module Logic (The "Stage")
 * This script acts as the Fuzzpet Module.
 * It reads history, updates state.txt (Mirror), and writes view.txt (Sovereign View).
 * 
 * C-PARITY FIXES:
 * - Map rendering: proper borders matching C version format exactly
 * - Key processing: handle arrow keys (1000-1003) and WASD
 * - Response logic: match C version's event responses
 * - State sync: proper Mirror update with all fields
 */

class PMOModule {
    constructor(bridgePath) {
        this.bridgePath = bridgePath;
        this.state = {
            name: "Fuzzball",
            hunger: 50,
            happiness: 75,
            energy: 100,
            level: 1,
            status: "active",
            alive: true,
            last_key: "None",
            pos_x: 5,
            pos_y: 5,
            clock_turn: 0,
            clock_time: "08:00:00",
            response: ""
        };
        this.lastHistoryPos = 0;
        this.lastViewSize = 0;
        this.mapWidth = 20;
        this.mapHeight = 10;
        this.map = [];
        for(let y=0; y<this.mapHeight; y++) {
            this.map[y] = new Array(this.mapWidth).fill('.');
        }
    }

    async callBridge(action, path, data = null) {
        const url = `${this.bridgePath}?action=${action}&path=${path}`;
        const options = data ? { method: 'POST', body: data } : {};
        try {
            const res = await fetch(url, options);
            return await res.json();
        } catch (e) {
            console.error('Bridge error:', e);
            return { error: e.message };
        }
    }

    /**
     * Initialize state from files (DNA/Mirror)
     */
    async init() {
        // Load initial map
        const mapData = await this.callBridge('read', 'pieces/apps/fuzzpet_app/world/map.txt');
        if (mapData.content) {
            const lines = mapData.content.trim().split('\n');
            for(let y=0; y<Math.min(lines.length, this.mapHeight); y++) {
                for(let x=0; x<Math.min(lines[y].length, this.mapWidth); x++) {
                    this.map[y][x] = lines[y][x];
                }
            }
        }

        // Load fuzzpet mirror state
        const mirror = await this.callBridge('read', 'pieces/apps/fuzzpet_app/fuzzpet/state.txt');
        if (mirror.content) {
            mirror.content.split('\n').forEach(line => {
                const eq = line.indexOf('=');
                if (eq > 0) {
                    const k = line.substring(0, eq).trim();
                    const v = line.substring(eq + 1).trim();
                    if (this.state.hasOwnProperty(k)) {
                        this.state[k] = isNaN(v) ? v : parseInt(v);
                    } else {
                        this.state[k] = v;
                    }
                }
            });
        }
        
        // Load clock state
        const clock = await this.callBridge('read', 'pieces/apps/fuzzpet_app/clock/state.txt');
        if (clock.content) {
            clock.content.split('\n').forEach(line => {
                const parts = line.split(' ');
                if (parts.length >= 2) {
                    if (parts[0] === 'turn') this.state.clock_turn = parseInt(parts[1]);
                    if (parts[0] === 'time') this.state.clock_time = parts[1];
                }
            });
        }

        // Initialize view.txt if missing
        await this.writeSovereignView();
    }

    /**
     * The Pulse: Check for new input and update logic
     * C-PARITY: Reads raw numbers from history (matching C's fscanf "%d")
     */
    async pulse() {
        const history = await this.callBridge('read', 'pieces/apps/fuzzpet_app/fuzzpet/history.txt');
        if (history.content) {
            const lines = history.content.trim().split('\n').filter(l => l.trim() !== '');
            if (lines.length > this.lastHistoryPos) {
                let dirty = false;
                for (let i = this.lastHistoryPos; i < lines.length; i++) {
                    const keyLine = lines[i].trim();
                    // C-PARITY: Handle both formats:
                    // - Raw number: "50" (from inject_raw_key / UI clicks)
                    // - KEY_PRESSED: "KEY_PRESSED: 50" (from keyboard capture)
                    let key = keyLine;
                    if (keyLine.startsWith('KEY_PRESSED: ')) {
                        key = keyLine.substring(13).trim();
                    }
                    if (key && await this.processKey(key)) {
                        dirty = true;
                    }
                }
                this.lastHistoryPos = lines.length;
                if (dirty) {
                    await this.syncState();
                }
            }
        }
        await this.writeSovereignView();
    }

    /**
     * Process a key press (C-PARITY: handles both numeric codes and char codes)
     * Returns true if state changed
     */
    async processKey(key) {
        let keyCode = key;
        
        // Convert string keys to numeric codes if needed
        if (typeof key === 'string') {
            if (!isNaN(key)) {
                keyCode = parseInt(key);
            } else if (key === 'w' || key === 'W') keyCode = 119;
            else if (key === 's' || key === 'S') keyCode = 115;
            else if (key === 'a' || key === 'A') keyCode = 97;
            else if (key === 'd' || key === 'D') keyCode = 100;
            else if (key === '1000') keyCode = 1000;
            else if (key === '1001') keyCode = 1001;
            else if (key === '1002') keyCode = 1002;
            else if (key === '1003') keyCode = 1003;
            else keyCode = key.charCodeAt(0);
        }

        // Update debug display
        if (keyCode >= 32 && keyCode <= 126) this.state.last_key = `${keyCode} ('${String.fromCharCode(keyCode)}')`;
        else if (keyCode === 13 || keyCode === 10) this.state.last_key = "ENTER";
        else if (keyCode === 27) this.state.last_key = "ESC";
        else this.state.last_key = `CODE ${keyCode}`;

        let moved = false;
        let response = "";

        let nx = this.state.pos_x;
        let ny = this.state.pos_y;

        // Movement keys (C-PARITY: handles both arrow codes and WASD)
        if (keyCode === 1002 || keyCode === 119) { ny--; moved = true; }
        else if (keyCode === 1003 || keyCode === 115) { ny++; moved = true; }
        else if (keyCode === 1000 || keyCode === 97) { nx--; moved = true; }
        else if (keyCode === 1001 || keyCode === 100) { nx++; moved = true; } 
        
        if (moved) {
            if (nx >= 0 && nx < this.mapWidth && ny >= 0 && ny < this.mapHeight && this.map[ny][nx] !== '#' && this.map[ny][nx] !== 'R') {
                if (this.map[ny][nx] === 'T') {
                    this.state.happiness = Math.min(100, this.state.happiness + 10);
                    response = "You found a treasure! Happiness +10";
                }
                this.state.pos_x = nx;
                this.state.pos_y = ny;
                this.state.hunger = Math.min(100, this.state.hunger + 1);
                this.state.energy = Math.max(0, this.state.energy - 1);
                
                // Trigger clock updates via bridge if plugins exist, or just do it locally
                this.state.clock_turn++;
            } else {
                moved = false; // Blocked
            }
        }
        
        // Action keys (C-PARITY: virtual key mapping from buttons)
        if (keyCode === 50 || key === '2') { // '2' = Feed
            this.state.hunger = Math.max(0, this.state.hunger - 20);
            response = "Yum yum~ Feeling so happy! *tail wags wildly*";
        } else if (keyCode === 51 || key === '3') { // '3' = Play
            this.state.happiness = Math.min(100, this.state.happiness + 10);
            response = "Wheee~ Best playtime ever!";
        } else if (keyCode === 52 || key === '4') { // '4' = Sleep
            this.state.energy = Math.min(100, this.state.energy + 30);
            response = "Zzz... feeling recharged!";
        } else if (keyCode === 53 || key === '5') { // '5' = Evolve
            this.state.level++;
            response = "Wow! I evolved and got stronger!";
        } else if (keyCode === 54 || key === '6') { // '6' = End turn
            this.state.clock_turn++;
            response = "*time passes*";
        }

        if (response) this.state.response = response;
        else if (moved) this.state.response = "*moves around excitedly*";
        else if (keyCode !== 27 && keyCode !== 13) this.state.response = "*confused tilt* ...huh?";

        return moved || response !== "";
    }

    /**
     * Sync state to Mirror (state.txt)
     */
    async syncState() {
        const data = Object.entries(this.state)
            .filter(([k]) => k !== 'response') // Don't persist response
            .map(([k, v]) => `${k}=${v}`)
            .join('\n');
        await this.callBridge('write', 'pieces/apps/fuzzpet_app/fuzzpet/state.txt', data);
        
        // Pulse the view_changed marker
        await this.callBridge('append', 'pieces/apps/fuzzpet_app/fuzzpet/view_changed.txt', 'X\n');
    }

    /**
     * writeSovereignView() - The Module generates its own stage content.
     * C-PARITY: Exact format matching the C version's map rendering.
     */
    async writeSovereignView() {
        // Build map with proper borders (C-PARITY)
        let mapContent = "";
        
        for (let y = 0; y < this.mapHeight; y++) {
            let rowStr = "";
            for (let x = 0; x < this.mapWidth; x++) {
                if (x === this.state.pos_x && y === this.state.pos_y) rowStr += "@";
                else rowStr += this.map[y][x];
            }
            // Align to C format: "║  %-20s                                     ║"
            const paddedRow = rowStr.padEnd(20);
            mapContent += `║  ${paddedRow}                                     ║\n`;
        }
        
        const responseText = this.state.response || "*moves around excitedly*";
        const lastKeyDisplay = this.state.last_key || "None";
        
        // Build view content (C-PARITY format: Map first, then Status)
        const viewContent = 
            mapContent +
            `╠═══════════════════════════════════════════════════════════╣\n` +
            `║  [FUZZPET]: ${responseText.padEnd(45)} ║\n` +
            `║  DEBUG [Last Key]: ${lastKeyDisplay.padEnd(39)} ║`;

        await this.callBridge('write', 'pieces/apps/fuzzpet_app/fuzzpet/view.txt', viewContent);
    }
}

// Export for use in index.html
if (typeof window !== 'undefined') {
    window.PMOModule = PMOModule;
}
