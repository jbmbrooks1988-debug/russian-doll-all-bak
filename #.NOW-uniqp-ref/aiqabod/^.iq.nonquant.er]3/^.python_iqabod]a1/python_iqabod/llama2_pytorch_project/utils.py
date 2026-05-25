"""Utility functions for Llama2 implementation"""
import torch
import random
import numpy as np
from typing import Optional


def set_seed(seed: int = 42):
    """Set random seed for reproducibility"""
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    if torch.cuda.is_available():
        torch.cuda.manual_seed(seed)
        torch.cuda.manual_seed_all(seed)
    torch.backends.cudnn.deterministic = True
    torch.backends.cudnn.benchmark = False


def count_parameters(model: torch.nn.Module) -> int:
    """Count the total number of parameters in a model"""
    return sum(p.numel() for p in model.parameters())


def count_trainable_parameters(model: torch.nn.Module) -> int:
    """Count the total number of trainable parameters in a model"""
    return sum(p.numel() for p in model.parameters() if p.requires_grad)


def get_device() -> torch.device:
    """Get the appropriate device for computation"""
    return torch.device("cuda" if torch.cuda.is_available() else "cpu")


def format_number(num: int) -> str:
    """Format a number with commas for readability"""
    return f"{num:,}"


def save_model_checkpoint(model: torch.nn.Module, path: str, epoch: int, step: int, **kwargs):
    """Save a model checkpoint with additional metadata"""
    checkpoint = {
        'epoch': epoch,
        'step': step,
        'model_state_dict': model.state_dict(),
        **kwargs
    }
    torch.save(checkpoint, path)


def load_model_checkpoint(model: torch.nn.Module, path: str) -> dict:
    """Load a model checkpoint"""
    checkpoint = torch.load(path, map_location=get_device())
    model.load_state_dict(checkpoint['model_state_dict'])
    return checkpoint


def calculate_perplexity(loss: float) -> float:
    """Calculate perplexity from cross-entropy loss"""
    return float(torch.exp(torch.tensor(loss)))


def create_mask(seq_len: int, device: torch.device) -> torch.Tensor:
    """Create a causal mask for transformer attention"""
    mask = torch.tril(torch.ones(seq_len, seq_len, device=device)).unsqueeze(0).unsqueeze(0)
    return mask


def pad_sequences(sequences, padding_value=0, padding_side='right'):
    """Pad a list of sequences to the same length"""
    max_len = max(len(seq) for seq in sequences)
    padded = []
    
    for seq in sequences:
        if len(seq) < max_len:
            padding = [padding_value] * (max_len - len(seq))
            if padding_side == 'right':
                padded_seq = seq + padding
            else:
                padded_seq = padding + seq
        else:
            padded_seq = seq
        padded.append(padded_seq)
    
    return padded


def get_linear_schedule_with_warmup(optimizer, num_warmup_steps, num_training_steps, last_epoch=-1):
    """Create a learning rate scheduler with linear warmup and decay"""
    from torch.optim.lr_scheduler import LambdaLR
    
    def lr_lambda(current_step: int):
        if current_step < num_warmup_steps:
            return float(current_step) / float(max(1, num_warmup_steps))
        return max(
            0.0, 
            float(num_training_steps - current_step) / float(max(1, num_training_steps - num_warmup_steps))
        )
    
    return LambdaLR(optimizer, lr_lambda, last_epoch)


def batchify(data, batch_size):
    """Split data into batches of specified size"""
    batches = []
    for i in range(0, len(data), batch_size):
        batches.append(data[i:i + batch_size])
    return batches


def flatten_list(list_of_lists):
    """Flatten a list of lists into a single list"""
    return [item for sublist in list_of_lists for item in sublist]