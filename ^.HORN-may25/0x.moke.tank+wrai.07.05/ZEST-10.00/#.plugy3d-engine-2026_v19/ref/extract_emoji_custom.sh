#!/bin/bash

# extract_emoji_custom.sh
# Script to extract emoji from custom atlas with proper path handling

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

ATLAS_FILE="$1"
PARSED_EMOJIS_PATH="$2"
EMOJI_NAME="$3"
OUTPUT_DIR="$4"

# Validate inputs
if [ $# -ne 4 ]; then
    echo "Usage: $0 <atlas_file> <parsed_emojis_path> <emoji_name> <output_dir>"
    exit 1
fi

if [ ! -f "$ATLAS_FILE" ]; then
    echo "Error: Atlas file not found: $ATLAS_FILE"
    exit 1
fi

if [ ! -f "$PARSED_EMOJIS_PATH" ]; then
    echo "Error: Parsed emojis file not found: $PARSED_EMOJIS_PATH"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Run the Python extraction script
cd "$SCRIPT_DIR"
python3 extract_from_custom_atlas.py "$ATLAS_FILE" "$PARSED_EMOJIS_PATH" "$EMOJI_NAME"

# The actual output location from the Python script (using relative paths)
ACTUAL_OUTPUT_NORM="$SCRIPT_DIR/$EMOJI_NAME/$EMOJI_NAME.csv"
ACTUAL_OUTPUT_NESTED="$SCRIPT_DIR/data/entities/emoji/$EMOJI_NAME/$EMOJI_NAME.csv"

# Check both possible locations where the file could be
if [ -f "$ACTUAL_OUTPUT_NORM" ]; then
    OUTPUT_FILE="$ACTUAL_OUTPUT_NORM"
    echo "Found output at normal location: $ACTUAL_OUTPUT_NORM"
elif [ -f "$ACTUAL_OUTPUT_NESTED" ]; then
    OUTPUT_FILE="$ACTUAL_OUTPUT_NESTED"
    echo "Found output at nested location: $ACTUAL_OUTPUT_NESTED"
else
    # Also check the most deeply nested path based on your findings
    DEEP_NESTED_PATH="$SCRIPT_DIR/data/entities/emoji/$EMOJI_NAME/$EMOJI_NAME/$EMOJI_NAME.csv"
    if [ -f "$DEEP_NESTED_PATH" ]; then
        OUTPUT_FILE="$DEEP_NESTED_PATH"
        echo "Found output at deeply nested location: $DEEP_NESTED_PATH"
    else
        echo "Error: Expected output file not found at any location:"
        echo "  Normal path: $ACTUAL_OUTPUT_NORM"
        echo "  Nested path: $ACTUAL_OUTPUT_NESTED"
        echo "  Deep nested path: $DEEP_NESTED_PATH"
        exit 1
    fi
fi

# Move to the specified output directory
DEST_DIR="$OUTPUT_DIR/$EMOJI_NAME"
mkdir -p "$DEST_DIR"
cp "$OUTPUT_FILE" "$DEST_DIR/"
echo "Successfully extracted $EMOJI_NAME to $DEST_DIR/${EMOJI_NAME}.csv"