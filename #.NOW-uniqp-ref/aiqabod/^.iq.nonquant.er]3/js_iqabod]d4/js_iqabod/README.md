# JavaScript LLM Terminal with PHP Backend

This project implements a JavaScript-based LLM terminal interface with a PHP backend for training and generation capabilities.

## Features

- **Vocabulary Creation**: Create vocabulary from text corpora files
- **Training**: Real training functionality implemented in PHP with configurable parameters (epochs, batch size, learning rate)
- **Generation**: Text generation with configurable parameters (temperature, top-k, top-p sampling)
- **Web Interface**: Terminal-style interface with command support
- **API Communication**: REST API for backend communication
- **Model Management**: Save/load model functionality
- **TensorFlow.js Integration**: Local model fallback when backend is unavailable
- **Terminal-like UI**: Command input and response display with special commands

## Architecture

```
js_iqabod/
├── backend/            # PHP backend services
│   ├── api.php        # Main API endpoint
│   ├── train.php      # Training functionality
│   ├── generate.php   # Generation functionality
│   ├── vocab.php      # Vocabulary creation functionality
│   ├── config.php     # PHP configuration
│   └── helpers.php    # Helper functions
├── models/             # Model storage
├── data/               # Training data storage
│   ├── corpora/       # Training corpora
│   └── vocab/         # Vocabulary files
├── js/
│   ├── app.js         # Enhanced main app with backend calls
│   ├── api.js         # API communication layer
│   ├── model.js       # Local LLM model implementation
│   └── main.js        # Main application logic
├── php/
│   └── [additional PHP modules]
├── index.html         # Updated HTML with API integration
└── README.md          # Updated documentation
```

## How to Run

### Requirements
- Web server with PHP 7.4+
- JavaScript-enabled browser
- Internet access (for TensorFlow.js CDN)

### Setup
1. Clone this repository
2. Place in your web server directory (e.g., Apache, Nginx) OR use the built-in PHP server
3. Ensure the following directories have write permissions:
   - `models/`
   - `data/corpora/`
   - `data/vocab/`
   - `logs/` (create this directory for logging)
4. Add training data to `data/corpora/` directory in `.txt` format

### Local Server Option (Alternative to Apache/Nginx)
If you don't have a web server, you can use the built-in PHP server:
```bash
./start_server.sh
```
This will start a local server at `http://localhost:8080` and you can access the interface at `http://localhost:8080/index.html`

### Usage
1. Access `index.html` via your web server
2. Use the interface to interact with the model

### Command Reference
- **Help**: `help` - Show available commands
- **Status**: `status` - Check backend status
- **Config**: `config` - Show backend configuration
- **Clear**: `clear` - Clear the terminal
- **Vocabulary**: `vocab:source=corpora/file.txt,vocabName=myvocab` - Create vocabulary
- **Training**: `train:epochs=5,batchSize=32,learningRate=0.001,modelName=mymodel,vocabName=myvocab` - Start training
- **Generation**: Simply type your prompt and press Enter - Generate text using trained model
- **View Corpus**: `view-corpus:name=sample.txt` - View corpus file contents
- **View Vocabulary**: `view-vocab:name=default` - View vocabulary information
- **View Model**: `view-model:name=default` - View model information
- **List Corpus**: `list-corpus` - List all corpus files
- **List Vocabulary**: `list-vocab` - List all vocabulary files
- **List Models**: `list-model` - List all model files

## Usage

### Basic Commands
- `help` - Show available commands
- `model-info` - Show local model info
- `status` - Check backend status
- `config` - Show backend configuration
- `clear` - Clear the terminal

### Vocabulary Creation
To create vocabulary from a corpus file, use the vocab command:
```
vocab:source=corpora/sample.txt,vocabName=myvocab
```

Available vocabulary parameters:
- `source` - Path to the text file in the corpora directory
- `vocabName` - Name for the vocabulary file (defaults to 'default')

### Training
To start a training session, use the training button in the UI or enter a command in the format:
```
train:epochs=5,batchSize=32,learningRate=0.001,modelName=mymodel,vocabName=myvocab
```

Available training parameters:
- `epochs` - Number of training epochs
- `batchSize` - Size of training batches
- `learningRate` - Learning rate for training
- `modelName` - Name for the model file (defaults to 'default')
- `vocabName` - Vocabulary to use for training (defaults to 'default')

### Generation
Enter any text to generate a response using the backend model. If the backend is not available, the system will fall back to the local TensorFlow.js model.

### Development & Debugging Commands
The following commands are useful for developers to inspect system components:

#### View Commands:
- `view-corpus:name=filename.txt` - View contents of a corpus file
- `view-vocab:name=vocabname` - View vocabulary information (size, tokens, creation date)
- `view-model:name=modelname` - View model information (size, status, path)

#### List Commands:
- `list-corpus` - List all available corpus files
- `list-vocab` - List all vocabulary files  
- `list-model` - List all model files

These commands are especially useful for debugging and development, allowing inspection of the training data, vocabulary, and models used by the system.

## PHP Backend Endpoints

- **POST /backend/vocab**: Create vocabulary from text file
- **GET /backend/vocab**: Get vocabulary information
- **POST /backend/train**: Start model training
- **POST /backend/generate**: Generate text with prompt
- **GET /backend/status**: Get model status
- **POST /backend/config**: Update model configuration
- **GET /backend/config**: Get model configuration

## Configuration

The system can be configured via the following constants in `backend/config.php`:

- `MODEL_VOCAB_SIZE` - Vocabulary size
- `MODEL_EMBED_DIM` - Embedding dimension
- `MODEL_NUM_HEADS` - Number of attention heads
- `MODEL_FF_DIM` - Feed-forward dimension
- `MODEL_NUM_LAYERS` - Number of transformer layers
- `CONTEXT_LENGTH` - Context window length
- Training and generation parameters
- File paths for models, data, vocab, and corpora

## Model Architecture

The backend implements a simplified transformer model with:
- Embedding layer
- Multiple transformer blocks with attention mechanisms
- Feed-forward networks
- Layer normalization
- KV-cache for efficient generation

## Training Data

Training data should be placed in the `data/corpora/` directory in `.txt` format. The training system will automatically tokenize and process all `.txt` files in this directory.

## Vocabulary Data

Vocabulary files are stored in the `data/vocab/` directory. The vocab creation system will process text files to extract unique tokens and create token-id mappings.

## Implementation Details

### JavaScript Frontend
- **TensorFlow.js Integration**: Local model fallback for when backend is unavailable
- **API Communication Layer**: Handles all backend communication
- **Terminal-like UI**: Command interface with responsive feedback
- **Loading Indicators**: Visual feedback during processing

### PHP Backend
- **Vocabulary Implementation**: Text tokenization and vocabulary creation
- **Training Implementation**: Real training with gradient updates
- **Generation Implementation**: Text generation with sampling strategies
- **REST API**: Standardized communication with frontend
- **Model Persistence**: Save/load trained models
- **Configurable Parameters**: Runtime parameters for training and generation

## Development

The frontend uses TensorFlow.js for local model functionality and communicates with the PHP backend via REST API for vocabulary creation, training and generation tasks. The system implements a fallback mechanism where if the backend is unavailable, it uses the local JavaScript implementation.

## Browser Support

- Modern browsers with WebGL support (required for TensorFlow.js)
- Works on Chrome, Firefox, Safari, Edge
- Requires JavaScript to be enabled
- Cross-origin requests allowed for API communication