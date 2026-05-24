// piece-manager.js - TPM Piece Management
// Manages piece state, history, and ledger following TPM architecture
// Now with localStorage fallback for offline persistence

class PieceManager {
    constructor() {
        this.pieces = new Map();
        this.piecesDir = 'pieces';
        this.phpApi = 'php/file_io.php';
        this.useLocalStorage = true; // Enable localStorage fallback
    }
    
    async loadPiece(pieceId) {
        // Try loading state.txt first (TPM Python pattern)
        try {
            const response = await fetch(`${this.phpApi}?action=read&path=${this.piecesDir}/${pieceId}/state.txt`);
            if (response.ok) {
                const text = await response.text();
                const state = this._parseKeyValueState(text);
                if (Object.keys(state).length > 0) {
                    const pieceData = { state };
                    this.pieces.set(pieceId, pieceData);
                    console.log(`[PieceManager] Loaded ${pieceId} from state.txt`);
                    return pieceData;
                }
            }
        } catch (error) {
            console.warn(`[PieceManager] state.txt load failed for ${pieceId}:`, error);
        }
        
        // Fallback to piece.json
        try {
            const response = await fetch(`${this.phpApi}?action=read&path=${this.piecesDir}/${pieceId}/piece.json`);
            if (response.ok) {
                const pieceData = await response.json();
                this.pieces.set(pieceId, pieceData);
                console.log(`[PieceManager] Loaded ${pieceId} from piece.json`);
                return pieceData;
            }
        } catch (error) {
            console.warn(`[PieceManager] piece.json load failed for ${pieceId}:`, error);
        }
        
        // Fallback to localStorage
        if (this.useLocalStorage) {
            const saved = localStorage.getItem(`piece_${pieceId}`);
            if (saved) {
                try {
                    const pieceData = { state: JSON.parse(saved) };
                    this.pieces.set(pieceId, pieceData);
                    console.log(`[PieceManager] Loaded ${pieceId} from localStorage`);
                    return pieceData;
                } catch (e) {
                    console.error(`[PieceManager] Failed to parse localStorage for ${pieceId}:`, e);
                }
            }
        }
        
        // Use default
        const pieceData = this.createDefaultPiece(pieceId);
        this.pieces.set(pieceId, pieceData);
        console.log(`[PieceManager] Using default state for ${pieceId}`);
        return pieceData;
    }
    
    _parseKeyValueState(text) {
        const state = {};
        const lines = text.trim().split('\n');
        for (const line of lines) {
            const trimmed = line.trim();
            if (!trimmed || trimmed.startsWith('#')) continue;
            
            // Parse "key value" format (e.g., "turn 8", "time 08:45:00")
            const spaceIndex = trimmed.indexOf(' ');
            if (spaceIndex > 0) {
                const key = trimmed.substring(0, spaceIndex).trim();
                const value = trimmed.substring(spaceIndex + 1).trim();
                
                // Try to convert to number if possible
                const numValue = Number(value);
                state[key] = isNaN(numValue) ? value : numValue;
            }
        }
        return state;
    }
    
    createDefaultPiece(pieceId) {
        const defaults = {
            'desktop': {
                state: {
                    visible: true,
                    context_menu_visible: false,
                    background: 'default'
                },
                methods: ['show_context_menu', 'hide_context_menu', 'add_window', 'remove_window', 'open_terminal', 'resize'],
                event_listeners: ['right_click', 'window_created', 'window_destroyed', 'context_menu_requested', 'desktop_resized'],
                responses: {
                    'right_click': 'Desktop right-clicked, showing context menu',
                    'window_created': 'Window added to desktop',
                    'window_destroyed': 'Window removed from desktop',
                    'desktop_resized': 'Desktop resized successfully'
                },
                relations: {
                    parent: 'root',
                    children: []
                }
            },
            'terminal': {
                state: {
                    visible: true,
                    content: '',
                    command_history: [],
                    current_command: '',
                    prompt: '$ '
                },
                methods: ['execute_command', 'display_message', 'display_error', 'clear_history', 'open', 'close', 'resize'],
                event_listeners: ['command_entered', 'command_executed', 'terminal_opened', 'terminal_closed', 'terminal_resized'],
                responses: {
                    'command_executed': 'Command processed in terminal',
                    'terminal_opened': 'Terminal window opened',
                    'terminal_closed': 'Terminal window closed',
                    'terminal_resized': 'Terminal window resized'
                },
                relations: {
                    parent: 'desktop',
                    children: []
                }
            },
            'keyboard': {
                state: {
                    visible: true,
                    focus: null,
                    input_buffer: ''
                },
                methods: ['handle_keydown', 'handle_keyup', 'set_focus', 'clear_focus'],
                event_listeners: ['keydown', 'keyup', 'focus_changed'],
                responses: {
                    'keydown': 'Key pressed',
                    'keyup': 'Key released',
                    'focus_changed': 'Focus changed to new element'
                },
                relations: {
                    parent: 'desktop',
                    children: []
                }
            }
        };
        
        return defaults[pieceId] || {
            state: { visible: true },
            methods: [],
            event_listeners: [],
            responses: {},
            relations: { parent: 'root', children: [] }
        };
    }
    
