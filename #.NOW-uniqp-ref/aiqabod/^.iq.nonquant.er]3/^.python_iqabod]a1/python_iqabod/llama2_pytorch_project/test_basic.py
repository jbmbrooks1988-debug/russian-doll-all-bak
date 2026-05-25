"""Basic test to verify the Llama2 implementation"""
import torch
from model import Llama2ForCausalLM
from config import ModelConfig
from tokenizer import Tokenizer
from generation import generate_text


def test_model_creation():
    """Test that we can create the model successfully"""
    print("Testing model creation...")
    
    # Create a small model config for testing
    config = ModelConfig(
        dim=256,           # Smaller dimension for testing
        hidden_dim=512,    # Smaller hidden dimension
        n_layers=4,        # Fewer layers for testing
        n_heads=8,         # Fewer heads
        n_kv_heads=4,      # Fewer KV heads
        vocab_size=1000,   # Smaller vocab for testing
        max_seq_len=512    # Smaller sequence length
    )
    
    # Create the model
    model = Llama2ForCausalLM(config)
    
    # Verify the model was created with correct dimensions
    assert model.model.tok_embeddings.num_embeddings == config.vocab_size
    assert model.model.norm.weight.shape[0] == config.dim
    print(f"✓ Model created successfully with {sum(p.numel() for p in model.parameters()):,} parameters")
    
    return model, config


def test_tokenizer():
    """Test the tokenizer"""
    print("Testing tokenizer...")
    
    # Create a simple tokenizer
    tokenizer = Tokenizer(vocab_size=1000)
    
    # Test encoding/decoding
    test_text = "Hello world! This is a test."
    encoded = tokenizer.encode(test_text)
    decoded = tokenizer.decode(encoded)
    
    print(f"✓ Original: {test_text}")
    print(f"✓ Encoded: {encoded[:10]}...")  # Show first 10 tokens
    print(f"✓ Decoded: {decoded}")
    
    return tokenizer


def test_generation():
    """Test text generation"""
    print("Testing generation...")
    
    # Create a small model for testing
    config = ModelConfig(
        dim=128,
        hidden_dim=256,
        n_layers=2,
        n_heads=4,
        n_kv_heads=2,
        vocab_size=500,
        max_seq_len=128
    )
    
    model = Llama2ForCausalLM(config)
    tokenizer = Tokenizer(vocab_size=config.vocab_size)
    
    # Simple test - just make sure we can run a forward pass
    dummy_input = torch.randint(0, config.vocab_size, (1, 10))  # Batch size 1, sequence length 10
    with torch.no_grad():
        output = model(dummy_input)
        
    print(f"✓ Model forward pass successful. Output shape: {output.shape}")
    print(f"✓ Vocabulary size: {output.shape[-1]}")
    
    return model, tokenizer, config


def run_all_tests():
    """Run all tests"""
    print("Running basic tests for Llama2 implementation...\n")
    
    try:
        model, config = test_model_creation()
        tokenizer = test_tokenizer()
        model2, tokenizer2, config2 = test_generation()
        
        print("\n✓ All basic tests passed! Implementation appears to be working correctly.")
        
        # Try a small generation example
        print("\nTrying a small generation example...")
        dummy_prompt = "The weather today is"
        dummy_tokens = tokenizer2.encode(dummy_prompt)
        dummy_input = torch.tensor([dummy_tokens])
        
        with torch.no_grad():
            logits = model2(dummy_input)
            # Get logits for the last token
            last_token_logits = logits[0, -1, :]
            # Sample next token
            next_token = torch.multinomial(torch.softmax(last_token_logits, dim=-1), num_samples=1)
            generated_text = tokenizer2.decode([next_token.item()])
        
        print(f"✓ Generated token: '{generated_text}' from prompt '{dummy_prompt}'")
        
    except Exception as e:
        print(f"✗ Tests failed with error: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    return True


if __name__ == "__main__":
    success = run_all_tests()
    if success:
        print("\n🎉 All tests completed successfully!")
    else:
        print("\n❌ Some tests failed.")