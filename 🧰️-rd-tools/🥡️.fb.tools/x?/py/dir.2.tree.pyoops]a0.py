import os
import csv
from pathlib import Path
import argparse

def get_dir_tree(root_path):
    # List to store directory info: [name, level, index_in_level, parent_index]
    dir_matrix = []
    # Track indices for each level
    level_indices = {}
    
    def walk_dirs(current_path, level=0, parent_index=-1):
        try:
            # Get directory name
            dir_name = os.path.basename(current_path) or current_path
            # Initialize index counter for this level if not exists
            if level not in level_indices:
                level_indices[level] = 0
            # Current directory's index in its level
            current_index = len(dir_matrix)
            # Add to matrix
            dir_matrix.append([dir_name, level, level_indices[level], parent_index])
            level_indices[level] += 1
            
            # Get all subdirectories
            subdirs = sorted([d for d in os.listdir(current_path) 
                            if os.path.isdir(os.path.join(current_path, d))])
            
            # Recursively process subdirectories
            for subdir in subdirs:
                walk_dirs(os.path.join(current_path, subdir), 
                         level + 1, 
                         current_index)
                
        except (PermissionError, OSError) as e:
            # Handle errors (e.g., permission denied)
            print(f"Warning: Could not access {current_path}: {e}")
    
    # Start walking from root
    walk_dirs(root_path)
    return dir_matrix

def save_to_csv(dir_matrix, output_file):
    # Write matrix to CSV
    with open(output_file, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        # Write header
        writer.writerow(['Directory_Name', 'Level', 'Index_In_Level', 'Parent_Index'])
        # Write directory data
        for row in dir_matrix:
            writer.writerow(row)

def main():
    # Parse command-line arguments
    parser = argparse.ArgumentParser(description='Generate directory tree CSV')
    parser.add_argument('directory', help='Root directory to scan')
    parser.add_argument('--output', default='dir_tree.csv', 
                       help='Output CSV file (default: dir_tree.csv)')
    args = parser.parse_args()
    
    # Validate directory
    if not os.path.isdir(args.directory):
        print(f"Error: '{args.directory}' is not a valid directory")
        return
    
    # Get directory tree
    dir_matrix = get_dir_tree(args.directory)
    
    # Save to CSV
    save_to_csv(dir_matrix, args.output)
    print(f"Directory tree saved to {args.output}")

if __name__ == "__main__":
    main()
