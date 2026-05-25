"""Training loop implementation for Llama2 model"""
import math
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import DataLoader
import time
from typing import Optional, Dict, Any
import os
import json
from model import Llama2ForCausalLM
from config import ModelConfig, TrainingConfig
from data import Dataset


class Trainer:
    """Training class for Llama2 model"""
    
    def __init__(
        self, 
        model: Llama2ForCausalLM, 
        train_loader: DataLoader, 
        val_loader: Optional[DataLoader] = None,
        training_config: TrainingConfig = None
    ):
        self.model = model
        self.train_loader = train_loader
        self.val_loader = val_loader
        self.config = training_config or TrainingConfig()
        
        # Setup device
        self.device = torch.device(self.config.device)
        self.model.to(self.device)
        
        # Setup optimizer
        self.optimizer = torch.optim.AdamW(
            self.model.parameters(),
            lr=self.config.learning_rate,
            weight_decay=self.config.weight_decay,
            betas=(self.config.beta1, self.config.beta2),
            eps=self.config.eps
        )
        
        # Learning rate scheduler
        self.scheduler = self._create_scheduler()
        
        # Mixed precision training scaler
        self.scaler = torch.cuda.amp.GradScaler(enabled=self.config.use_fp16)
        
        # Logging
        self.log_file = "loss_log.txt"
        self.step_count = 0
        self.start_time = time.time()
        
    def _create_scheduler(self):
        """Create learning rate scheduler"""
        def lr_lambda(current_step):
            if current_step < self.config.warmup_steps:
                # Linear warmup
                return float(current_step) / float(max(1, self.config.warmup_steps))
            else:
                # Cosine decay
                progress = float(current_step - self.config.warmup_steps) / \
                          float(max(1, self.config.lr_decay_steps - self.config.warmup_steps))
                return max(self.config.min_lr / self.config.learning_rate,
                          0.5 * (1.0 + math.cos(math.pi * progress)))
        
        return torch.optim.lr_scheduler.LambdaLR(self.optimizer, lr_lambda)
    
    def save_checkpoint(self, path: str, epoch: int, step: int):
        """Save model checkpoint"""
        checkpoint = {
            'epoch': epoch,
            'step': step,
            'model_state_dict': self.model.state_dict(),
            'optimizer_state_dict': self.optimizer.state_dict(),
            'scheduler_state_dict': self.scheduler.state_dict(),
            'scaler_state_dict': self.scaler.state_dict(),
            'config': self.config.to_dict()
        }
        torch.save(checkpoint, path)
    
    def load_checkpoint(self, path: str):
        """Load model checkpoint"""
        checkpoint = torch.load(path, map_location=self.device)
        self.model.load_state_dict(checkpoint['model_state_dict'])
        self.optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
        self.scheduler.load_state_dict(checkpoint['scheduler_state_dict'])
        self.scaler.load_state_dict(checkpoint['scaler_state_dict'])
        self.step_count = checkpoint['step']
        return checkpoint['epoch']
    
    def train_step(self, batch: torch.Tensor) -> torch.Tensor:
        """Perform a single training step"""
        self.model.train()
        
        # Move batch to device
        batch = batch.to(self.device)
        
        # Split into input and target
        # Input: all tokens except the last
        # Target: all tokens except the first (shifted by one)
        input_ids = batch[:, :-1]
        targets = batch[:, 1:]
        
        # Forward pass with autocast for mixed precision
        with torch.cuda.amp.autocast(enabled=self.config.use_fp16):
            outputs = self.model(input_ids)
            # Calculate loss
            loss = F.cross_entropy(
                outputs.view(-1, outputs.size(-1)),
                targets.reshape(-1),
                ignore_index=-1  # No specific padding index in our setup
            )
        
        # Backward pass
        self.optimizer.zero_grad()
        self.scaler.scale(loss).backward()
        
        # Gradient clipping
        self.scaler.unscale_(self.optimizer)
        torch.nn.utils.clip_grad_norm_(self.model.parameters(), self.config.gradient_clip_val)
        
        # Update parameters
        self.scaler.step(self.optimizer)
        self.scaler.update()
        
        # Update learning rate
        self.scheduler.step()
        
        return loss
    
    def eval_step(self) -> float:
        """Perform a single evaluation step"""
        if self.val_loader is None:
            return 0.0
            
        self.model.eval()
        total_loss = 0.0
        num_batches = 0
        
        with torch.no_grad():
            for batch in self.val_loader:
                batch = batch.to(self.device)
                
                # Split into input and target
                input_ids = batch[:, :-1]
                targets = batch[:, 1:]
                
                with torch.cuda.amp.autocast(enabled=self.config.use_fp16):
                    outputs = self.model(input_ids)
                    loss = F.cross_entropy(
                        outputs.view(-1, outputs.size(-1)),
                        targets.reshape(-1),
                        ignore_index=-1
                    )
                    
                total_loss += loss.item()
                num_batches += 1
                
                if num_batches >= self.config.eval_steps:
                    break  # Only evaluate for a limited number of steps
        
        return total_loss / num_batches if num_batches > 0 else 0.0
    
    def train_epoch(self, epoch: int) -> float:
        """Train for one epoch"""
        total_loss = 0.0
        num_batches = 0
        
        for batch_idx, batch in enumerate(self.train_loader):
            # Perform training step
            loss = self.train_step(batch)
            total_loss += loss.item()
            num_batches += 1
            self.step_count += 1
            
            # Logging
            if self.step_count % self.config.log_every == 0:
                current_lr = self.scheduler.get_last_lr()[0]
                elapsed_time = time.time() - self.start_time
                print(f"Step {self.step_count}: Loss = {loss.item():.4f}, "
                      f"LR = {current_lr:.6f}, Time = {elapsed_time:.2f}s")
                
                # Log to file
                with open(self.log_file, 'a') as f:
                    f.write(f"{self.step_count}\t{loss.item():.6f}\t{current_lr:.8f}\n")
            
            # Evaluation
            if self.step_count % self.config.eval_every == 0 and self.val_loader:
                val_loss = self.eval_step()
                print(f"Step {self.step_count}: Val Loss = {val_loss:.4f}")
                
                # Log evaluation to file
                with open(self.log_file, 'a') as f:
                    f.write(f"{self.step_count}\t{loss.item():.6f}\t{val_loss:.6f}\n")
            
            # Save checkpoint
            if self.step_count % self.config.save_every == 0:
                checkpoint_path = f"checkpoints/model_step_{self.step_count}.pt"
                os.makedirs("checkpoints", exist_ok=True)
                self.save_checkpoint(checkpoint_path, epoch, self.step_count)
                print(f"Checkpoint saved at step {self.step_count}")
        
        return total_loss / num_batches if num_batches > 0 else 0.0
    
    def train(self):
        """Main training loop"""
        print(f"Starting training on {self.device}")
        print(f"Training for {self.config.epochs} epochs")
        print(f"Total steps: {self.config.max_steps}")
        
        # Create checkpoints directory
        os.makedirs("checkpoints", exist_ok=True)
        
        for epoch in range(self.config.epochs):
            print(f"\nEpoch {epoch + 1}/{self.config.epochs}")
            
            # Train for one epoch
            epoch_loss = self.train_epoch(epoch)
            
            print(f"Epoch {epoch + 1} completed. Average loss: {epoch_loss:.4f}")
            
            # Save model after each epoch
            epoch_path = f"checkpoints/model_epoch_{epoch + 1}.pt"
            self.save_checkpoint(epoch_path, epoch, self.step_count)
            
            if self.step_count >= self.config.max_steps:
                print(f"Reached maximum steps ({self.config.max_steps}), stopping training.")
                break
    
        print("Training completed!")


def train_model(
    model_config: ModelConfig,
    training_config: TrainingConfig,
    train_dataset: Dataset,
    val_dataset: Optional[Dataset] = None
):
    """Convenience function to create and run training"""
    # Create model
    model = Llama2ForCausalLM(model_config)
    
    # Create data loaders
    train_loader = DataLoader(
        train_dataset,
        batch_size=training_config.batch_size,
        shuffle=True,
        num_workers=2
    )
    
    val_loader = None
    if val_dataset:
        val_loader = DataLoader(
            val_dataset,
            batch_size=training_config.batch_size,
            shuffle=False,
            num_workers=2
        )
    
    # Create trainer
    trainer = Trainer(
        model=model,
        train_loader=train_loader,
        val_loader=val_loader,
        training_config=training_config
    )
    
    # Start training
    trainer.train()
    
    return trainer