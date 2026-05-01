#!/usr/bin/env python3
"""
Convert 2D 8x8 emoji CSV files to 3D versions
"""

import os
import csv
import math
import numpy as np
from pathlib import Path


def load_2d_emoji(csv_file_path):
    """Load 2D 8x8 emoji data from CSV file."""
    grid = []
    try:
        with open(csv_file_path, 'r', newline='', encoding='utf-8') as csvfile:
            reader = csv.reader(csvfile)
            for row in reader:
                grid_row = []
                for cell in row:
                    # Parse "r,g,b" format
                    r, g, b = map(int, cell.strip('"').split(','))
                    grid_row.append((r, g, b))
                grid.append(grid_row)
    except Exception as e:
        print(f"Error loading {csv_file_path}: {e}")
        # Return a default grid if there's an error
        grid = [[(128, 128, 128) for _ in range(8)] for _ in range(8)]
    return grid


def generate_3d_points_from_2d(grid, depth=8):
    """
    Generate 3D points from 2D 8x8 grid to create a hollow cube where the 
    2D pattern appears on all 6 faces of the cube
    """
    # Create 3D points by mapping the 2D grid onto all 6 faces of a cube
    points_3d = []
    
    # Define the 6 faces of the cube with their fixed coordinate positions
    # Format: (axis1, axis2, fixed_axis, fixed_pos, scale_factor)
    # Using coordinates in the range [-1, 1] for consistent sizing
    faces = [
        # Front face (positive Z)
        {'axis1': 'x', 'axis2': 'y', 'fixed_axis': 'z', 'pos': 1},
        # Back face (negative Z)  
        {'axis1': 'x', 'axis2': 'y', 'fixed_axis': 'z', 'pos': -1},
        # Top face (positive Y)
        {'axis1': 'x', 'axis2': 'z', 'fixed_axis': 'y', 'pos': 1},
        # Bottom face (negative Y)
        {'axis1': 'x', 'axis2': 'z', 'fixed_axis': 'y', 'pos': -1},
        # Right face (positive X)
        {'axis1': 'y', 'axis2': 'z', 'fixed_axis': 'x', 'pos': 1},
        # Left face (negative X)
        {'axis1': 'y', 'axis2': 'z', 'fixed_axis': 'x', 'pos': -1}
    ]
    
    for face in faces:
        for grid_y in range(8):
            for grid_x in range(8):
                r, g, b = grid[grid_y][grid_x]
                # Only add points that have actual color (not pure black background)
                if r != 0 or g != 0 or b != 0:
                    # Map grid coordinates (0-7) to normalized coordinates (-1 to 1)
                    # Convert grid coordinates to normalized values in range [-1, 1]
                    coord1_norm = (grid_x - 3.5) / 3.5  # Normalize to [-1, 1]
                    coord2_norm = (grid_y - 3.5) / 3.5  # Normalize to [-1, 1]
                    
                    # Assign coordinates based on which axes this face uses
                    x = y = z = 0  # Initialize
                    if face['axis1'] == 'x' and face['axis2'] == 'y':
                        x, y, z = coord1_norm, coord2_norm, face['pos']
                    elif face['axis1'] == 'x' and face['axis2'] == 'z':
                        x, y, z = coord1_norm, face['pos'], coord2_norm
                    elif face['axis1'] == 'y' and face['axis2'] == 'z':
                        x, y, z = face['pos'], coord1_norm, coord2_norm
                    
                    points_3d.append((x, y, z, r, g, b))
    
    return points_3d


def save_3d_emoji(points_3d, output_file_path):
    """Save 3D points to a CSV file."""
    try:
        with open(output_file_path, 'w', newline='') as csvfile:
            writer = csv.writer(csvfile)
            # Write header
            writer.writerow(['x', 'y', 'z', 'r', 'g', 'b'])
            # Write data rows
            for x, y, z, r, g, b in points_3d:
                writer.writerow([f"{x:.3f}", f"{y:.3f}", f"{z:.3f}", r, g, b])
    except Exception as e:
        print(f"Error saving {output_file_path}: {e}")


def convert_2d_to_3d(input_dir, output_dir):
    """Convert all 2D emoji CSVs in input_dir to 3D versions in output_dir."""
    print(f"Converting 2D emojis in {input_dir} to 3D in {output_dir}")
    
    # Create output directory if it doesn't exist
    Path(output_dir).mkdir(parents=True, exist_ok=True)
    
    # Find all 2D emoji CSV files
    for root, dirs, files in os.walk(input_dir):
        for file in files:
            if file.endswith('.csv') and not file.startswith('.'):
                input_path = os.path.join(root, file)
                
                # Create corresponding output path
                rel_path = os.path.relpath(input_path, input_dir)
                output_path = os.path.join(output_dir, rel_path)
                
                # Create subdirectory if needed
                output_subdir = os.path.dirname(output_path)
                if output_subdir and output_subdir != output_path:  # Not a file in root
                    Path(output_subdir).mkdir(parents=True, exist_ok=True)
                
                print(f"Converting: {input_path} -> {output_path}")
                
                # Load 2D emoji
                grid_2d = load_2d_emoji(input_path)
                
                # Generate 3D points
                points_3d = generate_3d_points_from_2d(grid_2d)
                
                # Save 3D emoji
                save_3d_emoji(points_3d, output_path)
    
    print("Conversion completed!")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="Convert 2D 8x8 emoji CSVs to 3D versions")
    parser.add_argument("--input", "-i", default="mc_extracted_csvs_8x8", 
                       help="Input directory containing 2D emoji folders (default: mc_extracted_csvs_8x8)")
    parser.add_argument("--output", "-o", default="3d_output",
                       help="Output directory for 3D emoji folders (default: 3d_output)")
    
    args = parser.parse_args()
    
    convert_2d_to_3d(args.input, args.output)


if __name__ == "__main__":
    main()