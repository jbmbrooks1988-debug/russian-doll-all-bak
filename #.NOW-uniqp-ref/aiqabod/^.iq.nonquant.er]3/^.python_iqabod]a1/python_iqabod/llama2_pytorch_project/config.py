from dataclasses import dataclass
from typing import Optional
import json
import os


@dataclass
class ModelConfig:
    """Configuration class for Llama2 model parameters"""
    # Model dimensions
    dim: int = 4096          # Model dimension
    hidden_dim: int = 11008  # Hidden dimension for FFN
    n_layers: int = 32       # Number of transformer layers
    n_heads: int = 32        # Number of attention heads
    n_kv_heads: int = 8      # Number of key-value heads (for GQA)
    vocab_size: int = 32000  # Vocabulary size
    max_seq_len: int = 2048  # Maximum sequence length
    
    # Model parameters
    norm_eps: float = 1e-5   # Epsilon for RMSNorm
    rope_theta: float = 10000.0  # Base for RoPE
    
    # Generation parameters
    temperature: float = 0.6
    top_p: float = 0.9
    top_k: int = 50
    
    def to_dict(self):
        """Convert config to dictionary"""
        return {k: v for k, v in self.__dict__.items() if not k.startswith('_')}
    
    def save(self, path: str):
        """Save config to JSON file"""
        with open(path, 'w') as f:
            json.dump(self.to_dict(), f, indent=2)
    
    @classmethod
    def from_dict(cls, config_dict: dict):
        """Create config from dictionary"""
        return cls(**config_dict)
    
    @classmethod
    def load(cls, path: str):
        """Load config from JSON file"""
        with open(path, 'r') as f:
            config_dict = json.load(f)
        return cls.from_dict(config_dict)


@dataclass
class TrainingConfig:
    """Configuration class for training parameters"""
    # Training parameters
    learning_rate: float = 3e-4
    weight_decay: float = 0.1
    beta1: float = 0.9
    beta2: float = 0.95
    eps: float = 1e-8
    
    # Batch and scheduling
    batch_size: int = 16
    micro_batch_size: int = 4  # For gradient accumulation
    epochs: int = 1
    warmup_steps: int = 1000
    max_steps: int = 100000  # Maximum training steps
    lr_decay_steps: int = 100000  # Steps for cosine decay
    
    # Learning rate scheduling
    min_lr: float = 1e-5  # Minimum learning rate
    
    # Training settings
    gradient_clip_val: float = 1.0
    use_fp16: bool = False  # Use mixed precision training
    device: str = 'cuda' if __import__('torch').cuda.is_available() else 'cpu'
    
    # Checkpointing and logging
    save_every: int = 1000  # Save checkpoint every n steps
    log_every: int = 10     # Log metrics every n steps
    eval_every: int = 1000  # Evaluate model every n steps
    eval_steps: int = 100   # Number of eval steps
    
    # Data settings
    train_file: Optional[str] = None
    val_file: Optional[str] = None
    corpus_dir: str = "corpuses"
    curriculum_dir: str = "curriculum"
    
    def to_dict(self):
        """Convert config to dictionary"""
        return {k: v for k, v in self.__dict__.items() if not k.startswith('_')}
    
    def save(self, path: str):
        """Save config to JSON file"""
        with open(path, 'w') as f:
            json.dump(self.to_dict(), f, indent=2)
    
    @classmethod
    def from_dict(cls, config_dict: dict):
        """Create config from dictionary"""
        return cls(**config_dict)
    
    @classmethod
    def load(cls, path: str):
        """Load config from JSON file"""
        with open(path, 'r') as f:
            config_dict = json.load(f)
        return cls.from_dict(config_dict)


# Default configurations for different model sizes
LLAMA2_7B_CONFIG = ModelConfig(
    dim=4096,
    hidden_dim=11008,
    n_layers=32,
    n_heads=32,
    n_kv_heads=8,
    vocab_size=32000,
    max_seq_len=2048
)

LLAMA2_13B_CONFIG = ModelConfig(
    dim=5120,
    hidden_dim=13824,
    n_layers=40,
    n_heads=40,
    n_kv_heads=8,
    vocab_size=32000,
    max_seq_len=2048
)

LLAMA2_70B_CONFIG = ModelConfig(
    dim=8192,
    hidden_dim=28672,
    n_layers=80,
    n_heads=64,
    n_kv_heads=8,
    vocab_size=32000,
    max_seq_len=2048
)

# Default configs
DEFAULT_MODEL_CONFIG = LLAMA2_7B_CONFIG
DEFAULT_TRAINING_CONFIG = TrainingConfig()