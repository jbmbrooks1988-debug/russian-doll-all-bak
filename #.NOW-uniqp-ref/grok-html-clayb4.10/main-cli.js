// main-cli.js - Main CLI Application

class CLIApp {
    constructor() {
        this.apps = {
            'settings': 'settings.js',
            'profiles': 'profiles.js', 
            'wallet': 'wallet.js',
            'store': 'store.js',
            'msr': 'msr.js'
        };
        
        this.commands = {
            'list': this.listApps.bind(this),
            'help': this.showHelp.bind(this)
        };
        
        // Initialize command input
        this.init();
    }
    
    init() {
        console.log('Welcome to JS CLI App!');
        console.log('Type "list" to see available apps or "help" for more information.');
        
        // In a real implementation, you would set up a readline interface
        // For now, we'll just log the available commands
    }
    
    listApps() {
        console.log('Available apps:');
        for (const [name, file] of Object.entries(this.apps)) {
            console.log(`- ${name}`);
        }
    }
    
    showHelp() {
        console.log('Available commands:');
        console.log('- list: Show available apps');
        console.log('- help: Show this help message');
        console.log('App-specific commands will run their respective .js files');
    }
    
    executeCommand(cmd) {
        if (this.commands[cmd]) {
            this.commands[cmd]();
        } else if (this.apps[cmd]) {
            console.log(`Running app: ${cmd} from ${this.apps[cmd]}`);
            // In a real implementation, this would dynamically load and execute the app's JS file
        } else {
            console.log(`Command '${cmd}' not recognized. Type 'help' for available commands.`);
        }
    }
}

// Initialize the CLI app
const cli = new CLIApp();

// Example usage (in a real app, this would come from user input)
// cli.executeCommand('list');