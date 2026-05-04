// plugin-interface.js - Base Plugin Interface for PyOS-TPM
// All plugins must extend this base class

class PluginInterface {
    constructor(engine) {
        this.engine = engine;
        this.enabled = true;
        this.name = this.constructor.name;
        this.version = '1.0.0';
        this.config = {};
    }
    
    /**
     * Called when the plugin is loaded
     * Override this in your plugin
     */
    initialize() {
        console.log(`[${this.name}] Plugin initialized`);
    }
    
    /**
     * Called when the plugin is activated
     * Override this in your plugin
     */
    activate() {
        this.enabled = true;
        console.log(`[${this.name}] Plugin activated`);
    }
    
    /**
     * Called when the plugin is deactivated
     * Override this in your plugin
     */
    deactivate() {
        this.enabled = false;
        console.log(`[${this.name}] Plugin deactivated`);
    }
    
    /**
     * Handle input events
     * @param {string} key - The key pressed
     * @param {object} event - The original event object
     * @returns {boolean} - Whether the event was handled
     */
    handleInput(key, event) {
        return false;
    }
    
    /**
     * Handle keyboard events
     * @param {KeyboardEvent} event 
     * @returns {boolean}
     */
    handleKeydown(event) {
        return false;
    }
    
    /**
     * Handle mouse events
     * @param {MouseEvent} event
     * @returns {boolean}
     */
    handleMouse(event) {
        return false;
    }
    
    /**
     * Update plugin state (called every frame)
     * @param {number} dt - Delta time in seconds
     */
    update(dt) {
        // Override in plugin
    }
    
    /**
     * Render plugin content
     * Override this in your plugin if it needs custom rendering
     */
    render(container) {
        // Override in plugin
    }
    
    /**
     * Get plugin configuration
     * @param {string} key - Config key
     * @param {*} defaultValue - Default value if key doesn't exist
     */
    getConfig(key, defaultValue = null) {
        return this.config[key] !== undefined ? this.config[key] : defaultValue;
    }
    
    /**
     * Set plugin configuration
     * @param {string} key - Config key
     * @param {*} value - Config value
     */
    setConfig(key, value) {
        this.config[key] = value;
    }
    
    /**
     * Save plugin configuration
     */
    async saveConfig() {
        if (typeof pieceManager !== 'undefined') {
            await pieceManager.updatePieceState('plugins', {
                [this.name]: this.config
            });
        }
    }
    
    /**
     * Load plugin configuration
     */
    async loadConfig() {
        if (typeof pieceManager !== 'undefined') {
            const pluginsState = pieceManager.getPieceState('plugins');
            if (pluginsState && pluginsState[this.name]) {
                this.config = pluginsState[this.name];
            }
        }
    }
    
    /**
     * Log a message to the master ledger
     * @param {string} type - Log type
     * @param {string} message - Log message
     */
    log(type, message) {
        if (typeof masterLedger !== 'undefined') {
            masterLedger.log(type, message, this.name);
        } else {
            console.log(`[${this.name}:${type}] ${message}`);
        }
    }
    
    /**
     * Get the plugin status
     * @returns {object}
     */
    getStatus() {
        return {
            name: this.name,
            version: this.version,
            enabled: this.enabled,
            config: this.config
        };
    }
    
    /**
     * Cleanup when plugin is unloaded
     */
    destroy() {
        this.enabled = false;
        console.log(`[${this.name}] Plugin destroyed`);
    }
}

// Plugin Registry for managing multiple plugins
class PluginRegistry {
    constructor() {
        this.plugins = new Map();
        this.engine = null;
    }
    
    setEngine(engine) {
        this.engine = engine;
    }
    
    register(name, pluginClass) {
        if (!this.engine) {
            console.error('PluginRegistry: Engine not set');
            return false;
        }
        
        const plugin = new pluginClass(this.engine);
        this.plugins.set(name, plugin);
        console.log(`[PluginRegistry] Registered plugin: ${name}`);
        return true;
    }
    
    async load(name) {
        const plugin = this.plugins.get(name);
        if (!plugin) {
            console.error(`[PluginRegistry] Plugin not found: ${name}`);
            return false;
        }
        
        await plugin.loadConfig();
        plugin.initialize();
        return true;
    }
    
    async loadAll() {
        for (const [name, plugin] of this.plugins) {
            await this.load(name);
        }
    }
    
    activate(name) {
        const plugin = this.plugins.get(name);
        if (plugin) {
            plugin.activate();
            return true;
        }
        return false;
    }
    
    deactivate(name) {
        const plugin = this.plugins.get(name);
        if (plugin) {
            plugin.deactivate();
            return true;
        }
        return false;
    }
    
    get(name) {
        return this.plugins.get(name);
    }
    
    getAll() {
        return Array.from(this.plugins.entries());
    }
    
    update(dt) {
        for (const [name, plugin] of this.plugins) {
            if (plugin.enabled) {
                plugin.update(dt);
            }
        }
    }
    
    destroy() {
        for (const [name, plugin] of this.plugins) {
            plugin.destroy();
        }
        this.plugins.clear();
    }
}

// Global plugin registry
const pluginRegistry = new PluginRegistry();
