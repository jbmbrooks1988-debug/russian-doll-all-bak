"""BPE Tokenizer for Llama2 model"""
import regex as re
import json
from typing import List, Dict, Optional, Tuple
import os


def get_regex_pattern():
    """Returns the regex pattern for tokenization."""
    # This is the same pattern used in GPT-2/GPT-4 tokenizers
    return r"""'s|'t|'re|'ve|'m|'ll|'d| ?\p{L}+| ?\p{N}+| ?[^\s\p{L}\p{N}]+|\s+(?!\S)|\s+"""


class BPETokenizer:
    """Byte-Pair Encoding tokenizer implementation for Llama2"""
    
    def __init__(self, vocab_size: int = 32000):
        self.vocab_size = vocab_size
        
        # Special tokens
        self.bos_id = 1  # Beginning of sentence
        self.eos_id = 2  # End of sentence
        self.unk_id = 0  # Unknown token
        self.pad_id = 0  # Padding token
        
        # Vocabulary mappings
        self.token_to_id: Dict[str, int] = {}
        self.id_to_token: Dict[int, str] = {}
        
        # Initialize basic vocabulary
        self._initialize_vocab()
        
    def _initialize_vocab(self):
        """Initialize the basic vocabulary with special tokens and byte tokens"""
        # Add special tokens
        special_tokens = {
            "<unk>": self.unk_id,
            "<s>": self.bos_id,
            "</s>": self.eos_id,
            "<pad>": self.pad_id
        }
        
        for token, token_id in special_tokens.items():
            self.token_to_id[token] = token_id
            self.id_to_token[token_id] = token
            
        # Add byte tokens (256 possible bytes)
        for i in range(256):
            byte_token = chr(i)
            if byte_token not in self.token_to_id:
                token_id = len(self.token_to_id)
                self.token_to_id[byte_token] = token_id
                self.id_to_token[token_id] = byte_token
    
    def _get_stats(self, ids: List[int]) -> Dict[Tuple[int, int], int]:
        """Count frequency of consecutive pairs in the list of IDs."""
        counts = {}
        for pair in zip(ids, ids[1:]):
            counts[pair] = counts.get(pair, 0) + 1
        return counts
    
    def _merge(self, ids: List[int], pair: Tuple[int, int], new_id: int) -> List[int]:
        """Merge consecutive occurrences of a pair into a new token."""
        new_ids = []
        i = 0
        while i < len(ids):
            if i < len(ids) - 1 and (ids[i], ids[i+1]) == pair:
                new_ids.append(new_id)
                i += 2
            else:
                new_ids.append(ids[i])
                i += 1
        return new_ids
    
    def _encode_chunk(self, text: str) -> List[int]:
        """Encode a single text chunk using BPE."""
        # First, convert to bytes and then to tokens
        tokens = []
        for char in text:
            byte_token = char
            if byte_token in self.token_to_id:
                tokens.append(self.token_to_id[byte_token])
            else:
                # Use unknown token
                tokens.append(self.unk_id)
        return tokens
    
    def train(self, texts: List[str], vocab_size: int, verbose: bool = False):
        """Train BPE tokenizer on a list of texts."""
        # Pre-tokenize texts
        pre_tokenized_texts = []
        for text in texts:
            # Use regex to split text into chunks
            chunks = re.findall(get_regex_pattern(), text)
            pre_tokenized_texts.extend(chunks)
        
        # Convert each chunk to initial tokens (bytes)
        all_ids = []
        for chunk in pre_tokenized_texts:
            ids = self._encode_chunk(chunk)
            # Add a special separator token between sequences
            all_ids.extend(ids + [self.eos_id])
        
        # Start with byte-level tokens and build up
        num_merges = vocab_size - len(self.token_to_id)
        
        for i in range(num_merges):
            # Get frequencies of all pairs
            stats = self._get_stats(all_ids)
            if not stats:
                break
                
            # Get the most frequent pair
            pair = max(stats, key=stats.get)
            
            # Create a new token ID for the pair
            new_token_id = len(self.token_to_id)
            self.token_to_id[f"token_{pair[0]}_{pair[1]}"] = new_token_id
            self.id_to_token[new_token_id] = f"{self.id_to_token[pair[0]]}{self.id_to_token[pair[1]]}"
            
            # Merge the pair in the sequence
            all_ids = self._merge(all_ids, pair, new_token_id)
            
            if verbose and i % 1000 == 0:
                print(f"Merged {i}/{num_merges}: {pair} -> {new_token_id}")
    
    def encode(self, text: str) -> List[int]:
        """Encode text to token IDs."""
        # Apply regex to split text
        chunks = re.findall(get_regex_pattern(), text)
        
        ids = []
        for chunk in chunks:
            # For simplicity in this implementation, we'll use a basic approach
            # In a more advanced implementation, we would apply the learned BPE merges
            chunk_ids = self._encode_chunk(chunk)
            ids.extend(chunk_ids)
        
        return ids
    
    def decode(self, ids: List[int]) -> str:
        """Decode token IDs back to text."""
        tokens = []
        for token_id in ids:
            if token_id in self.id_to_token:
                tokens.append(self.id_to_token[token_id])
            else:
                tokens.append(self.id_to_token[self.unk_id])
        
        # Join tokens and convert back from bytes if necessary
        text = "".join(tokens)
        return text
    
    def save(self, file_path: str):
        """Save tokenizer to file."""
        tokenizer_data = {
            'vocab_size': self.vocab_size,
            'token_to_id': self.token_to_id,
            'id_to_token': self.id_to_token,
            'bos_id': self.bos_id,
            'eos_id': self.eos_id,
            'unk_id': self.unk_id,
            'pad_id': self.pad_id
        }
        
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(tokenizer_data, f, ensure_ascii=False, indent=2)
    
    def load(self, file_path: str):
        """Load tokenizer from file."""
        with open(file_path, 'r', encoding='utf-8') as f:
            tokenizer_data = json.load(f)
        
        self.vocab_size = tokenizer_data['vocab_size']
        # Convert string keys and values to appropriate types
        self.token_to_id = {}
        for k, v in tokenizer_data['token_to_id'].items():
            # Keep string tokens as strings, convert integer tokens to integers
            token_key = k if isinstance(k, str) else int(k)
            token_value = int(v)
            self.token_to_id[token_key] = token_value
        
        self.id_to_token = {int(k): v for k, v in tokenizer_data['id_to_token'].items()}
        self.bos_id = tokenizer_data['bos_id']
        self.eos_id = tokenizer_data['eos_id']
        self.unk_id = tokenizer_data['unk_id']
        self.pad_id = tokenizer_data['pad_id']


