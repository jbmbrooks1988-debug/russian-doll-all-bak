# Llama2 PyTorch Implementation

A complete implementation of the Llama2 architecture in Python/PyTorch, featuring the same command-line interface as the original C implementation while providing enhanced functionality with PyTorch's advantages.

## Table of Contents
- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Examples](#examples)
- [Project Structure](#project-structure)
- [Architecture Details](#architecture-details)

## Features

- Complete Llama2 architecture implementation (RMSNorm, RoPE, SwiGLU, GQA)
- BPE tokenizer with byte-level tokenization
- Training with AdamW optimizer and learning rate scheduling
- Text generation with multiple sampling strategies (greedy, top-k, top-p)
- KV-cache for efficient generation
- Same CLI interface as the original C implementation
- GPU support with automatic differentiation
- Modular code structure with clean interfaces

## Requirements

- Python 3.8+
- PyTorch 2.0+
- NumPy
- Regex library

## Installation

1. Clone or download this repository
2. Install the required dependencies:

```bash
pip install -r requirements.txt
```

## Usage

The implementation supports three main modes: vocabulary creation, training, and text generation.

### Main Command Structure

```bash
python main.py [options]
```

### Available Options

- `--vocab PATH`: Path to vocabulary file for tokenization
- `--train PATH`: Path to training data
- `--generate TEXT`: Text to generate from
- `--model_path PATH`: Path to pre-trained model
- `--tokenizer_path PATH`: Path to tokenizer file (default: "tokenizer.model")
- `--config_path PATH`: Path to model config file
- `--train_config_path PATH`: Path to training config file
- `--max_new_tokens N`: Max new tokens to generate (default: 256)
- `--temperature FLOAT`: Temperature for generation (default: 0.6)
- `--top_p FLOAT`: Top-p for generation (default: 0.9)
- `--top_k INT`: Top-k for generation (default: 50)

## Examples

### 1. Create and Train a Tokenizer

```bash
python main.py --vocab "path/to/your/corpus.txt"
```

This will create a tokenizer model at `tokenizer.model` based on the provided corpus.

### 2. Train the Model

```bash
python main.py --train "path/to/your/training/data" --model_path "model.pth"
```

This will load training data from the specified path and save the trained model to "model.pth".

### 3. Generate Text

```bash
python main.py --generate "The weather today is" --max_new_tokens 100 --temperature 0.8
```

This will generate 100 new tokens starting from the prompt "The weather today is" with temperature 0.8.

### 4. Generate with Different Sampling Strategies

```bash
# Using top-p sampling
python main.py --generate "Once upon a time" --top_p 0.9

# Using top-k sampling
python main.py --generate "The future of AI" --top_k 40

# Using greedy sampling (temperature = 0)
python main.py --generate "In a world where" --temperature 0
```

### 5. Train with Custom Configuration

```bash
python main.py --train "data/" --config_path "model_config.json" --train_config_path "train_config.json"
```

## Project Structure

```
llama2_pytorch_project/
├── config.py          # Model and training configurations
├── tokenizer.py       # BPE tokenizer implementation
├── model.py           # Core Llama2 model architecture
├── training.py        # Training loop and utilities
├── generation.py      # Text generation functions
├── data.py            # Data loading and preprocessing
├── main.py            # Main entry point
├── train.py           # Training script
├── generate.py        # Generation script
├── utils.py           # Utility functions
├── test_basic.py      # Basic tests
└── requirements.txt   # Project dependencies
```

## Architecture Details

### Model Components

1. **Llama2RMSNorm**: Root Mean Square Layer Normalization
2. **Llama2RoPE**: Rotary Positional Embeddings for sequence position encoding
3. **Llama2Attention**: Multi-head attention with Grouped-Query Attention (GQA)
4. **Llama2FeedForward**: SwiGLU (Swish-Gated Linear Unit) feed-forward network
5. **Llama2DecoderLayer**: Complete transformer decoder block
6. **Llama2Model**: Full model with embeddings and output projection

### Key Features Implemented

- **Grouped-Query Attention**: More efficient attention with reduced memory usage
- **Rotary Positional Embeddings**: Better positional encoding than standard methods
- **SwiGLU Activation**: Advanced activation function for better performance
- **RMSNorm**: Normalization technique that works well with transformer architectures
- **KV-Cache**: Efficient caching mechanism for faster text generation
- **Causal Masking**: Ensures model only attends to previous tokens during generation

## Configuration

### Model Configuration

The model supports various configurations through the `ModelConfig` class which includes:

- `dim`: Model dimension
- `hidden_dim`: Hidden dimension for the feed-forward network
- `n_layers`: Number of transformer layers
- `n_heads`: Number of attention heads
- `n_kv_heads`: Number of key-value heads for GQA
- `vocab_size`: Vocabulary size
- `max_seq_len`: Maximum sequence length
- `norm_eps`: Epsilon value for normalization
- `temperature`, `top_p`, `top_k`: Generation parameters

### Training Configuration

The training process supports various parameters through the `TrainingConfig` class which includes:

- Learning rate scheduling
- Weight decay
- Batch size and gradient accumulation
- Checkpointing and logging intervals
- Device configuration (CPU/GPU)

## Customization

You can customize the model by:

1. Modifying the `ModelConfig` parameters
2. Adjusting training parameters in `TrainingConfig`
3. Using different sampling strategies during generation
4. Changing the model architecture by modifying the model files

## Testing

Run the basic tests to verify your installation:

```bash
python test_basic.py
```

The test suite verifies:
- Model creation with correct dimensions
- Tokenizer encoding and decoding functionality
- Model forward pass
- Basic generation capability

## Quick Start Pipeline

To run a complete pipeline that creates vocabulary, trains a model, and generates text in one go:

```bash
python run_persistent_pipeline.py
```

This script will:
1. Create a sample corpus from example texts
2. Train a vocabulary from the corpus
3. Create and configure a model
4. Simulate training (save the initial model)
5. Generate text with different sampling strategies
6. Save all files to `./pipeline_output/`

After the pipeline completes, you can use the trained components:

```bash
python main.py --generate "Your prompt here" --model_path ./pipeline_output/model.pth --tokenizer_path ./pipeline_output/tokenizer.model
```

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is available under the MIT License.