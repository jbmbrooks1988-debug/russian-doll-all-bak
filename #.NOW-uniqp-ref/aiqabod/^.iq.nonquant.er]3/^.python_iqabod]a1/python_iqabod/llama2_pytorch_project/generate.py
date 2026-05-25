"""Text generation script for Llama2 model"""
import argparse
import torch
import os
from model import Llama2ForCausalLM
from tokenizer import Tokenizer
from config import ModelConfig, DEFAULT_MODEL_CONFIG
from generation import generate_text, Generator


def main():
    parser = argparse.ArgumentParser(description="Generate text with Llama2 Model")
    parser.add_argument("--prompt", type=str, required=True, help="Input prompt for text generation")
    parser.add_argument("--model_config", type=str, default=None, help="Path to model config file")
    parser.add_argument("--model_path", type=str, required=True, help="Path to pre-trained model")
    parser.add_argument("--tokenizer_path", type=str, required=True, help="Path to tokenizer file")
    parser.add_argument("--max_new_tokens", type=int, default=256, help="Maximum number of new tokens to generate")
    parser.add_argument("--temperature", type=float, default=0.6, help="Sampling temperature")
    parser.add_argument("--top_p", type=float, default=0.9, help="Top-p sampling parameter")
    parser.add_argument("--top_k", type=int, default=50, help="Top-k sampling parameter")
    parser.add_argument("--output_file", type=str, default=None, help="File to save generated text")
    parser.add_argument("--echo", action="store_true", help="Include prompt in output")
    
    args = parser.parse_args()
    
    # Load model configuration
    if args.model_config and os.path.exists(args.model_config):
        model_config = ModelConfig.load(args.model_config)
    else:
        model_config = DEFAULT_MODEL_CONFIG
    
    # Load tokenizer
    if os.path.exists(args.tokenizer_path):
        tokenizer = Tokenizer(model_path=args.tokenizer_path, vocab_size=model_config.vocab_size)
        print(f"Loaded tokenizer from {args.tokenizer_path}")
    else:
        raise FileNotFoundError(f"Tokenizer file not found: {args.tokenizer_path}")
    
    # Create model
    print("Creating model...")
    model = Llama2ForCausalLM(model_config)
    
    # Load pre-trained model
    if os.path.exists(args.model_path):
        print(f"Loading model from {args.model_path}")
        model.load_state_dict(torch.load(args.model_path, map_location=model_config.device))
    else:
        raise FileNotFoundError(f"Model file not found: {args.model_path}")
    
    # Move model to appropriate device
    device = torch.device(model_config.device)
    model.to(device)
    model.eval()
    
    print(f"Generating text with prompt: '{args.prompt}'")
    
    # Generate text
    generated_text = generate_text(
        model=model,
        tokenizer=tokenizer,
        model_config=model_config,
        prompt=args.prompt,
        max_new_tokens=args.max_new_tokens,
        temperature=args.temperature,
        top_p=args.top_p,
        top_k=args.top_k
    )
    
    # Prepare output
    if args.echo:
        output_text = args.prompt + generated_text
    else:
        output_text = generated_text
    
    print("\nGenerated text:")
    print(output_text)
    
    # Save to file if specified
    if args.output_file:
        with open(args.output_file, 'w', encoding='utf-8') as f:
            f.write(output_text)
        print(f"\nGenerated text saved to {args.output_file}")
    
    return output_text


if __name__ == "__main__":
    main()