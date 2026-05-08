#!/bin/bash

# Minecraft Texture to CSV Converter Script
# Converts individual texture files to CSV format with support for 8x8 and 16x16 output

echo "Minecraft Texture to CSV Converter"
echo "=================================="

# Check if required python packages are available
if ! python3 -c "import pygame, OpenGL, PIL" &> /dev/null; then
    echo "Error: Required Python packages not found!"
    echo "Please install them with: pip install pygame PyOpenGL PyOpenGL_accelerate Pillow"
    exit 1
fi

# Set default values
INPUT_DIR="textures+labled=indi=16x16=93"
OUTPUT_SIZE=8

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -i|--input-dir)
            INPUT_DIR="$2"
            shift 2
            ;;
        -s|--size)
            OUTPUT_SIZE="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -i, --input-dir DIR    Input directory with PNG textures (default: textures+labled=indi=16x16=93)"
            echo "  -s, --size SIZE        Output size (8 or 16) (default: 8)"
            echo "  -h, --help             Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                                    # Convert to 8x8"
            echo "  $0 -s 16                             # Convert to 16x16"
            echo "  $0 -i ./custom_textures -s 16        # Custom input and size"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information."
            exit 1
            ;;
    esac
done

# Validate output size
if [[ "$OUTPUT_SIZE" != "8" && "$OUTPUT_SIZE" != "16" ]]; then
    echo "Error: Output size must be 8 or 16"
    exit 1
fi

# Check if input directory exists
if [[ ! -d "$INPUT_DIR" ]]; then
    echo "Error: Input directory '$INPUT_DIR' does not exist!"
    exit 1
fi

echo "Input directory: $INPUT_DIR"
echo "Output size: ${OUTPUT_SIZE}x${OUTPUT_SIZE}"
echo ""

# Run the converter
python3 mc_texture_to_csv_converter.py --input-dir "$INPUT_DIR" --size "$OUTPUT_SIZE"

echo ""
echo "Conversion complete!"