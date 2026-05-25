"""Llama2 PyTorch Model Implementation"""
import math
from typing import Optional, Tuple
import torch
import torch.nn.functional as F
import torch.nn as nn
from config import ModelConfig


class Llama2RMSNorm(nn.Module):
    """RMS Normalization layer for Llama2"""
    
    def __init__(self, dim: int, eps: float = 1e-6):
        super().__init__()
        self.eps = eps
        self.weight = nn.Parameter(torch.ones(dim))
    
    def _norm(self, x):
        return x * torch.rsqrt(x.pow(2).mean(-1, keepdim=True) + self.eps)
    
    def forward(self, x: torch.Tensor) -> torch.Tensor:
        output = self._norm(x.float()).type_as(x)
        return output * self.weight


def precompute_freqs_cis(dim: int, end: int, theta: float = 10000.0) -> torch.Tensor:
    """Precompute rotary positional embeddings (RoPE)"""
    freqs = 1.0 / (theta ** (torch.arange(0, dim, 2)[: (dim // 2)].float() / dim))
    t = torch.arange(end, device=freqs.device)
    freqs = torch.outer(t, freqs).float()
    freqs_cis = torch.polar(torch.ones_like(freqs), freqs)
    return freqs_cis


def reshape_for_broadcast(freqs_cis: torch.Tensor, x: torch.Tensor) -> torch.Tensor:
    """Reshape frequency tensor for broadcasting with x"""
    ndim = x.ndim
    assert 1 < ndim
    # freqs_cis has shape [seq_len, .., dim//2], x has shape [bsz, seq_len, heads, dim//2]
    # we need to reshape freqs_cis to [1, seq_len, 1, dim//2] for broadcasting
    bs = x.shape[0]  # batch size
    seq_len = x.shape[1]  # sequence length
    dims = [1] * ndim  # Start with all dimensions as 1
    dims[1] = seq_len  # Place seq_len in the second dimension
    dims[-1] = freqs_cis.shape[-1]  # Place freq dim in the last dimension
    return freqs_cis.view(*dims)


def apply_rotary_emb(
    xq: torch.Tensor,
    xk: torch.Tensor,
    freqs_cis: torch.Tensor,
) -> Tuple[torch.Tensor, torch.Tensor]:
    """Apply rotary positional embeddings to query and key tensors"""
    xq_ = torch.view_as_complex(xq.float().reshape(*xq.shape[:-1], -1, 2))
    xk_ = torch.view_as_complex(xk.float().reshape(*xk.shape[:-1], -1, 2))
    freqs_cis = reshape_for_broadcast(freqs_cis, xq_)
    xq_out = torch.view_as_real(xq_ * freqs_cis).flatten(3)
    xk_out = torch.view_as_real(xk_ * freqs_cis).flatten(3)
    return xq_out.type_as(xq), xk_out.type_as(xk)


class Llama2Attention(nn.Module):
    """Multi-head attention with Grouped-Query Attention (GQA) for Llama2"""
    
    def __init__(self, config: ModelConfig):
        super().__init__()
        self.n_kv_heads = config.n_kv_heads
        self.n_heads = config.n_heads
        self.n_rep = self.n_heads // self.n_kv_heads
        self.head_dim = config.dim // config.n_heads
        
        # Calculate output projection dimensions
        self.k_dim = self.n_kv_heads * self.head_dim
        self.v_dim = self.n_kv_heads * self.head_dim
        self.q_dim = config.n_heads * self.head_dim
        
        # Linear projections for Q, K, V
        self.wq = nn.Linear(config.dim, self.q_dim, bias=False)
        self.wk = nn.Linear(config.dim, self.k_dim, bias=False)
        self.wv = nn.Linear(config.dim, self.v_dim, bias=False)
        self.wo = nn.Linear(self.q_dim, config.dim, bias=False)
        
        # Causal mask to ensure attention is only applied to previous tokens
        self.register_buffer("mask", torch.tril(torch.ones(config.max_seq_len, config.max_seq_len)).view(1, 1, config.max_seq_len, config.max_seq_len))
        
        self.dropout = nn.Dropout(0.1)
    
    def forward(
        self,
        x: torch.Tensor,
        freqs_cis: torch.Tensor,
        start_pos: int,
        mask: Optional[torch.Tensor] = None,
    ) -> torch.Tensor:
        bsz, seqlen, _ = x.shape
        xq, xk, xv = self.wq(x), self.wk(x), self.wv(x)
        
        # Reshape to separate heads
        xq = xq.view(bsz, seqlen, self.n_heads, self.head_dim)
        xk = xk.view(bsz, seqlen, self.n_kv_heads, self.head_dim)
        xv = xv.view(bsz, seqlen, self.n_kv_heads, self.head_dim)
        
        # Apply rotary positional embeddings
        xq, xk = apply_rotary_emb(xq, xk, freqs_cis=freqs_cis)
        
        # Repeat K and V to match number of query heads
        if self.n_rep > 1:
            xk = xk.unsqueeze(2).repeat(1, 1, self.n_rep, 1, 1).view(bsz, seqlen, self.n_heads, self.head_dim)
            xv = xv.unsqueeze(2).repeat(1, 1, self.n_rep, 1, 1).view(bsz, seqlen, self.n_heads, self.head_dim)
        
        # Transpose to get (batch, heads, seq_len, head_dim)
        xq = xq.transpose(1, 2)
        xk = xk.transpose(1, 2)
        xv = xv.transpose(1, 2)
        
        # For causal attention, use only tokens up to the current position
        if mask is None:
            mask = torch.zeros((1, 1, seqlen, start_pos + seqlen), device=x.device, dtype=x.dtype)
            mask = mask.masked_fill(mask == 0, float('-inf')).masked_fill(torch.triu(torch.ones_like(mask), diagonal=start_pos + 1) == 1, float('-inf'))
        
        # Compute attention scores
        scores = torch.matmul(xq, xk.transpose(2, 3)) / math.sqrt(self.head_dim)
        scores = scores + mask
        scores = F.softmax(scores.float(), dim=-1).type_as(xq)
        scores = self.dropout(scores)
        
        # Apply attention to values
        output = torch.matmul(scores, xv)
        
        # Reshape back to (batch, seq_len, dim)
        output = output.transpose(1, 2).contiguous().view(bsz, seqlen, -1)
        
        # Output projection
        return self.wo(output)


class Llama2FeedForward(nn.Module):
    """SwiGLU Feed-Forward Network for Llama2"""
    
    def __init__(self, config: ModelConfig):
        super().__init__()
        self.w1 = nn.Linear(config.dim, config.hidden_dim, bias=False)  # Gate
        self.w2 = nn.Linear(config.hidden_dim, config.dim, bias=False)  # Output
        self.w3 = nn.Linear(config.dim, config.hidden_dim, bias=False)  # Up projection
        
    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # SwiGLU activation: x * sigmoid(x) for gate, then multiply with up-projection result
        gate = F.silu(self.w1(x))  # Swish activation
        up = self.w3(x)
        return self.w2(gate * up)


class Llama2DecoderLayer(nn.Module):
    """Complete transformer decoder layer for Llama2"""
    
    def __init__(self, config: ModelConfig):
        super().__init__()
        self.attention = Llama2Attention(config)
        self.feed_forward = Llama2FeedForward(config)
        self.attention_norm = Llama2RMSNorm(config.dim, eps=config.norm_eps)
        self.ffn_norm = Llama2RMSNorm(config.dim, eps=config.norm_eps)
    
    def forward(
        self,
        x: torch.Tensor,
        freqs_cis: torch.Tensor,
        start_pos: int,
        mask: Optional[torch.Tensor] = None,
    ) -> torch.Tensor:
        # Attention with residual connection
        h = x + self.attention(self.attention_norm(x), freqs_cis, start_pos, mask)
        
        # Feed-forward with residual connection
        out = h + self.feed_forward(self.ffn_norm(h))
        
        return out


class Llama2Model(nn.Module):
    """Full Llama2 model with embeddings and output projection"""
    
    def __init__(self, config: ModelConfig):
        super().__init__()
        self.vocab_size = config.vocab_size
        self.n_layers = config.n_layers
        
        self.tok_embeddings = nn.Embedding(config.vocab_size, config.dim)
        self.layers = nn.ModuleList([Llama2DecoderLayer(config) for _ in range(config.n_layers)])
        self.norm = Llama2RMSNorm(config.dim, eps=config.norm_eps)
        self.output = nn.Linear(config.dim, config.vocab_size, bias=False)
        
        # Precompute frequency embeddings
        self.freqs_cis = precompute_freqs_cis(
            config.dim // config.n_heads, config.max_seq_len * 2
        )
        
        # Weight sharing between input and output embeddings
        self.tok_embeddings.weight = self.output.weight
        
        # Initialize weights
        self.apply(self._init_weights)
        # Apply special scaled init to the output projection
        for pn, p in self.named_parameters():
            if pn.endswith('wo.weight') or pn.endswith('w2.weight'):
                # Special scale for output projection weights
                torch.nn.init.normal_(p, mean=0.0, std=0.02/math.sqrt(2 * config.n_layers))
    
    def _init_weights(self, module):
        """Initialize weights for different modules"""
        if isinstance(module, nn.Linear):
            torch.nn.init.normal_(module.weight, mean=0.0, std=0.02)
            if module.bias is not None:
                torch.nn.init.zeros_(module.bias)
        elif isinstance(module, nn.Embedding):
            torch.nn.init.normal_(module.weight, mean=0.0, std=0.02)
        elif isinstance(module, nn.LayerNorm):
            torch.nn.init.zeros_(module.bias)
            torch.nn.init.ones_(module.weight)
    
    def forward(self, tokens: torch.Tensor, start_pos: int = 0) -> torch.Tensor:
        """Forward pass of the Llama2 model"""
        # Get sequence length from input
        _, seq_len = tokens.shape
        
        # Get token embeddings
        h = self.tok_embeddings(tokens)
        
        # Get corresponding frequency embeddings
        freqs_cis = self.freqs_cis[start_pos : start_pos + seq_len]
        
        # Create causal mask to prevent attention to future tokens
        mask = None
        if seq_len > 1:
            mask = torch.full((1, 1, seq_len, seq_len), float("-inf"), device=tokens.device)
            mask = torch.triu(mask, diagonal=start_pos + 1).type_as(h)
        
        # Forward through all decoder layers
        for layer in self.layers:
            h = layer(h, freqs_cis, start_pos, mask)
        
        # Apply final normalization
        h = self.norm(h)
        
        # Output projection to vocabulary
        output = self.output(h).float()
        
        return output


class Llama2ForCausalLM(nn.Module):
    """Llama2 model for causal language modeling"""
    
    def __init__(self, config: ModelConfig):
        super().__init__()
        self.model = Llama2Model(config)
        self.vocab_size = config.vocab_size
    
    def forward(self, input_ids: torch.Tensor, start_pos: int = 0) -> torch.Tensor:
        """Forward pass returning logits for language modeling"""
        return self.model(input_ids, start_pos)
    
    def get_token_logits(self, input_ids: torch.Tensor, start_pos: int = 0) -> torch.Tensor:
        """Get logits for the last token in the sequence"""
        logits = self.model(input_ids, start_pos)
        return logits[:, -1, :]  # Return only the logits for the last token