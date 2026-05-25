#!/usr/bin/env python3
"""
Complete Llama2 PyTorch Pipeline Script with Training
This script performs vocabulary creation, model training, and text generation in one go.
"""

import os
import subprocess
import sys
import torch
import torch.nn.functional as F
from torch.utils.data import DataLoader, TensorDataset
from config import ModelConfig, TrainingConfig
from model import Llama2ForCausalLM
from tokenizer import Tokenizer
from data import load_data_from_file
from generation import generate_text
import json


def main():
    print("🚀 Starting complete Llama2 PyTorch pipeline with REAL training...")
    print("Steps: 1) Create vocabulary 2) TRAIN model 3) Generate text")
    
    # Create a persistent directory for our files
    output_dir = "./trained_pipeline_output"
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
    ]
    
    with open(corpus_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(sample_texts))
    
    print(f"   ✓ Created sample corpus: {corpus_path}")
    
    # Step 2: Create vocabulary
    print("\n🔤 Step 2: Creating vocabulary from corpus...")
    vocab_path = os.path.join(output_dir, "tokenizer.model")
    
    print("   Training tokenizer...")
    tokenizer = Tokenizer(vocab_size=500)  # Smaller vocab for quick training
    tokenizer.train([corpus_path], vocab_path, verbose=True)
    print(f"   ✓ Created vocabulary: {vocab_path}")
    
    # Step 3: Configure model
    print("\n⚙️  Step 3: Configuring model...")
    model_config = ModelConfig(
        dim=128,            # Much smaller for quick training
        hidden_dim=256,     # Smaller for quick training
        n_layers=2,         # Fewer layers for quick training
        n_heads=4,          # Fewer heads for quick training
        n_kv_heads=2,       # For GQA
        vocab_size=500,     # Match tokenizer vocab
        max_seq_len=64,     # Smaller sequence length
        temperature=0.7,
        top_p=0.9,
        top_k=50
    )
    
    training_config = TrainingConfig(
        learning_rate=1e-3,
        batch_size=4,
        micro_batch_size=2,
        epochs=3,           # Multiple epochs for better training
        max_steps=50,       # Limited steps for demo
        save_every=10,
        log_every=5,
        eval_every=20,
        eval_steps=5,
        device='cuda' if torch.cuda.is_available() else 'cpu'
    )
    
    print(f"   ✓ Model configured: {model_config.dim}d, {model_config.n_layers} layers")
    print(f"   ✓ Training config: {training_config.max_steps} steps, {training_config.epochs} epochs")
    
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
    
    # Step 6: Load tokenizer with correct vocab size
    print("\n🔍 Step 6: Loading tokenizer for training...")
    tokenizer_for_training = Tokenizer(model_path=vocab_path, vocab_size=model_config.vocab_size)
    print("   ✓ Tokenizer loaded")
    
    # Step 7: Prepare training data
    print("\n📚 Step 7: Preparing training data...")
    # Tokenize the corpus
    with open(corpus_path, 'r', encoding='utf-8') as f:
        text = f.read()
    encoded = tokenizer_for_training.encode(text)
    
    # Create training sequences (input and target)
    seq_len = model_config.max_seq_len
    sequences = []
    for i in range(0, len(encoded) - seq_len, seq_len // 2):  # Overlapping windows
        seq = encoded[i:i + seq_len + 1]  # +1 for target
        if len(seq) == seq_len + 1:
            sequences.append(seq)
    
    # Convert to tensors
    if not sequences:
        print("   ⚠ Not enough data to create sequences, creating minimal data...")
        # Create some minimal training data
        sample_sequence = encoded[:seq_len + 1]
        sequences = [sample_sequence + [0] * (seq_len + 1 - len(sample_sequence)) if len(sample_sequence) < seq_len + 1 else sample_sequence]
    
    sequences = [seq[:seq_len + 1] for seq in sequences if len(seq) >= seq_len + 1]
    
    if not sequences:
        print("   ❌ No valid sequences could be created from the corpus")
        return
    
    input_data = torch.tensor([seq[:-1] for seq in sequences], dtype=torch.long)  # all but last
    target_data = torch.tensor([seq[1:] for seq in sequences], dtype=torch.long)  # all but first
    
    train_dataset = TensorDataset(input_data, target_data)
    train_loader = DataLoader(train_dataset, batch_size=training_config.batch_size, shuffle=True)
    
    print(f"   ✓ Created {len(sequences)} training sequences")
    print(f"   ✓ Data shape: {input_data.shape}, Targets shape: {target_data.shape}")
    
    # Step 8: Train the model (actual training this time!)
    print(f"\n🏋️  Step 8: Training model for {training_config.epochs} epochs...")
    
    optimizer = torch.optim.AdamW(
        model.parameters(), 
        lr=training_config.learning_rate,
        weight_decay=training_config.weight_decay,
        eps=training_config.eps
    )
    
    # Move model to device
    device = torch.device(training_config.device)
    model.to(device)
    model.train()
    
    print(f"   Training on device: {device}")
    
    total_steps = 0
    for epoch in range(training_config.epochs):
        print(f"   Epoch {epoch + 1}/{training_config.epochs}")
        epoch_loss = 0
        steps_in_epoch = 0
        
        for batch_idx, (inputs, targets) in enumerate(train_loader):
            if total_steps >= training_config.max_steps:
                break
                
            inputs, targets = inputs.to(device), targets.to(device)
            
            optimizer.zero_grad()
            
            # Forward pass
            logits = model(inputs).float()
            
            # Compute loss
            loss = F.cross_entropy(
                logits.view(-1, logits.size(-1)), 
                targets.reshape(-1), 
                ignore_index=-100
            )
            
            # Backward pass
            loss.backward()
            
            # Gradient clipping
            torch.nn.utils.clip_grad_norm_(model.parameters(), training_config.gradient_clip_val)
            
            # Update weights
            optimizer.step()
            
            epoch_loss += loss.item()
            steps_in_epoch += 1
            total_steps += 1
            
            if total_steps % training_config.log_every == 0:
                print(f"     Step {total_steps}, Loss: {loss.item():.4f}")
            
            if total_steps >= training_config.max_steps:
                break
        
        avg_loss = epoch_loss / max(steps_in_epoch, 1)
        print(f"   Epoch {epoch + 1} completed. Average Loss: {avg_loss:.4f}")
    
    # Save trained model
    torch.save(model.state_dict(), model_path)
    print(f"   ✓ Trained model saved: {model_path}")
    
    # Step 9: Generate text with trained model
    print("\n✨ Step 9: Generating text with TRAINED model...")
    
    model.eval()
    
    # Test generation with different prompts
    test_prompts = [
        "The",
        "Python",
        "AI",
        "Machine"
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
                max_new_tokens=15,
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
                max_new_tokens=15,
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
                max_new_tokens=15,
                temperature=0.8,
                top_k=20
            )
            print(f"     🎯 Top-k:  {result_top_k[:60]}...")
        except Exception as e:
            print(f"     ❌ Top-k failed: {e}")
    
    # Step 10: Results summary
    print(f"\n🎉 Pipeline completed successfully with real training!")
    print(f"\n📍 All files saved in: {output_dir}/")
    print(f"   • Vocabulary: {vocab_path}")
    print(f"   • Trained Model: {model_path}")
    print(f"   • Model config: {model_config_path}")
    print(f"   • Training config: {train_config_path}")
    print(f"   • Corpus: {corpus_path}")
    
    print(f"\n📋 To use your trained model, you can run:")
    print(f"   python main.py --generate 'Your prompt here' --model_path {model_path}")
    print(f"   python main.py --generate 'Your prompt here' --tokenizer_path {vocab_path}")
    
    print(f"\n💡 The model has now been trained on your corpus and should generate more coherent text!")


if __name__ == "__main__":
    main()