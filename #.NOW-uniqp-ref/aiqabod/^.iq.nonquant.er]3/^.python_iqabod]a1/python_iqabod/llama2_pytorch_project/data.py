"""Data loading and preprocessing utilities for Llama2 model"""
import os
import torch
from torch.utils.data import Dataset, DataLoader
from typing import List, Optional, Union
import json
from tokenizer import Tokenizer, BPETokenizer


class TextDataset(Dataset):
    """Dataset class for text data"""
    
    def __init__(
        self, 
        texts: List[str], 
        tokenizer: Tokenizer, 
        max_length: int = 2048,
        stride: Optional[int] = None
    ):
        self.tokenizer = tokenizer
        self.max_length = max_length
        self.stride = stride or max_length // 2  # Overlap by half the sequence length
        
        # Tokenize all texts
        self.encoded_texts = []
        for text in texts:
            # Encode the text
            encoded = self.tokenizer.encode(text, bos=True, eos=True)
            # Break into chunks of max_length
            for i in range(0, len(encoded), self.stride):
                chunk = encoded[i:i + self.max_length]
                # Pad if necessary
                if len(chunk) < self.max_length:
                    chunk += [self.tokenizer.tokenizer.pad_id] * (self.max_length - len(chunk))
                self.encoded_texts.append(torch.tensor(chunk, dtype=torch.long))
    
    def __len__(self):
        return len(self.encoded_texts)
    
    def __getitem__(self, idx):
        return self.encoded_texts[idx]


class FileDataset(Dataset):
    """Dataset class that loads data directly from files"""
    
    def __init__(
        self,
        file_path: str,
        tokenizer: Tokenizer,
        max_length: int = 2048,
        stride: Optional[int] = None
    ):
        self.tokenizer = tokenizer
        self.max_length = max_length
        self.stride = stride or max_length // 2
        
        # Load and tokenize the file
        with open(file_path, 'r', encoding='utf-8') as f:
            text = f.read()
        
        self.encoded_text = self.tokenizer.encode(text, bos=True, eos=True)
        self.encoded_chunks = []
        
        # Break into chunks
        for i in range(0, len(self.encoded_text), self.stride):
            chunk = self.encoded_text[i:i + self.max_length]
            # Pad if necessary
            if len(chunk) < self.max_length:
                chunk += [self.tokenizer.tokenizer.pad_id] * (self.max_length - len(chunk))
            self.encoded_chunks.append(torch.tensor(chunk, dtype=torch.long))
    
    def __len__(self):
        return len(self.encoded_chunks)
    
    def __getitem__(self, idx):
        return self.encoded_chunks[idx]


class CorpusDataset(Dataset):
    """Dataset that loads from a corpus directory"""
    
    def __init__(
        self,
        corpus_dir: str,
        tokenizer: Tokenizer,
        max_length: int = 2048,
        stride: Optional[int] = None,
        file_extension: str = '.txt'
    ):
        self.tokenizer = tokenizer
        self.max_length = max_length
        self.stride = stride or max_length // 2
        self.files = []
        
        # Find all text files in corpus directory
        for root, dirs, files in os.walk(corpus_dir):
            for file in files:
                if file.endswith(file_extension):
                    self.files.append(os.path.join(root, file))
        
        # Load and tokenize all files
        self.encoded_chunks = []
        for file_path in self.files:
            with open(file_path, 'r', encoding='utf-8') as f:
                text = f.read()
            
            encoded = self.tokenizer.encode(text, bos=True, eos=True)
            
            # Break into chunks
            for i in range(0, len(encoded), self.stride):
                chunk = encoded[i:i + self.max_length]
                # Pad if necessary
                if len(chunk) < self.max_length:
                    chunk += [self.tokenizer.tokenizer.pad_id] * (self.max_length - len(chunk))
                self.encoded_chunks.append(torch.tensor(chunk, dtype=torch.long))
    
    def __len__(self):
        return len(self.encoded_chunks)
    
    def __getitem__(self, idx):
        return self.encoded_chunks[idx]


def create_tokenizer_from_corpus(
    corpus_paths: List[str], 
    vocab_size: int = 32000,
    tokenizer_path: str = "tokenizer.model"
) -> Tokenizer:
    """Create and train a tokenizer on corpus text files"""
    # Read all corpus files
    all_texts = []
    for path in corpus_paths:
        with open(path, 'r', encoding='utf-8') as f:
            all_texts.append(f.read())
    
    # Create and train tokenizer
    tokenizer = Tokenizer(vocab_size=vocab_size)
    tokenizer.train(all_texts, tokenizer_path, verbose=True)
    
    return tokenizer


def get_dataloader(
    dataset: Dataset,
    batch_size: int = 16,
    shuffle: bool = True,
    num_workers: int = 2
) -> DataLoader:
    """Create a DataLoader for the given dataset"""
    return DataLoader(
        dataset,
        batch_size=batch_size,
        shuffle=shuffle,
        num_workers=num_workers,
        pin_memory=True
    )


def load_data_from_corpus(
    corpus_dir: str,
    tokenizer: Tokenizer,
    max_length: int = 2048,
    batch_size: int = 16,
    shuffle: bool = True
) -> DataLoader:
    """Load data from corpus directory and return a DataLoader"""
    dataset = CorpusDataset(
        corpus_dir=corpus_dir,
        tokenizer=tokenizer,
        max_length=max_length
    )
    
    return get_dataloader(dataset, batch_size, shuffle)


def load_data_from_file(
    file_path: str,
    tokenizer: Tokenizer,
    max_length: int = 2048,
    batch_size: int = 16,
    shuffle: bool = True
) -> DataLoader:
    """Load data from a single file and return a DataLoader"""
    dataset = FileDataset(
        file_path=file_path,
        tokenizer=tokenizer,
        max_length=max_length
    )
    
    return get_dataloader(dataset, batch_size, shuffle)


def load_data_from_texts(
    texts: List[str],
    tokenizer: Tokenizer,
    max_length: int = 2048,
    batch_size: int = 16,
    shuffle: bool = True
) -> DataLoader:
    """Load data from a list of text strings and return a DataLoader"""
    dataset = TextDataset(
        texts=texts,
        tokenizer=tokenizer,
        max_length=max_length
    )
    
    return get_dataloader(dataset, batch_size, shuffle)


# For backward compatibility
Dataset = TextDataset