// master-ledger.js - Central Audit Trail for TPM
// Logs all state changes and events for full auditability
// Now with localStorage backup for persistence

class MasterLedger {
    constructor() {
        this.entries = [];
        this.ledgerFile = 'pieces/master_ledger/ledger.json';
        this.phpApi = 'php/file_io.php';
        this.maxEntries = 1000; // Keep last 1000 entries in memory
        this.autoSave = true;
        this.saveInterval = 5000; // Auto-save every 5 seconds
        this.useLocalStorage = true; // Enable localStorage backup
    }
    
    async init() {
        await this.load();
        
        // Start auto-save if enabled
        if (this.autoSave) {
            setInterval(() => this.save(), this.saveInterval);
        }
        
        console.log(`[MasterLedger] Initialized with ${this.entries.length} entries`);
    }
    
    async load() {
        // Try PHP first
        try {
            const response = await fetch(`${this.phpApi}?action=read&path=${this.ledgerFile}`);
            if (response.ok) {
                const data = await response.json();
                this.entries = Array.isArray(data) ? data : [];
                // Backup to localStorage
                this.saveToLocalStorage();
                console.log(`[MasterLedger] Loaded ${this.entries.length} entries from PHP`);
                return;
            }
        } catch (error) {
            console.warn('[MasterLedger] PHP load failed:', error);
        }
        
        // Fallback to localStorage
        if (this.useLocalStorage) {
            const saved = localStorage.getItem('master_ledger');
            if (saved) {
                try {
                    this.entries = JSON.parse(saved);
                    console.log(`[MasterLedger] Loaded ${this.entries.length} entries from localStorage`);
                } catch (e) {
                    console.error('[MasterLedger] Failed to parse localStorage:', e);
                    this.entries = [];
                }
            }
        }
    }
    
    saveToLocalStorage() {
        if (!this.useLocalStorage) return;
        try {
            localStorage.setItem('master_ledger', JSON.stringify(this.entries));
        } catch (e) {
            console.warn('[MasterLedger] localStorage save failed:', e);
        }
    }
    
    log(entryType, message, source, immediate = false) {
        const entry = {
            timestamp: new Date().toISOString(),
            type: entryType,
            message: message,
            source: source
        };
        
        this.entries.push(entry);
        
        // Trim old entries if over limit
        if (this.entries.length > this.maxEntries) {
            this.entries = this.entries.slice(this.entries.length - this.maxEntries);
        }
        
        // IMMEDIATE: Save to localStorage (synchronous, always works)
        this.saveToLocalStorage();
        
        // ASYNC: Try to save to PHP
        if (immediate) {
            // Force immediate save
            this.saveBatch();
        } else if (this.autoSave) {
            this.queueSave(entry);
        }
        
        // Also log to console for debugging
        console.log(`[LEDGER:${entryType}] ${message} (Source: ${source})`);
        
        return entry;
    }
    
    saveQueue = [];
    saveTimeout = null;
    
    queueSave(entry) {
        this.saveQueue.push(entry);
        
        if (this.saveTimeout) {
            clearTimeout(this.saveTimeout);
        }
        
        this.saveTimeout = setTimeout(() => {
            this.saveBatch();
        }, 1000);
    }
    
    async saveBatch() {
        if (this.saveQueue.length === 0) return;
        
        const entriesToSave = [...this.saveQueue];
        this.saveQueue = [];
        
        try {
            await fetch(`${this.phpApi}?action=append`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    path: this.ledgerFile,
                    data: entriesToSave.length === 1 ? entriesToSave[0] : entriesToSave
                })
            });
        } catch (error) {
            console.error('Failed to save ledger entries:', error);
            // Re-queue failed entries
            this.saveQueue.unshift(...entriesToSave);
        }
    }
    
    async save() {
        if (this.saveQueue.length > 0) {
            await this.saveBatch();
        }
    }
    
    getEntries(filters = {}) {
        return this.entries.filter(entry => {
            if (filters.type && entry.type !== filters.type) return false;
            if (filters.source && entry.source !== filters.source) return false;
            if (filters.after && entry.timestamp < filters.after) return false;
            if (filters.before && entry.timestamp > filters.before) return false;
            if (filters.search) {
                const searchLower = filters.search.toLowerCase();
                return entry.message.toLowerCase().includes(searchLower) ||
                       entry.type.toLowerCase().includes(searchLower) ||
                       entry.source.toLowerCase().includes(searchLower);
            }
            return true;
        });
    }
    
    getLatest(count = 10) {
        return this.entries.slice(-count);
    }
    
    getEntryCount() {
        return this.entries.length;
    }
    
    clear() {
        this.entries = [];
        this.log('System', 'Ledger cleared', 'master_ledger');
    }
    
    export(format = 'json') {
        if (format === 'json') {
            return JSON.stringify(this.entries, null, 2);
        } else if (format === 'csv') {
            const header = 'timestamp,type,message,source\n';
            const rows = this.entries.map(e => 
                `${e.timestamp},${e.type},"${e.message.replace(/"/g, '""')}",${e.source}`
            ).join('\n');
            return header + rows;
        } else if (format === 'text') {
            return this.entries.map(e => 
                `[${e.timestamp}] ${e.type}: ${e.message} (Source: ${e.source})`
            ).join('\n');
        }
        return null;
    }
    
    async exportToFile(format = 'json', filename = 'ledger_export') {
        const content = this.export(format);
        const blob = new Blob([content], { type: 'text/plain' });
        const url = URL.createObjectURL(blob);
        
        const a = document.createElement('a');
        a.href = url;
        a.download = `${filename}.${format}`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
    }
    
    // Query helpers
    getStateChanges(source = null) {
        const filters = { type: 'StateChange' };
        if (source) filters.source = source;
        return this.getEntries(filters);
    }
    
    getEvents(source = null) {
        const filters = { type: 'EventFire' };
        if (source) filters.source = source;
        return this.getEntries(filters);
    }
    
    getTimeline(component, startTime, endTime) {
        return this.getEntries({
            after: startTime,
            before: endTime
        }).filter(e => e.message.includes(component));
    }
    
    search(query) {
        return this.getEntries({ search: query });
    }
}

// Global instance
const masterLedger = new MasterLedger();
