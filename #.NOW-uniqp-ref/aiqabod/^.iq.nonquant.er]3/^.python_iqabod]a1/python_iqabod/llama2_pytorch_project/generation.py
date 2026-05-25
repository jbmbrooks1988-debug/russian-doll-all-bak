"""Text generation implementation for Llama2 model"""
import torch
import torch.nn.functional as F
import numpy as np
from typing import List, Optional, Union
import time
from model import Llama2ForCausalLM
from tokenizer import Tokenizer
from config import ModelConfig


def sample_logits(
    logits: torch.Tensor,
    temperature: float = 0.6,
    top_p: float = 0.9,
    top_k: int = 50
) -> torch.Tensor:
    """Sample from logits using temperature, top-p and top-k sampling"""
    # Apply temperature
    if temperature > 0:
        logits = logits / temperature
    else:
        # If temperature is 0, take the argmax (greedy decoding)
        return torch.argmax(logits, dim=-1, keepdim=True)
    
    # Apply top-k filtering
    if top_k > 0:
        # Get the top-k values and indices
        top_k_logits, top_k_indices = torch.topk(logits, min(top_k, logits.size(-1)))
        # Create a mask to set values outside top-k to -inf
        mask = torch.full_like(logits, float('-inf'))
        mask.scatter_(1, top_k_indices, 0)
        logits = logits + mask
    
    # Apply top-p (nucleus) filtering
    if top_p < 1.0:
        # Sort logits in descending order
        sorted_logits, sorted_indices = torch.sort(logits, descending=True)
        # Calculate cumulative probabilities
        cumulative_probs = torch.cumsum(F.softmax(sorted_logits, dim=-1), dim=-1)
        # Create mask for tokens that exceed top_p
        sorted_indices_to_remove = cumulative_probs > top_p
        # Shift the indices to the right to keep the first token that exceeds top_p
        sorted_indices_to_remove[..., 1:] = sorted_indices_to_remove[..., :-1].clone()
        sorted_indices_to_remove[..., 0] = 0
        # Create mask for the original indices
        indices_to_remove = sorted_indices_to_remove.scatter(1, sorted_indices, sorted_indices_to_remove)
        logits = logits.masked_fill(indices_to_remove, float('-inf'))
    
    # Apply softmax to get probabilities
    probs = F.softmax(logits, dim=-1)
    
    # Sample from the probabilities
    next_token = torch.multinomial(probs, num_samples=1)
    
    return next_token


