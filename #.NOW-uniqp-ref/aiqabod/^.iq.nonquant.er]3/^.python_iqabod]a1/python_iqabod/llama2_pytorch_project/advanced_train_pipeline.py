#!/usr/bin/env python3
"""
Advanced Llama2 PyTorch Pipeline Script with Extended Training
This script performs vocabulary creation, comprehensive model training, and text generation in one go.
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
from generation import generate_text
import json
import math


def create_large_corpus():
    """Create a much larger corpus to improve training"""
    base_texts = [
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
    
    # Repeat the texts to create a larger corpus
    large_texts = base_texts * 5  # Multiply to get more data
    
    return '\n'.join(large_texts)


def main():
    print("🚀 Starting ADVANCED Llama2 PyTorch pipeline with EXTENDED training...")
    print("Steps: 1) Create large vocabulary 2) EXTENSIVE training 3) High-quality generation")
    
    # Create a persistent directory for our files
    output_dir = "./advanced_trained_pipeline_output"
    os.makedirs(output_dir, exist_ok=True)
    
    print(f"\n📁 Using persistent directory: {output_dir}")
    
    # Step 1: Create large training corpus
    print("\n📝 Step 1: Creating LARGE sample corpus for vocabulary training...")
    corpus_path = os.path.join(output_dir, "large_corpus.txt")
    
    large_corpus = create_large_corpus()
    with open(corpus_path, 'w', encoding='utf-8') as f:
        f.write(large_corpus)
    
    print(f"   ✓ Created large corpus ({len(large_corpus)} characters): {corpus_path}")
    
    # Step 2: Create vocabulary
    print("\n🔤 Step 2: Creating vocabulary from large corpus...")
    vocab_path = os.path.join(output_dir, "tokenizer.model")
    
    print("   Training tokenizer...")
    tokenizer = Tokenizer(vocab_size=800)  # Larger vocab
    tokenizer.train([corpus_path], vocab_path, verbose=True)
    print(f"   ✓ Created vocabulary: {vocab_path}")
    
    # Step 3: Configure model (larger than before)
    print("\n⚙️  Step 3: Configuring larger model...")
    model_config = ModelConfig(
        dim=256,            # Larger model
        hidden_dim=512,     # Larger hidden dim
        n_layers=4,         # More layers
        n_heads=8,          # More heads
        n_kv_heads=4,       # For GQA
        vocab_size=800,     # Match tokenizer vocab
        max_seq_len=128,    # Longer sequences
        temperature=0.7,
        top_p=0.9,
        top_k=50
    )
    
    training_config = TrainingConfig(
        learning_rate=5e-4,  # Slightly lower learning rate
        batch_size=8,        # Larger batch size
        micro_batch_size=4,
        epochs=5,            # More epochs
        max_steps=200,       # More steps for better training
        save_every=20,
        log_every=10,
        eval_every=50,
        eval_steps=10,
        device='cuda' if torch.cuda.is_available() else 'cpu',
        gradient_clip_val=1.0  # Add gradient clipping
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
    print("\n🏗️  Step 5: Creating larger model...")
    model = Llama2ForCausalLM(model_config)
    model_path = os.path.join(output_dir, "model.pth")
    print(f"   ✓ Model created with {sum(p.numel() for p in model.parameters()):,} parameters")
    
    # Step 6: Load tokenizer with correct vocab size
    print("\n🔍 Step 6: Loading tokenizer for training...")
    tokenizer_for_training = Tokenizer(model_path=vocab_path, vocab_size=model_config.vocab_size)
    print("   ✓ Tokenizer loaded")
    
    # Step 7: Prepare training data with better approach
    print("\n📚 Step 7: Preparing comprehensive training data...")
    
    # Read and tokenize the corpus
    with open(corpus_path, 'r', encoding='utf-8') as f:
        text = f.read()
    encoded = tokenizer_for_training.encode(text)
    
    print(f"   Corpus encoded to {len(encoded)} tokens")
    
    # Create training sequences with sliding window
    seq_len = model_config.max_seq_len
    stride = seq_len // 4  # Overlapping windows to maximize data usage
    sequences = []
    
    for i in range(0, len(encoded) - seq_len, stride):
        seq = encoded[i:i + seq_len + 1]  # +1 for target
        if len(seq) == seq_len + 1:
            sequences.append(seq)
    
    if not sequences:
        print("   ❌ No valid sequences could be created from the corpus")
        return
    
    print(f"   ✓ Created {len(sequences)} training sequences")
    
    # Convert to tensors
    input_data = torch.tensor([seq[:-1] for seq in sequences], dtype=torch.long)  # all but last
    target_data = torch.tensor([seq[1:] for seq in sequences], dtype=torch.long)  # all but first
    
    train_dataset = TensorDataset(input_data, target_data)
    train_loader = DataLoader(
        train_dataset, 
        batch_size=training_config.batch_size, 
        shuffle=True,
        pin_memory=True if torch.cuda.is_available() else False
    )
    
    print(f"   ✓ Data shape: {input_data.shape}, Targets shape: {target_data.shape}")
    
    # Step 8: Train the model with better optimizer settings
    print(f"\n🏋️  Step 8: Training model for {training_config.epochs} epochs with {training_config.max_steps} total steps...")
    
    # Use AdamW optimizer with weight decay
    optimizer = torch.optim.AdamW(
        model.parameters(), 
        lr=training_config.learning_rate,
        weight_decay=training_config.weight_decay,
        eps=training_config.eps,
        betas=(training_config.beta1, training_config.beta2)
    )
    
    # Learning rate scheduler (warmup + cosine decay)
    num_warmup_steps = max(10, training_config.max_steps // 10)  # 10% warmup
    num_training_steps = training_config.max_steps
    
    def lr_lambda(current_step: int):
        if current_step < num_warmup_steps:
            return float(current_step) / float(max(1, num_warmup_steps))
        progress = float(current_step - num_warmup_steps) / float(max(1, num_training_steps - num_warmup_steps))
        return max(0.1, 0.5 * (1.0 + math.cos(math.pi * progress)))
    
    scheduler = torch.optim.lr_scheduler.LambdaLR(optimizer, lr_lambda)
    
    # Move model to device
    device = torch.device(training_config.device)
    model.to(device)
    model.train()
    
    print(f"   Training on device: {device}")
    print(f"   Using learning rate scheduling (warmup: {num_warmup_steps} steps)")
    
    total_steps = 0
    best_loss = float('inf')
    
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
                ignore_index=-100,
                reduction='mean'
            )
            
            # Backward pass
            loss.backward()
            
            # Gradient clipping
            torch.nn.utils.clip_grad_norm_(model.parameters(), training_config.gradient_clip_val)
            
            # Update weights
            optimizer.step()
            
            # Update learning rate
            scheduler.step()
            
            epoch_loss += loss.item()
            steps_in_epoch += 1
            total_steps += 1
            
            if total_steps % training_config.log_every == 0:
                current_lr = scheduler.get_last_lr()[0]
                print(f"     Step {total_steps}, Loss: {loss.item():.4f}, LR: {current_lr:.2e}")
            
            if total_steps >= training_config.max_steps:
                break
        
        avg_loss = epoch_loss / max(steps_in_epoch, 1)
        print(f"   Epoch {epoch + 1} completed. Average Loss: {avg_loss:.4f}")
        
        # Save best model
        if avg_loss < best_loss:
            best_loss = avg_loss
            best_model_path = os.path.join(output_dir, "best_model.pth")
            torch.save(model.state_dict(), best_model_path)
            print(f"   ✓ Best model saved with loss: {best_loss:.4f}")
    
    # Save final trained model
    torch.save(model.state_dict(), model_path)
    print(f"   ✓ Final trained model saved: {model_path}")
    
    # Step 9: Generate text with trained model
    print("\n✨ Step 9: Generating text with TRAINED model...")
    
    model.eval()
    
    # Test generation with various prompts
    test_prompts = [
        "The",
        "Python",
        "AI",
        "Machine learning",
        "The quick",
        "Natural language",
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
        
        # Top-p sampling (with reasonable temperature)
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
    
    # Step 10: Results summary
    print(f"\n🎉 ADVANCED pipeline completed successfully with comprehensive training!")
    print(f"\n📍 All files saved in: {output_dir}/")
    print(f"   • Vocabulary: {vocab_path}")
    print(f"   • Trained Model: {model_path}")
    print(f"   • Best Model: {best_model_path}")
    print(f"   • Model config: {model_config_path}")
    print(f"   • Training config: {train_config_path}")
    print(f"   • Corpus: {corpus_path}")
    
    print(f"\n📋 To use your trained model, you can run:")
    print(f"   python main.py --generate 'Your prompt here' --model_path {model_path}")
    print(f"   python main.py --generate 'Your prompt here' --tokenizer_path {vocab_path}")
    
    print(f"\n💡 The model has now been extensively trained and should generate significantly more coherent text!")


if __name__ == "__main__":
    main()