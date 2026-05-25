#!/bin/bash

# Clean up script to remove large model files while keeping the code
echo "🧹 Cleaning up large model files..."

# Remove any large model files that might exist
find . -name "*.model" -type f -size +1M -delete
find . -name "*.json" -path "./models/*" -type f -delete
find . -name "*.json" -path "./data/vocab/*" -type f -size +1M -delete
find . -name "*.bin" -type f -size +1M -delete
find . -name "*.safetensors" -type f -delete
find . -name "*.h5" -type f -delete
find . -name "model-*" -type f -delete
find . -name "*.weights" -type f -delete
find . -name "*.pth" -type f -delete
find . -name "*.pt" -type f -delete

# Remove model and vocabulary directories if they exist (but keep the directory structure)
if [ -d "models/" ]; then
    find models/ -type f -size +1M -delete
fi

if [ -d "data/vocab/" ]; then
    find data/vocab/ -type f -size +1M -delete
fi

echo "✅ Cleanup complete!"
echo "📁 Code files preserved, large model files removed."