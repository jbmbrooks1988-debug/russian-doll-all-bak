// counter-plugin.js - Counter App Plugin for PyOS-TPM Web
// Follows TPM architecture: Input → History → Ledger → Process → Response

class CounterPlugin extends PluginInterface {
    constructor(engine) {
        super(engine);
        this.name = 'CounterPlugin';
        this.count = 0;
        this.history = [];
        this.listeners = new Map();
    }
    
    async initialize() {
        console.log(`[${this.name}] Initializing...`);
        
        // Load saved state from piece
        const savedState = pieceManager.getPieceState('counter', 'count');
        if (savedState !== null) {
            this.count = savedState;
            console.log(`[${this.name}] Loaded count: ${this.count}`);
        }
        
        // Register event listeners
        this.setupEventListeners();
        
        this.log('StateChange', 'Counter plugin initialized');
    }
    
    setupEventListeners() {
        // Listen for counter state changes
        pieceManager.updatePieceState('counter', { initialized: true });
    }
    
    increment() {
        const oldValue = this.count;
        this.count++;
        
        // Log to history
        this.addToHistory('increment', { oldValue, newValue: this.count });
        
        // Update piece state
        pieceManager.updatePieceState('counter', { count: this.count });
        
        // Log to ledger
        this.log('StateChange', `Counter: ${oldValue} → ${this.count}`);
        
        // Notify listeners
        this.notify('change', { action: 'increment', value: this.count });
        
        return this.count;
    }
    
    decrement() {
        const oldValue = this.count;
        this.count--;
        
        // Log to history
        this.addToHistory('decrement', { oldValue, newValue: this.count });
        
        // Update piece state
        pieceManager.updatePieceState('counter', { count: this.count });
        
        // Log to ledger
        this.log('StateChange', `Counter: ${oldValue} → ${this.count}`);
        
        // Notify listeners
        this.notify('change', { action: 'decrement', value: this.count });
        
        return this.count;
    }
    
    reset() {
        const oldValue = this.count;
        this.count = 0;
        
        // Log to history
        this.addToHistory('reset', { oldValue, newValue: this.count });
        
        // Update piece state
        pieceManager.updatePieceState('counter', { count: this.count });
        
        // Log to ledger
        this.log('StateChange', `Counter reset: ${oldValue} → ${this.count}`);
        
        // Notify listeners
        this.notify('change', { action: 'reset', value: this.count });
        
        return this.count;
    }
    
    getCount() {
        return this.count;
    }
    
    addToHistory(action, data) {
        const entry = {
            timestamp: new Date().toISOString(),
            action: action,
            data: data
        };
        
        this.history.push(entry);
        
        // Persist to piece history
        pieceManager.appendToHistory('counter', entry);
    }
    
    on(event, callback) {
        if (!this.listeners.has(event)) {
            this.listeners.set(event, []);
        }
        this.listeners.get(event).push(callback);
    }
    
    off(event, callback) {
        if (this.listeners.has(event)) {
            const callbacks = this.listeners.get(event);
            const index = callbacks.indexOf(callback);
            if (index > -1) {
                callbacks.splice(index, 1);
            }
        }
    }
    
    notify(event, data) {
        if (this.listeners.has(event)) {
            this.listeners.get(event).forEach(callback => {
                try {
                    callback(data);
                } catch (e) {
                    console.error(`[${this.name}] Error in event callback:`, e);
                }
            });
        }
    }
}

// Global instance
const counterPlugin = new CounterPlugin();

// Auto-initialize when pieceManager is ready
if (typeof pieceManager !== 'undefined') {
    counterPlugin.initialize();
}