class Generator:
    """Text generation class for Llama2 model"""
    
    def __init__(
        self,
        model: Llama2ForCausalLM,
        tokenizer: Tokenizer,
        model_config: ModelConfig
    ):
        self.model = model
        self.tokenizer = tokenizer
        self.model_config = model_config
        self.device = next(model.parameters()).device
    
    @torch.no_grad()
    def generate(
        self,
        prompt: str,
        max_new_tokens: int = 256,
        temperature: float = 0.6,
        top_p: float = 0.9,
        top_k: int = 50,
        stop_token: Optional[int] = None,
        echo: bool = False
    ) -> str:
        """Generate text from a prompt"""
        # Encode the prompt
        input_ids = self.tokenizer.encode(prompt, bos=True, eos=False)
        tokens = torch.tensor(input_ids, dtype=torch.long, device=self.device).unsqueeze(0)
        
        # Store the original length to determine where generation starts
        prompt_length = tokens.size(1)
        
        # Store generated tokens
        generated_tokens = []
        
        # Generate tokens one by one
        for i in range(max_new_tokens):
            # Get the model's output for the last token
            if tokens.size(1) > 1:
                # Use only the last token for generation (KV-cache efficiency)
                start_pos = tokens.size(1) - 1
                logits = self.model.get_token_logits(tokens, start_pos=start_pos)
            else:
                # First pass - use the entire sequence
                logits = self.model.get_token_logits(tokens, start_pos=0)
            
            # Sample next token
            next_token = sample_logits(
                logits,
                temperature=temperature,
                top_p=top_p,
                top_k=top_k
            )
            
            # Add next token to sequence
            tokens = torch.cat([tokens, next_token], dim=1)
            generated_tokens.append(next_token.squeeze().item())
            
            # Check for stop token
            if stop_token is not None and next_token.item() == stop_token:
                break
        
        # Decode the generated tokens
        if echo:
            # Return full sequence (prompt + generated)
            full_tokens = input_ids + generated_tokens
            return self.tokenizer.decode(full_tokens)
        else:
            # Return only generated part
            return self.tokenizer.decode(generated_tokens)
    
    @torch.no_grad()
    def generate_stream(
        self,
        prompt: str,
        max_new_tokens: int = 256,
        temperature: float = 0.6,
        top_p: float = 0.9,
        top_k: int = 50,
        stop_token: Optional[int] = None
    ):
        """Generate text token by token with streaming output"""
        # Encode the prompt
        input_ids = self.tokenizer.encode(prompt, bos=True, eos=False)
        tokens = torch.tensor(input_ids, dtype=torch.long, device=self.device).unsqueeze(0)
        
        # Store the original length to determine where generation starts
        prompt_length = tokens.size(1)
        
        generated_tokens = []
        
        for i in range(max_new_tokens):
            # Get the model's output for the last token
            if tokens.size(1) > 1:
                start_pos = tokens.size(1) - 1
                logits = self.model.get_token_logits(tokens, start_pos=start_pos)
            else:
                logits = self.model.get_token_logits(tokens, start_pos=0)
            
            # Sample next token
            next_token = sample_logits(
                logits,
                temperature=temperature,
                top_p=top_p,
                top_k=top_k
            )
            
            # Add next token to sequence
            tokens = torch.cat([tokens, next_token], dim=1)
            new_token = next_token.squeeze().item()
            generated_tokens.append(new_token)
            
            # Yield the decoded token
            yield self.tokenizer.decode([new_token])
            
            # Check for stop token
            if stop_token is not None and new_token == stop_token:
                break
    
    def generate_with_callback(
        self,
        prompt: str,
        max_new_tokens: int = 256,
        temperature: float = 0.6,
        top_p: float = 0.9,
        top_k: int = 50,
        stop_token: Optional[int] = None,
        callback=None
    ) -> str:
        """Generate text with a callback function for each new token"""
        # Encode the prompt
        input_ids = self.tokenizer.encode(prompt, bos=True, eos=False)
        tokens = torch.tensor(input_ids, dtype=torch.long, device=self.device).unsqueeze(0)
        
        generated_tokens = []
        
        for i in range(max_new_tokens):
            # Get the model's output for the last token
            if tokens.size(1) > 1:
                start_pos = tokens.size(1) - 1
                logits = self.model.get_token_logits(tokens, start_pos=start_pos)
            else:
                logits = self.model.get_token_logits(tokens, start_pos=0)
            
            # Sample next token
            next_token = sample_logits(
                logits,
                temperature=temperature,
                top_p=top_p,
                top_k=top_k
            )
            
            # Add next token to sequence
            tokens = torch.cat([tokens, next_token], dim=1)
            new_token = next_token.squeeze().item()
            generated_tokens.append(new_token)
            
            # Call the callback function if provided
            if callback:
                callback(self.tokenizer.decode([new_token]))
            
            # Check for stop token
            if stop_token is not None and new_token == stop_token:
                break
        
        # Return the full generated text
        return self.tokenizer.decode(generated_tokens)


def generate_text(
    model: Llama2ForCausalLM,
    tokenizer: Tokenizer,
    model_config: ModelConfig,
    prompt: str,
    max_new_tokens: int = 256,
    temperature: float = 0.6,
    top_p: float = 0.9,
    top_k: int = 50
) -> str:
    """Convenience function for text generation"""
    generator = Generator(model, tokenizer, model_config)
    return generator.generate(
        prompt=prompt,
        max_new_tokens=max_new_tokens,
        temperature=temperature,
        top_p=top_p,
        top_k=top_k
    )