"""Main entry point for Llama2 PyTorch implementation"""
import argparse
import torch
import os
from model import Llama2ForCausalLM
from tokenizer import Tokenizer
from config import ModelConfig, TrainingConfig, DEFAULT_MODEL_CONFIG, DEFAULT_TRAINING_CONFIG
from training import train_model
from generation import generate_text
from data import load_data_from_corpus, create_tokenizer_from_corpus


def main():
    parser = argparse.ArgumentParser(description="Llama2 PyTorch Implementation")
    parser.add_argument("--vocab", type=str, help="Path to vocabulary file for tokenization")
    parser.add_argument("--train", type=str, help="Path to training data")
    parser.add_argument("--generate", type=str, help="Text to generate from")
    parser.add_argument("--model_path", type=str, default=None, help="Path to pre-trained model")
    parser.add_argument("--tokenizer_path", type=str, default="tokenizer.model", help="Path to tokenizer file")
    parser.add_argument("--config_path", type=str, default=None, help="Path to model config file")
    parser.add_argument("--train_config_path", type=str, default=None, help="Path to training config file")
    parser.add_argument("--max_new_tokens", type=int, default=256, help="Max new tokens to generate")
    parser.add_argument("--temperature", type=float, default=0.6, help="Temperature for generation")
    parser.add_argument("--top_p", type=float, default=0.9, help="Top-p for generation")
    parser.add_argument("--top_k", type=int, default=50, help="Top-k for generation")
    
    args = parser.parse_args()
    
    # Load model configuration
    if args.config_path and os.path.exists(args.config_path):
        model_config = ModelConfig.load(args.config_path)
    else:
        model_config = DEFAULT_MODEL_CONFIG
    
    # Load training configuration if in training mode
    if args.train_config_path and os.path.exists(args.train_config_path):
        training_config = TrainingConfig.load(args.train_config_path)
    else:
        training_config = DEFAULT_TRAINING_CONFIG
    
    # Handle vocabulary creation/training
    if args.vocab:
        print(f"Creating tokenizer from vocabulary: {args.vocab}")
        # Create tokenizer from corpus
        tokenizer = create_tokenizer_from_corpus([args.vocab], model_config.vocab_size, args.tokenizer_path)
        print(f"Tokenizer saved to {args.tokenizer_path}")
        return
    
    # Load tokenizer
    if os.path.exists(args.tokenizer_path):
        tokenizer = Tokenizer(model_path=args.tokenizer_path, vocab_size=model_config.vocab_size)
        print(f"Loaded tokenizer from {args.tokenizer_path}")
    else:
        # Create default tokenizer
        tokenizer = Tokenizer(vocab_size=model_config.vocab_size)
        print("Created default tokenizer")
    
    # Determine which mode to run
    if args.train:
        print("Starting training mode...")
        # Load training data
        train_loader = load_data_from_corpus(
            corpus_dir=args.train,
            tokenizer=tokenizer,
            max_length=model_config.max_seq_len,
            batch_size=training_config.batch_size
        )
        
        # Create model
        model = Llama2ForCausalLM(model_config)
        
        if args.model_path and os.path.exists(args.model_path):
            print(f"Loading model from {args.model_path}")
            model.load_state_dict(torch.load(args.model_path, map_location=model_config.device))
        
        # Start training
        trainer = train_model(
            model_config=model_config,
            training_config=training_config,
            train_dataset=train_loader.dataset
        )
        
    elif args.generate:
        print("Starting generation mode...")
        # Create model
        model = Llama2ForCausalLM(model_config)
        
        if args.model_path and os.path.exists(args.model_path):
            print(f"Loading model from {args.model_path}")
            model.load_state_dict(torch.load(args.model_path, map_location=model_config.device))
        
        # Move model to appropriate device
        device = torch.device(model_config.device)
        model.to(device)
        model.eval()
        
        # Generate text
        generated_text = generate_text(
            model=model,
            tokenizer=tokenizer,
            model_config=model_config,
            prompt=args.generate,
            max_new_tokens=args.max_new_tokens,
            temperature=args.temperature,
            top_p=args.top_p,
            top_k=args.top_k
        )
        
        print(f"Generated text: {generated_text}")
        
    else:
        print("Please specify one of the following:")
        print("  --vocab PATH: To create and train a tokenizer")
        print("  --train PATH: To train the model")
        print("  --generate TEXT: To generate text from a prompt")
        parser.print_help()


if __name__ == "__main__":
    main()