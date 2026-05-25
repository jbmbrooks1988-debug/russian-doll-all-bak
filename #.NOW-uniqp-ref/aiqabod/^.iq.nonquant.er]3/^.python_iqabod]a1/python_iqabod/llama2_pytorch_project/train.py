"""Training script for Llama2 model"""
import argparse
import torch
import os
from model import Llama2ForCausalLM
from tokenizer import Tokenizer
from config import ModelConfig, TrainingConfig, DEFAULT_MODEL_CONFIG, DEFAULT_TRAINING_CONFIG
from training import train_model
from data import load_data_from_corpus, create_tokenizer_from_corpus, CorpusDataset


def main():
    parser = argparse.ArgumentParser(description="Train Llama2 Model")
    parser.add_argument("--train_data", type=str, required=True, help="Path to training data directory")
    parser.add_argument("--val_data", type=str, default=None, help="Path to validation data directory")
    parser.add_argument("--model_config", type=str, default=None, help="Path to model config file")
    parser.add_argument("--train_config", type=str, default=None, help="Path to training config file")
    parser.add_argument("--tokenizer_path", type=str, default="tokenizer.model", help="Path to tokenizer file")
    parser.add_argument("--model_path", type=str, default=None, help="Path to pre-trained model to continue training")
    parser.add_argument("--output_dir", type=str, default="checkpoints", help="Directory to save model checkpoints")
    parser.add_argument("--vocab_size", type=int, default=32000, help="Vocabulary size")
    parser.add_argument("--max_seq_len", type=int, default=2048, help="Maximum sequence length")
    
    args = parser.parse_args()
    
    # Load configurations
    if args.model_config and os.path.exists(args.model_config):
        model_config = ModelConfig.load(args.model_config)
    else:
        model_config = DEFAULT_MODEL_CONFIG
        # Override with command line args
        model_config.vocab_size = args.vocab_size
        model_config.max_seq_len = args.max_seq_len
    
    if args.train_config and os.path.exists(args.train_config):
        training_config = TrainingConfig.load(args.train_config)
    else:
        training_config = DEFAULT_TRAINING_CONFIG
        # Override with command line args
        training_config.train_file = args.train_data
        training_config.val_file = args.val_data
    
    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Load or create tokenizer
    if os.path.exists(args.tokenizer_path):
        print(f"Loading tokenizer from {args.tokenizer_path}")
        tokenizer = Tokenizer(model_path=args.tokenizer_path, vocab_size=model_config.vocab_size)
    else:
        print(f"Tokenizer not found at {args.tokenizer_path}, creating from training data...")
        # Create tokenizer from training data
        tokenizer = create_tokenizer_from_corpus(
            [os.path.join(args.train_data, f) for f in os.listdir(args.train_data) 
             if f.endswith(('.txt', '.json', '.csv'))],
            model_config.vocab_size,
            args.tokenizer_path
        )
    
    # Load training dataset
    print(f"Loading training data from {args.train_data}")
    train_dataset = CorpusDataset(
        corpus_dir=args.train_data,
        tokenizer=tokenizer,
        max_length=model_config.max_seq_len
    )
    
    # Load validation dataset if provided
    val_dataset = None
    if args.val_data:
        print(f"Loading validation data from {args.val_data}")
        val_dataset = CorpusDataset(
            corpus_dir=args.val_data,
            tokenizer=tokenizer,
            max_length=model_config.max_seq_len
        )
    
    # Create model
    print("Creating model...")
    model = Llama2ForCausalLM(model_config)
    
    # Load pre-trained model if provided
    if args.model_path and os.path.exists(args.model_path):
        print(f"Loading pre-trained model from {args.model_path}")
        model.load_state_dict(torch.load(args.model_path, map_location=model_config.device))
    
    # Print model size
    total_params = sum(p.numel() for p in model.parameters())
    trainable_params = sum(p.numel() for p in model.parameters() if p.requires_grad)
    print(f"Model created with {total_params:,} total parameters")
    print(f"Trainable parameters: {trainable_params:,}")
    
    # Start training
    print("Starting training...")
    trainer = train_model(
        model_config=model_config,
        training_config=training_config,
        train_dataset=train_dataset,
        val_dataset=val_dataset
    )
    
    print("Training completed!")


if __name__ == "__main__":
    main()