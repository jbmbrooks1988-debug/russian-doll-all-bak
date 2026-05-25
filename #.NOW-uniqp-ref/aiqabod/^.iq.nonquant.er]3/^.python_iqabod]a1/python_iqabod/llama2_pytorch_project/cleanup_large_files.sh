#!/bin/bash

# Clean up script to remove large model files while keeping the code
echo "🧹 Cleaning up large model files..."

# Remove pipeline output directories that contain large model files
rm -rf pipeline_output/
rm -rf trained_pipeline_output/
rm -rf advanced_trained_pipeline_output/

# Remove any large model files that might be in the root
find . -name "*.pth" -type f -delete
find . -name "*.pt" -type f -delete
find . -name "*.ckpt" -type f -delete
find . -name "*.bin" -type f -delete

# Remove any weight files
find . -name "*.weights" -type f -delete

# Remove any vocab/tokenizer models (except the code files)
find . -name "tokenizer.model*" -type f -delete

# Remove loss logs and other training artifacts
find . -name "loss.txt" -type f -delete
find . -name "train.log" -type f -delete
find . -name "*.log" -type f -delete

echo "✅ Cleanup complete!"
echo "📁 Code files preserved, large model files removed."
echo ""
echo "To verify what was kept, you can run:"
echo "ls -la *.py *.md *.txt *.c *.h *.json *.sh"