// JavaScript LLM Terminal Application with PHP Backend Integration
class LLMTerminal {
    constructor() {
        this.terminal = document.getElementById('terminal');
        this.commandInput = document.getElementById('command-input');
        this.submitBtn = document.getElementById('submit-btn');
        
        this.llmModel = null;
        this.apiClient = null;
        this.modelLoaded = false;
        
        this.setupEventListeners();
        this.initializeModel();
        this.initializeApiClient();
    }
    
    async initializeModel() {
        try {
            this.appendTerminalMessage('Loading LLM model...', 'system-message');
            
            // Create and initialize the LLM model
            this.llmModel = new LLMModel();
            await this.llmModel.loadModel();
            
            this.modelLoaded = true;
            this.appendTerminalMessage('LLM model loaded successfully!', 'system-message');
        } catch (error) {
            this.appendTerminalMessage(`Error loading model: ${error.message}`, 'system-message');
        }
    }
    
    initializeApiClient() {
        this.apiClient = new ApiClient();
    }
    
    setupEventListeners() {
        // Submit button click event
        this.submitBtn.addEventListener('click', () => {
            this.processCommand();
        });
        
        // Enter key in input field
        this.commandInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                this.processCommand();
            }
        });
    }
    
    async processCommand() {
        const command = this.commandInput.value.trim();
        if (!command) return;
        
        // Display the user's command
        this.appendTerminalMessage(`> ${command}`, 'command');
        
        // Clear the input
        this.commandInput.value = '';
        
        // Handle special commands
        if (this.handleSpecialCommand(command)) {
            return;
        }
        
        // Process the command with the LLM
        await this.processWithLLM(command);
    }
    
    handleSpecialCommand(command) {
        // Handle commands that start with 'train:'
        if (command.toLowerCase().startsWith('train:')) {
            const trainCommand = command.substring(6).trim(); // Remove 'train:' prefix (6 characters)
            this.handleTrainCommand(trainCommand);
            return true;
        }
        
        // Handle commands that start with 'vocab:'
        if (command.toLowerCase().startsWith('vocab:')) {
            const vocabCommand = command.substring(6).trim(); // Remove 'vocab:' prefix (6 characters)
            this.handleVocabCommand(vocabCommand);
            return true;
        }
        
        // Handle commands that start with 'view-corpus:'
        if (command.toLowerCase().startsWith('view-corpus:')) {
            const viewCorpusCommand = command.substring(12).trim(); // Remove 'view-corpus:' prefix (12 characters)
            this.handleViewCorpusCommand(viewCorpusCommand);
            return true;
        }
        
        // Handle commands that start with 'view-vocab:'
        if (command.toLowerCase().startsWith('view-vocab:')) {
            const viewVocabCommand = command.substring(10).trim(); // Remove 'view-vocab:' prefix (10 characters)
            this.handleViewVocabCommand(viewVocabCommand);
            return true;
        }
        
        // Handle commands that start with 'view-model:'
        if (command.toLowerCase().startsWith('view-model:')) {
            const viewModelCommand = command.substring(11).trim(); // Remove 'view-model:' prefix (11 characters)
            this.handleViewModelCommand(viewModelCommand);
            return true;
        }
        
        // Handle list commands
        switch (command.toLowerCase()) {
            case 'list-corpus':
                this.handleListCorpusCommand();
                return true;
            case 'list-vocab':
                this.handleListVocabCommand();
                return true;
            case 'list-model':
                this.handleListModelCommand();
                return true;
            case 'view vocab':
                this.handleViewVocabCommand('name=default');
                return true;
            case 'view corpus':
                this.handleViewCorpusCommand('name=sample.txt');
                return true;
            case 'view model':
                this.handleViewModelCommand('name=default');
                return true;
        }
        
        switch (command.toLowerCase()) {
            case 'help':
                this.appendTerminalMessage('Available commands:', 'response');
                this.appendTerminalMessage('  help - Show this help message', 'response');
                this.appendTerminalMessage('  model-info - Show information about the loaded model', 'response');
                this.appendTerminalMessage('  clear - Clear the terminal', 'response');
                this.appendTerminalMessage('  status - Check the backend status', 'response');
                this.appendTerminalMessage('  config - Show the backend configuration', 'response');
                this.appendTerminalMessage('  train:epochs=5,batchSize=32,learningRate=0.001 - Train the model', 'response');
                this.appendTerminalMessage('  vocab:source=corpora/sample.txt,vocabName=default - Create vocabulary', 'response');
                this.appendTerminalMessage('  view-corpus:name=sample.txt - View corpus file contents', 'response');
                this.appendTerminalMessage('  view-vocab:name=default - View vocabulary information', 'response');
                this.appendTerminalMessage('  view-model:name=default - View model information', 'response');
                this.appendTerminalMessage('  view vocab - View the default vocabulary file contents (short format)', 'response');
                this.appendTerminalMessage('  view corpus - View the default corpus file contents (short format)', 'response');
                this.appendTerminalMessage('  view model - View the default model information (short format)', 'response');
                this.appendTerminalMessage('  list-corpus - List all corpus files', 'response');
                this.appendTerminalMessage('  list-vocab - List all vocabulary files', 'response');
                this.appendTerminalMessage('  list-model - List all model files', 'response');
                return true;
            case 'model-info':
                if (this.modelLoaded) {
                    this.appendTerminalMessage('Model: TensorFlow.js LLM (Simulated)', 'response');
                    this.appendTerminalMessage('Status: Loaded and ready', 'response');
                } else {
                    this.appendTerminalMessage('Model: Not loaded yet', 'response');
                }
                return true;
            case 'status':
                this.checkBackendStatus();
                return true;
            case 'config':
                this.getBackendConfig();
                return true;
            case 'clear':
                this.clearTerminal();
                this.appendTerminalMessage('JavaScript LLM Terminal - Ready', 'system-message');
                this.appendTerminalMessage('Type a command or question and press Enter/Submit', 'system-message');
                this.appendTerminalMessage('For backend commands, use: train:[command], vocab:[command], status, config', 'system-message');
                return true;
            default:
                return false;
        }
    }
    
    async handleTrainCommand(trainCommand) {
        try {
            // Parse training parameters from command
            // Format: epochs=N,batchSize=N,learningRate=N,modelName=string
            const params = {};
            
            if (trainCommand) {
                const pairs = trainCommand.split(',');
                for (const pair of pairs) {
                    const [key, value] = pair.split('=');
                    if (key && value) {
                        const trimmedKey = key.trim();
                        const trimmedValue = value.trim();
                        
                        // Convert to appropriate type
                        if (trimmedKey.includes('epochs') || trimmedKey.includes('batch')) {
                            params[trimmedKey] = parseInt(trimmedValue);
                        } else if (trimmedKey.includes('rate') || trimmedKey.includes('temperature') || trimmedKey.includes('top')) {
                            params[trimmedKey] = parseFloat(trimmedValue);
                        } else {
                            params[trimmedKey] = trimmedValue;
                        }
                    }
                }
            }
            
            // Show loading indicator
            const loadingElement = document.createElement('div');
            loadingElement.className = 'loading';
            loadingElement.textContent = 'Training started...';
            loadingElement.id = 'training-indicator';
            this.terminal.appendChild(loadingElement);
            this.scrollToBottom();
            
            // Call backend API for training
            const result = await this.apiClient.train(params);
            
            // Remove loading indicator
            const loadingIndicator = document.getElementById('training-indicator');
            if (loadingIndicator) {
                loadingIndicator.remove();
            }
            
            // Display training result
            this.appendTerminalMessage(`Training completed: ${result.message}`, 'response');
            this.appendTerminalMessage(`Model: ${result.model}, Epochs: ${result.epochs}`, 'response');
        } catch (error) {
            this.appendTerminalMessage(`Error during training: ${error.message}`, 'system-message');
        }
    }
    
    async handleVocabCommand(vocabCommand) {
        try {
            // Parse vocabulary parameters from command
            // Expected format: source=path/to/file.txt,vocabName=name
            const params = {};
            
            if (vocabCommand) {
                const pairs = vocabCommand.split(',');
                for (const pair of pairs) {
                    const [key, value] = pair.split('=');
                    if (key && value) {
                        const trimmedKey = key.trim();
                        const trimmedValue = value.trim();
                        
                        // Map parameters to expected backend format
                        switch(trimmedKey.toLowerCase()) {
                            case 'source':
                                params.source = trimmedValue;
                                break;
                            case 'vocabname':
                                params.vocabName = trimmedValue;
                                break;
                            default:
                                params[trimmedKey] = trimmedValue;
                                break;
                        }
                    }
                }
            }
            
            // Show loading indicator
            const loadingElement = document.createElement('div');
            loadingElement.className = 'loading';
            loadingElement.textContent = 'Creating vocabulary...';
            loadingElement.id = 'vocab-indicator';
            this.terminal.appendChild(loadingElement);
            this.scrollToBottom();
            
            // Call backend API for vocabulary creation
            const result = await this.apiClient.createVocab(params);
            
            // Remove loading indicator
            const vocabIndicator = document.getElementById('vocab-indicator');
            if (vocabIndicator) {
                vocabIndicator.remove();
            }
            
            // Display vocabulary creation result
            this.appendTerminalMessage(`Vocabulary created: ${result.message}`, 'response');
            this.appendTerminalMessage(`Name: ${result.vocab.name}, Size: ${result.vocab.size}, Source: ${result.vocab.source}`, 'response');
        } catch (error) {
            this.appendTerminalMessage(`Error during vocabulary creation: ${error.message}`, 'system-message');
        }
    }
    
    async handleViewCorpusCommand(viewCorpusCommand) {
        try {
            // Parse view corpus parameters from command
            // Expected format: name=filename.txt
            let fileName = 'sample.txt'; // default value
            
            if (viewCorpusCommand) {
                const pairs = viewCorpusCommand.split(',');
                for (const pair of pairs) {
                    const [key, value] = pair.split('=');
                    if (key && value) {
                        const trimmedKey = key.trim().toLowerCase();
                        const trimmedValue = value.trim();
                        
                        if (trimmedKey === 'name') {
                            fileName = trimmedValue;
                        }
                    }
                }
            }
            
            // Show loading indicator
            const loadingElement = document.createElement('div');
            loadingElement.className = 'loading';
            loadingElement.textContent = `Loading corpus file: ${fileName}...`;
            loadingElement.id = 'view-corpus-indicator';
            this.terminal.appendChild(loadingElement);
            this.scrollToBottom();
            
            // Call backend API to get corpus file content
            const result = await this.apiClient.getCorpusFile(fileName);
            
            // Remove loading indicator
            const indicator = document.getElementById('view-corpus-indicator');
            if (indicator) {
                indicator.remove();
            }
            
            // Display corpus file content
            this.appendTerminalMessage(`Corpus file: ${fileName}`, 'response');
            this.appendTerminalMessage(`Size: ${result.size} bytes`, 'response');
            this.appendTerminalMessage(`Content:`, 'response');
            this.appendTerminalMessage(result.content, 'response');
        } catch (error) {
            this.appendTerminalMessage(`Error viewing corpus file: ${error.message}`, 'system-message');
        }
    }
    
    async handleViewVocabCommand(viewVocabCommand) {
        try {
            // Parse view vocab parameters from command
            // Expected format: name=default
            let vocabName = 'default'; // default value
            
            if (viewVocabCommand) {
                const pairs = viewVocabCommand.split(',');
                for (const pair of pairs) {
                    const [key, value] = pair.split('=');
                    if (key && value) {
                        const trimmedKey = key.trim().toLowerCase();
                        const trimmedValue = value.trim();
                        
                        if (trimmedKey === 'name') {
                            vocabName = trimmedValue;
                        }
                    }
                }
            }
            
            // Show loading indicator
            const loadingElement = document.createElement('div');
            loadingElement.className = 'loading';
            loadingElement.textContent = `Loading vocabulary: ${vocabName}...`;
            loadingElement.id = 'view-vocab-indicator';
            this.terminal.appendChild(loadingElement);
            this.scrollToBottom();
            
            // Call backend API to get vocab information
            const result = await this.apiClient.getVocabInfo(vocabName);
            
            // Remove loading indicator
            const indicator = document.getElementById('view-vocab-indicator');
            if (indicator) {
                indicator.remove();
            }
            
            // Display vocab information
            this.appendTerminalMessage(`Vocabulary: ${vocabName}`, 'response');
            this.appendTerminalMessage(`Size: ${result.vocab.size} tokens`, 'response');
            this.appendTerminalMessage(`Created: ${result.vocab.metadata.created_at}`, 'response');
            this.appendTerminalMessage(`Source: ${result.vocab.metadata.source}`, 'response');
            this.appendTerminalMessage(`First 10 tokens: ${result.vocab.tokens.slice(0, 10).join(', ')}`, 'response');
        } catch (error) {
            this.appendTerminalMessage(`Error viewing vocabulary: ${error.message}`, 'system-message');
        }
    }
    
    async handleViewModelCommand(viewModelCommand) {
        try {
            // Parse view model parameters from command
            // Expected format: name=default
            let modelName = 'default'; // default value
            
            if (viewModelCommand) {
                const pairs = viewModelCommand.split(',');
                for (const pair of pairs) {
                    const [key, value] = pair.split('=');
                    if (key && value) {
                        const trimmedKey = key.trim().toLowerCase();
                        const trimmedValue = value.trim();
                        
                        if (trimmedKey === 'name') {
                            modelName = trimmedValue;
                        }
                    }
                }
            }
            
            // Show loading indicator
            const loadingElement = document.createElement('div');
            loadingElement.className = 'loading';
            loadingElement.textContent = `Loading model: ${modelName}...`;
            loadingElement.id = 'view-model-indicator';
            this.terminal.appendChild(loadingElement);
            this.scrollToBottom();
            
            // Call backend API to get model information
            const result = await this.apiClient.getModelInfo(modelName);
            
            // Remove loading indicator
            const indicator = document.getElementById('view-model-indicator');
            if (indicator) {
                indicator.remove();
            }
            
            // Display model information
            this.appendTerminalMessage(`Model: ${modelName}`, 'response');
            this.appendTerminalMessage(`Status: ${result.model.exists ? 'Exists' : 'Not found'}`, 'response');
            if (result.model.exists) {
                this.appendTerminalMessage(`Path: ${result.model.path}`, 'response');
                this.appendTerminalMessage(`Size: ${result.model.size} bytes`, 'response');
                this.appendTerminalMessage(`Modified: ${result.model.modified}`, 'response');
                this.appendTerminalMessage(`Configuration:`, 'response');
                for (const [key, value] of Object.entries(result.model.config)) {
                    this.appendTerminalMessage(`  ${key}: ${value}`, 'response');
                }
            } else {
                this.appendTerminalMessage(`Model file not found: ${result.model.path}`, 'response');
            }
        } catch (error) {
            this.appendTerminalMessage(`Error viewing model: ${error.message}`, 'system-message');
        }
    }
    
    async handleListCorpusCommand() {
        try {
            // Show loading indicator
            const loadingElement = document.createElement('div');
            loadingElement.className = 'loading';
            loadingElement.textContent = 'Loading corpus files...';
            loadingElement.id = 'list-corpus-indicator';
            this.terminal.appendChild(loadingElement);
            this.scrollToBottom();
            
            // Call backend API to list corpus files
            const result = await this.apiClient.listCorpusFiles();
            
            // Remove loading indicator
            const indicator = document.getElementById('list-corpus-indicator');
            if (indicator) {
                indicator.remove();
            }
            
            // Display corpus files
            this.appendTerminalMessage(`Found ${result.files.length} corpus file(s):`, 'response');
            if (result.files.length > 0) {
                result.files.forEach(file => {
                    this.appendTerminalMessage(`- ${file.name} (${file.size} bytes, modified: ${file.modified})`, 'response');
                });
            } else {
                this.appendTerminalMessage('No corpus files found.', 'response');
            }
        } catch (error) {
            this.appendTerminalMessage(`Error listing corpus files: ${error.message}`, 'system-message');
        }
    }
    
    async handleListVocabCommand() {
        try {
            // Show loading indicator
            const loadingElement = document.createElement('div');
            loadingElement.className = 'loading';
            loadingElement.textContent = 'Loading vocabulary files...';
            loadingElement.id = 'list-vocab-indicator';
            this.terminal.appendChild(loadingElement);
            this.scrollToBottom();
            
            // Call backend API to list vocab files
            const result = await this.apiClient.listVocabFiles();
            
            // Remove loading indicator
            const indicator = document.getElementById('list-vocab-indicator');
            if (indicator) {
                indicator.remove();
            }
            
            // Display vocab files
            this.appendTerminalMessage(`Found ${result.files.length} vocabulary file(s):`, 'response');
            if (result.files.length > 0) {
                result.files.forEach(file => {
                    this.appendTerminalMessage(`- ${file.name} (${file.size} bytes, modified: ${file.modified})`, 'response');
                });
            } else {
                this.appendTerminalMessage('No vocabulary files found.', 'response');
            }
        } catch (error) {
            this.appendTerminalMessage(`Error listing vocabulary files: ${error.message}`, 'system-message');
        }
    }
    
    async handleListModelCommand() {
        try {
            // Show loading indicator
            const loadingElement = document.createElement('div');
            loadingElement.className = 'loading';
            loadingElement.textContent = 'Loading model files...';
            loadingElement.id = 'list-model-indicator';
            this.terminal.appendChild(loadingElement);
            this.scrollToBottom();
            
            // Call backend API to list model files
            const result = await this.apiClient.listModelFiles();
            
            // Remove loading indicator
            const indicator = document.getElementById('list-model-indicator');
            if (indicator) {
                indicator.remove();
            }
            
            // Display model files
            this.appendTerminalMessage(`Found ${result.files.length} model file(s):`, 'response');
            if (result.files.length > 0) {
                result.files.forEach(file => {
                    this.appendTerminalMessage(`- ${file.name} (${file.size} bytes, modified: ${file.modified})`, 'response');
                });
            } else {
                this.appendTerminalMessage('No model files found.', 'response');
            }
        } catch (error) {
            this.appendTerminalMessage(`Error listing model files: ${error.message}`, 'system-message');
        }
    }
    
    async checkBackendStatus() {
        try {
            const status = await this.apiClient.getStatus();
            this.appendTerminalMessage(`Backend Status: ${status.status}`, 'response');
            this.appendTerminalMessage(`Model loaded: ${status.model_loaded ? 'Yes' : 'No'}`, 'response');
            this.appendTerminalMessage(`Timestamp: ${status.timestamp}`, 'response');
        } catch (error) {
            this.appendTerminalMessage(`Error checking status: ${error.message}`, 'system-message');
        }
    }
    
    async getBackendConfig() {
        try {
            const config = await this.apiClient.getConfig();
            this.appendTerminalMessage('Backend Configuration:', 'response');
            for (const [key, value] of Object.entries(config.config.default)) {
                this.appendTerminalMessage(`${key}: ${value}`, 'response');
            }
        } catch (error) {
            this.appendTerminalMessage(`Error getting config: ${error.message}`, 'system-message');
        }
    }
    
    async processWithLLM(input) {
        try {
            // Show loading indicator
            const loadingElement = document.createElement('div');
            loadingElement.className = 'loading';
            loadingElement.textContent = 'Processing with LLM...';
            loadingElement.id = 'loading-indicator';
            this.terminal.appendChild(loadingElement);
            this.scrollToBottom();
            
            // Try to generate response using backend API first
            try {
                const result = await this.apiClient.generate({
                    prompt: input,
                    maxTokens: 100,
                    temperature: 0.7,
                    topK: 50,
                    topP: 0.9
                });
                
                // Remove loading indicator
                const loadingIndicator = document.getElementById('loading-indicator');
                if (loadingIndicator) {
                    loadingIndicator.remove();
                }
                
                // Display the response from backend
                this.appendTerminalMessage(`Backend: ${result.generated}`, 'response');
                return;
            } catch (backendError) {
                // If backend fails, fall back to local model
                console.warn("Backend generation failed, falling back to local model:", backendError);
            }
            
            // Generate response using the local LLM model
            const response = await this.llmModel.generateResponse(input);
            
            // Remove loading indicator
            const loadingIndicator = document.getElementById('loading-indicator');
            if (loadingIndicator) {
                loadingIndicator.remove();
            }
            
            // Display the response
            this.appendTerminalMessage(`Local: ${response}`, 'response');
        } catch (error) {
            this.appendTerminalMessage(`Error processing command: ${error.message}`, 'system-message');
        }
    }
    
    appendTerminalMessage(message, className) {
        const messageElement = document.createElement('div');
        messageElement.className = className;
        messageElement.textContent = message;
        this.terminal.appendChild(messageElement);
        this.scrollToBottom();
    }
    
    scrollToBottom() {
        this.terminal.scrollTop = this.terminal.scrollHeight;
    }
    
    clearTerminal() {
        this.terminal.innerHTML = '';
    }
}

// Initialize the application when the page loads
document.addEventListener('DOMContentLoaded', () => {
    window.llmTerminal = new LLMTerminal();
});