    async loadAllPieces() {
        const pieceIds = ['desktop', 'terminal', 'keyboard', 'fuzzpet', 'clock', 'world', 'counter', 'display'];
        for (const id of pieceIds) {
            await this.loadPiece(id);
        }
        
        if (typeof masterLedger !== 'undefined') {
            masterLedger.log('StateChange', `loaded ${this.pieces.size} pieces`, 'piece_manager');
        }
        
        console.log(`Piece Manager: Loaded ${this.pieces.size} pieces`);
    }
    
    getPieceState(pieceId, key = null) {
        const piece = this.pieces.get(pieceId);
        if (!piece) return null;
        
        if (key) {
            return piece.state[key];
        }
        return piece.state;
    }
    
    async updatePieceState(pieceId, newValues) {
        const piece = this.pieces.get(pieceId);
        if (!piece) return false;
        
        // Update in-memory state
        Object.assign(piece.state, newValues);
        
        // IMMEDIATE: Save to pieces/*.json file via PHP (TPM pattern - save on EVERY change)
        const success = await this._savePieceToPHP(pieceId, piece);
        
        if (success) {
            console.log(`[PieceManager] ✓ Saved ${pieceId} to pieces/${pieceId}/piece.json`);
        } else {
            console.error(`[PieceManager] ✗ Failed to save ${pieceId}`);
        }
        
        // Also save to localStorage as backup
        if (this.useLocalStorage) {
            try {
                localStorage.setItem(`piece_${pieceId}`, JSON.stringify(piece.state));
            } catch (e) {
                console.warn(`[PieceManager] localStorage backup failed:`, e);
            }
        }
        
        // Log to master ledger
        if (typeof masterLedger !== 'undefined') {
            for (const [key, value] of Object.entries(newValues)) {
                masterLedger.log('StateChange', `${pieceId} ${key} ${JSON.stringify(value)}`, 'piece_manager', true);
            }
        }
        
        return success;
    }
    
    async _savePieceToPHP(pieceId, piece) {
        try {
            const response = await fetch(`${this.phpApi}?action=write`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    path: `${this.piecesDir}/${pieceId}/piece.json`,
                    content: JSON.stringify(piece, null, 2),
                    format: 'json'
                })
            });
            
            const result = await response.json();
            console.log(`[PieceManager] piece.json save result:`, result);
            
            // ALSO save state.txt in key-value format (TPM Python pattern)
            if (result.success && piece.state) {
                const stateLines = [];
                for (const [key, value] of Object.entries(piece.state)) {
                    stateLines.push(`${key} ${value}`);
                }
                const stateContent = stateLines.join('\n') + '\n';
                
                console.log(`[PieceManager] Saving ${pieceId} state.txt:`, stateContent.trim());
                
                // Save state.txt file
                const stateResponse = await fetch(`${this.phpApi}?action=write`, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        path: `${this.piecesDir}/${pieceId}/state.txt`,
                        content: stateContent,
                        format: 'keyvalue'
                    })
                });
                
                const stateResult = await stateResponse.json();
                console.log(`[PieceManager] state.txt save result:`, stateResult);
                
                if (!stateResult.success) {
                    console.error(`[PieceManager] Failed to save state.txt for ${pieceId}`);
                }
            }
            
            return result.success === true;
        } catch (error) {
            console.error(`[PieceManager] PHP save error:`, error);
            return false;
        }
    }
    
    async fireEvent(pieceId, eventType, eventData = {}) {
        const piece = this.pieces.get(pieceId);
        if (!piece) return;
        
        // Log to piece history
        await this.appendToHistory(pieceId, {
            timestamp: new Date().toISOString(),
            event: eventType,
            data: eventData
        });
        
        // Execute event listeners
        const listeners = piece.event_listeners || [];
        if (listeners.includes(eventType)) {
            const handler = this[`on${eventType.charAt(0).toUpperCase() + eventType.slice(1)}`];
            if (handler) {
                handler.call(this, pieceId, eventData);
            }
        }
        
        // Log response
        const response = piece.responses[eventType] || piece.responses['default'];
        if (response && typeof masterLedger !== 'undefined') {
            masterLedger.log('EventFire', `${eventType} on ${pieceId}: ${response}`, 'piece_manager');
        }
    }
    
    async appendToHistory(pieceId, entry) {
        try {
            await fetch(`${this.phpApi}?action=append`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    path: `${this.piecesDir}/${pieceId}/history.json`,
                    data: entry
                })
            });
        } catch (error) {
            console.error(`Failed to append to ${pieceId} history:`, error);
        }
    }
    
    getPiece(pieceId) {
        return this.pieces.get(pieceId);
    }
    
    hasPiece(pieceId) {
        return this.pieces.has(pieceId);
    }
    
    getAllPieces() {
        return Array.from(this.pieces.entries());
    }
}

// Global instance
const pieceManager = new PieceManager();
