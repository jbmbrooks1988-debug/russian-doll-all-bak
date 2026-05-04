#!/usr/bin/env python3
"""
Create a CSV file documenting the arrangement of blocks in the Minecraft texture atlas.
This maps block names to their positions (row, col) in the 16x16 grid of the texture atlas.
"""

import json
import csv

def create_atlas_mapping_csv():
    # Load the block info JSON
    with open('../block_info.json', 'r') as f:
        block_info = json.load(f)
    
    # Get the atlas dimensions from the JSON
    atlas_rows = block_info["texture_atlas"]["rows"]
    atlas_cols = block_info["texture_atlas"]["columns"]
    
    # Create a 2D grid to hold block names at their (row, col) positions
    atlas_grid = [['' for _ in range(atlas_cols)] for _ in range(atlas_rows)]
    
    # Map each block to its position in the grid
    for block_name, coords_list in block_info["blocks"].items():
        # Use the first face coordinates to determine atlas position
        row, col = coords_list[0]  # [row, col] - using first face as representative
        if 0 <= row < atlas_rows and 0 <= col < atlas_cols:
            atlas_grid[row][col] = block_name
        else:
            print(f"Warning: Block {block_name} has invalid coordinates [{row}, {col}] for a {atlas_rows}x{atlas_cols} atlas")
    
    # Write the grid to a CSV file
    with open('atlas_block_mapping.csv', 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        
        # Write header row with column numbers
        header_row = ['Row\\Col'] + [f'C{i}' for i in range(atlas_cols)]
        writer.writerow(header_row)
        
        # Write each row of the grid
        for row_idx, row_data in enumerate(atlas_grid):
            row_with_label = [f'R{row_idx}'] + row_data
            writer.writerow(row_with_label)
    
    print(f"Atlas mapping CSV created: atlas_block_mapping.csv")
    print(f"Shows {atlas_rows} rows × {atlas_cols} columns texture atlas layout")


if __name__ == "__main__":
    create_atlas_mapping_csv()