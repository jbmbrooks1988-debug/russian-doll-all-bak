#!/usr/bin/env python3
"""
Minecraft Texture to CSV Converter
Converts individual texture files to CSV format, supporting both 8x8 and 16x16 output
"""

import os
import csv
from PIL import Image
import argparse


def convert_to_8x8(tile_img):
    """
    Convert a tile image to 8x8 pixels with nearest neighbor resampling to preserve color variety.
    
    Args:
        tile_img: Input tile image
    
    Returns:
        8x8 grid with RGB values as strings "r,g,b"
    """
    # Use NEAREST resampling to preserve more color variety instead of averaging
    try:
        # For newer versions of PIL
        downscaled = tile_img.resize((8, 8), Image.Resampling.NEAREST)
    except AttributeError:
        # For older versions of PIL
        downscaled = tile_img.resize((8, 8), Image.NEAREST)
    
    # Convert to RGB if needed
    downscaled = downscaled.convert('RGB')
    
    # Get the pixels as a list of RGB tuples
    pixels = list(downscaled.getdata())
    
    # Reshape into 8x8 grid
    grid = []
    for row in range(8):
        grid_row = []
        for col in range(8):
            idx = row * 8 + col
            r, g, b = pixels[idx]
            grid_row.append(f"{r},{g},{b}")
        grid.append(grid_row)
    
    return grid


def convert_to_16x16(tile_img):
    """
    Convert a tile image to 16x16 pixels (maintains size if already 16x16).
    
    Args:
        tile_img: Input tile image
    
    Returns:
        16x16 grid with RGB values as strings "r,g,b"
    """
    # Resize to 16x16 if needed
    resized = tile_img.resize((16, 16), Image.Resampling.NEAREST)
    resized = resized.convert('RGB')
    
    # Get the pixels as a list of RGB tuples
    pixels = list(resized.getdata())
    
    # Reshape into 16x16 grid
    grid = []
    for row in range(16):
        grid_row = []
        for col in range(16):
            idx = row * 16 + col
            r, g, b = pixels[idx]
            grid_row.append(f"{r},{g},{b}")
        grid.append(grid_row)
    
    return grid


def save_grid_as_csv(grid, output_path):
    """
    Save the grid as a CSV file.
    
    Args:
        grid: Grid with RGB values as strings
        output_path: Path to save the CSV file
    """
    with open(output_path, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerows(grid)


def process_individual_textures(texture_dir, output_base_dir, output_size=8):
    """
    Process individual texture files from a directory and convert them to CSV format.
    
    Args:
        texture_dir: Directory containing individual texture files
        output_base_dir: Base directory to save the CSV files
        output_size: Size of output grid (8 or 16)
    """
    if not os.path.exists(texture_dir):
        print(f"Texture directory {texture_dir} does not exist!")
        return
    
    # Create output directories
    output_dir = os.path.join(output_base_dir, f"mc_extracted_csvs_{output_size}x{output_size}")
    os.makedirs(output_dir, exist_ok=True)
    
    # Get all PNG files in the texture directory
    texture_files = [f for f in os.listdir(texture_dir) if f.lower().endswith('.png')]
    texture_files.sort()  # Sort alphabetically
    
    print(f"Found {len(texture_files)} texture files in {texture_dir}")
    
    for texture_file in texture_files:
        # Get the block name from the filename (without extension)
        block_name = os.path.splitext(texture_file)[0]
        
        # Full path to the texture file
        texture_path = os.path.join(texture_dir, texture_file)
        
        try:
            # Open the texture image
            tile_img = Image.open(texture_path).convert('RGB')
            
            # Convert to the appropriate size
            if output_size == 8:
                grid = convert_to_8x8(tile_img)
            elif output_size == 16:
                grid = convert_to_16x16(tile_img)
            else:
                raise ValueError("Output size must be 8 or 16")
            
            # Create output directory for this block
            block_dir = os.path.join(output_dir, block_name)
            os.makedirs(block_dir, exist_ok=True)
            
            # Save as CSV
            csv_path = os.path.join(block_dir, f"{block_name}.csv")
            save_grid_as_csv(grid, csv_path)
            
            print(f"Processed {block_name} -> {csv_path} ({output_size}x{output_size})")
        except Exception as e:
            print(f"Error processing {texture_file}: {e}")


def main():
    """Main function with command-line interface."""
    parser = argparse.ArgumentParser(description="Minecraft Texture to CSV Converter")
    parser.add_argument("--input-dir", "-i", default="textures+labled=indi=16x16=93",
                        help="Input directory containing individual texture files (default: textures+labled=indi=16x16=93)")
    parser.add_argument("--output-dir", "-o", default=".",
                        help="Output directory base for CSV files (default: current directory)")
    parser.add_argument("--size", "-s", type=int, choices=[8, 16], default=8,
                        help="Output grid size: 8 for 8x8, 16 for 16x16 (default: 8)")
    
    args = parser.parse_args()
    
    print(f"Processing textures from: {args.input_dir}")
    print(f"Output size: {args.size}x{args.size}")
    print(f"Output directory: {args.output_dir}")
    
    process_individual_textures(args.input_dir, args.output_dir, args.size)


if __name__ == "__main__":
    main()
    print("\nMinecraft texture processing complete!")