class Tokenizer:
    """Main tokenizer interface for Llama2"""
    
    def __init__(self, model_path: Optional[str] = None, vocab_size: int = 32000):
        if model_path and os.path.exists(model_path):
            self.tokenizer = BPETokenizer(vocab_size)
            self.tokenizer.load(model_path)
        else:
            self.tokenizer = BPETokenizer(vocab_size)
    
    def encode(self, text: str, bos: bool = True, eos: bool = True) -> List[int]:
        """Encode text with optional BOS and EOS tokens."""
        ids = self.tokenizer.encode(text)
        
        if bos:
            ids = [self.tokenizer.bos_id] + ids
        if eos:
            ids = ids + [self.tokenizer.eos_id]
        
        return ids
    
    def decode(self, ids: List[int]) -> str:
        """Decode token IDs back to text."""
        # Remove BOS/EOS tokens if present
        if ids and ids[0] == self.tokenizer.bos_id:
            ids = ids[1:]
        if ids and ids[-1] == self.tokenizer.eos_id:
            ids = ids[:-1]
        
        return self.tokenizer.decode(ids)
    
    def train(self, texts: List[str], vocab_file: str, verbose: bool = False):
        """Train the tokenizer on the provided texts."""
        self.tokenizer.train(texts, self.tokenizer.vocab_size, verbose)
        self.tokenizer.save(vocab_file)
    
    def save(self, file_path: str):
        """Save the tokenizer."""
        self.tokenizer.save(file_path)
    
    def load(self, file_path: str):
        """Load the tokenizer."""
        self.tokenizer.load(file_path)