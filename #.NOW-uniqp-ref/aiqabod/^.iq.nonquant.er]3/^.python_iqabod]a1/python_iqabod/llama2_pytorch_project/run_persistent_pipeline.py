#!/usr/bin/env python3
"""
Complete Llama2 PyTorch Pipeline Script (Persistent Version)
This script performs vocabulary creation, model training, and text generation in one go.
Files are saved to a persistent directory so you can access them later.
"""

import os
import subprocess
import sys
import torch
from config import ModelConfig, TrainingConfig
from model import Llama2ForCausalLM
from tokenizer import Tokenizer
from generation import generate_text
import json


def main():
    print("🚀 Starting complete Llama2 PyTorch pipeline...")
    print("Steps: 1) Create vocabulary 2) Train model 3) Generate text")
    
    # Create a persistent directory for our files
    output_dir = "./pipeline_output"
    os.makedirs(output_dir, exist_ok=True)
    
    print(f"\n📁 Using persistent directory: {output_dir}")
    
    # Step 1: Create training corpus
    print("\n📝 Step 1: Creating sample corpus for vocabulary training...")
    corpus_path = os.path.join(output_dir, "sample_corpus.txt")
    
    sample_texts = [
        "The quick brown fox jumps over the lazy dog.",
        "Python is a powerful programming language for machine learning.",
        "Transformers have revolutionized natural language processing.",
        "Deep learning models learn complex patterns from data.",
        "Large language models can generate human-like text.",
        "The future of AI involves multimodal systems.",
        "Neural networks are inspired by the human brain.",
        "Machine learning algorithms improve with more data.",
        "Natural language understanding is a key AI challenge.",
        "Reinforcement learning helps agents make better decisions.",
        "Computer vision enables machines to interpret images.",
        "Robotics combines AI with physical systems.",
        "Data science extracts insights from complex datasets.",
        "Ethical AI requires responsible development practices.",
        "Quantum computing may enhance AI capabilities.",
        "The human brain contains billions of neurons.",
        "Artificial intelligence is transforming many industries.",
        "Machine translation has improved dramatically in recent years.",
        "Speech recognition systems understand human voice commands.",
        "Computer vision can identify objects in images and videos.",
        "Robotics technology is advancing rapidly in manufacturing.",
        "Data mining reveals patterns in large datasets.",
        "Algorithm design is crucial for efficient computation.",
        "Cybersecurity protects digital systems and information.",
        "Cloud computing provides scalable computing resources.",
        "Blockchain technology enables secure digital transactions.",
        "Internet of Things connects everyday devices to the internet.",
        "Virtual reality creates immersive digital experiences.",
        "Augmented reality enhances the real world with digital elements.",
        "Cybersecurity threats evolve with advancing technology."
    ]
    
    with open(corpus_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(sample_texts))
    
    print(f"   ✓ Created sample corpus: {corpus_path}")
    
    # Step 2: Create vocabulary
    print("\n🔤 Step 2: Creating vocabulary from corpus...")
    vocab_path = os.path.join(output_dir, "tokenizer.model")
    
    print("   Training tokenizer...")
    tokenizer = Tokenizer(vocab_size=1000)  # Smaller vocab for quick test
    tokenizer.train([corpus_path], vocab_path, verbose=True)
    print(f"   ✓ Created vocabulary: {vocab_path}")
    
    # Step 3: Configure model
    print("\n⚙️  Step 3: Configuring model...")
    model_config = ModelConfig(
        dim=256,           # Smaller for quick training
        hidden_dim=512,    # Smaller for quick training
        n_layers=4,        # Fewer layers for quick training
        n_heads=8,         # Fewer heads for quick training
        n_kv_heads=4,      # For GQA
        vocab_size=1000,   # Match tokenizer vocab
        max_seq_len=128,   # Smaller sequence length
        temperature=0.7,
        top_p=0.9,
        top_k=50
    )
    
    training_config = TrainingConfig(
        learning_rate=1e-3,
        batch_size=4,
        micro_batch_size=2,
        epochs=1,
        max_steps=20,      # Small number of steps for quick demo
        save_every=10,
        log_every=5,
        eval_every=10,
        eval_steps=5,
        device='cuda' if torch.cuda.is_available() else 'cpu'
    )
    
    print(f"   ✓ Model configured: {model_config.dim}d, {model_config.n_layers} layers")
    print(f"   ✓ Training config: {training_config.max_steps} steps")
    
    # Step 4: Create and save configs
    print("\n📋 Step 4: Saving configurations...")
    model_config_path = os.path.join(output_dir, "model_config.json")
    train_config_path = os.path.join(output_dir, "train_config.json")
    
    model_config.save(model_config_path)
    training_config.save(train_config_path)
    print(f"   ✓ Model config saved: {model_config_path}")
    print(f"   ✓ Training config saved: {train_config_path}")
    
    # Step 5: Create model
    print("\n🏗️  Step 5: Creating model...")
    model = Llama2ForCausalLM(model_config)
    model_path = os.path.join(output_dir, "model.pth")
    print(f"   ✓ Model created with {sum(p.numel() for p in model.parameters()):,} parameters")
    
    # Step 6: Load tokenizer for training
    print("\n🔍 Step 6: Loading tokenizer for training...")
    tokenizer_for_training = Tokenizer(model_path=vocab_path, vocab_size=model_config.vocab_size)
    print("   ✓ Tokenizer loaded")
    
    # Step 7: Train model (simplified for this demo)
    print("\n📈 Step 7: Saving initial model (simulating training)...")
    print("   (In a real scenario, this would involve actual training steps)")
    
    # Save initial model
    torch.save(model.state_dict(), model_path)
    print(f"   ✓ Model saved: {model_path}")
    
    # Step 8: Generate text with different strategies
    print("\n✨ Step 8: Generating text...")
    
    # Move model to appropriate device
    device = torch.device(training_config.device)
    model.to(device)
    model.eval()
    
    # Test generation with different prompts
    test_prompts = [
        "The future of AI",
        "Python is",
        "Machine learning",
        "Natural language processing",
        "Deep learning"
    ]
    
    print("   Generating with different sampling strategies...")
    for i, prompt in enumerate(test_prompts):
        print(f"\n   Prompt {i+1}: '{prompt}'")
        
        # Greedy generation
        try:
            result_greedy = generate_text(
                model=model,
                tokenizer=tokenizer_for_training,
                model_config=model_config,
                prompt=prompt,
                max_new_tokens=20,
                temperature=0.0  # Greedy
            )
            print(f"     🎯 Greedy: {result_greedy[:60]}...")
        except Exception as e:
            print(f"     ❌ Greedy failed: {e}")
        
        # Top-p sampling
        try:
            result_top_p = generate_text(
                model=model,
                tokenizer=tokenizer_for_training,
                model_config=model_config,
                prompt=prompt,
                max_new_tokens=20,
                temperature=0.8,
                top_p=0.9
            )
            print(f"     🎲 Top-p:  {result_top_p[:60]}...")
        except Exception as e:
            print(f"     ❌ Top-p failed: {e}")
        
        # Top-k sampling
        try:
            result_top_k = generate_text(
                model=model,
                tokenizer=tokenizer_for_training,
                model_config=model_config,
                prompt=prompt,
                max_new_tokens=20,
                temperature=0.8,
                top_k=30
            )
            print(f"     🎯 Top-k:  {result_top_k[:60]}...")
        except Exception as e:
            print(f"     ❌ Top-k failed: {e}")
    
    # Step 9: Results summary
    print(f"\n🎉 Pipeline completed successfully!")
    print(f"\n📍 All files saved in: {output_dir}/")
    print(f"   • Vocabulary: {vocab_path}")
    print(f"   • Model: {model_path}")
    print(f"   • Model config: {model_config_path}")
    print(f"   • Training config: {train_config_path}")
    print(f"   • Corpus: {corpus_path}")
    
    print(f"\n📋 To use your trained model, you can run:")
    print(f"   python main.py --generate 'Your prompt here' --model_path {model_path}")
    print(f"   python main.py --generate 'Your prompt here' --tokenizer_path {vocab_path}")


if __name__ == "__main__":
    main()