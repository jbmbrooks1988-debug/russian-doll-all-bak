"""
Emoji Entity Loader Module
Provides functions for loading and managing 3D emoji-based entities from 2D tile CSV files with depth extrusion.
Can be used by the Project Manager to load entities defined in project files.
"""

import os
import csv
import math
import subprocess
from pathlib import Path


class EmojiEntity:
    """Represents an emoji-based 3D entity loaded from 2D tile data"""
    def __init__(self, entity_id, x=0, y=0, z=0, color='white'):
        self.id = entity_id
        self.x = x
        self.y = y
        self.z = z
        self.color = color
        self.voxel_model = []
        self.loaded = False

    def load_voxel_model(self, directory_path):
        """Load 2D 8x8 tile CSV and convert to 3D voxel representation by extruding back in Z direction"""
        try:
            raw_voxel_data = []
            
            # Look for CSV files in the directory
            for file_name in os.listdir(directory_path):
                if file_name.endswith('.csv'):
                    csv_path = os.path.join(directory_path, file_name)
                    
                    with open(csv_path, 'r', newline='', encoding='utf-8') as csvfile:
                        reader = csv.reader(csvfile)
                        for y, row in enumerate(reader):
                            for x, cell in enumerate(row):
                                if cell.strip():  # Skip empty cells
                                    # Parse the cell as "r,g,b" format
                                    rgb_values = cell.strip('"').split(',')
                                    if len(rgb_values) == 3:
                                        r, g, b = map(int, rgb_values)
                                        # Only add non-black voxels (non-transparent pixels)
                                        if r != 0 or g != 0 or b != 0:
                                            # Add 8 voxels extending in the Z direction (extrusion)
                                            for z in range(8):  # Extrude back by 8 voxels
                                                # Normalize coordinates to match entity model format (0-7 range)
                                                # Invert Y for correct display as done in entity rendering
                                                norm_x = float(x)
                                                norm_y = 7.0 - float(y)  # Invert Y
                                                norm_z = float(z)
                                                
                                                raw_voxel_data.append((norm_x, norm_y, norm_z, r, g, b))
                    
                    break  # Only process the first CSV file found
            
            if not raw_voxel_data:
                print(f"No voxel data found for {self.id}")
                return

            # Center the model at origin for proper positioning
            min_x = min(v[0] for v in raw_voxel_data)
            max_x = max(v[0] for v in raw_voxel_data)
            min_y = min(v[1] for v in raw_voxel_data)
            max_y = max(v[1] for v in raw_voxel_data)
            min_z = min(v[2] for v in raw_voxel_data)
            max_z = max(v[2] for v in raw_voxel_data)

            # Calculate offsets to center the model
            offset_x = (max_x + min_x) / 2.0 - 3.5
            offset_y = (max_y + min_y) / 2.0 - 3.5  
            offset_z = (max_z + min_z) / 2.0 - 3.5

            # Normalize coordinates to be centered and in proper scale
            normalized_voxel_data = []
            for x, y, z, r, g, b in raw_voxel_data:
                norm_x = x - offset_x
                norm_y = y - offset_y
                norm_z = z - offset_z
                
                # Ensure coordinates are within bounds
                norm_x = max(0, min(7, norm_x))
                norm_y = max(0, min(7, norm_y))
                norm_z = max(0, min(7, norm_z))
                
                normalized_voxel_data.append((norm_x, norm_y, norm_z, r, g, b))

            self.voxel_model = normalized_voxel_data
            self.loaded = True
            print(f"Successfully loaded and normalized emoji voxel model for {self.id} with {len(self.voxel_model)} voxels")
        except Exception as e:
            print(f"Error loading emoji voxel model for {self.id}: {e}")

            self.voxel_model = normalized_voxel_data
            self.loaded = True
            print(f"Successfully loaded and normalized emoji voxel model for {self.id} with {len(self.voxel_model)} voxels")
        except Exception as e:
            print(f"Error loading emoji voxel model for {self.id}: {e}")


def load_emoji_entity(entity_def):
    """Load an emoji entity based on definition from project file"""
    # Extract properties from entity definition
    entity_id = entity_def.get('id', 'unknown')
    pos_x = entity_def.get('pos_x', 0)
    pos_y = entity_def.get('pos_y', 0)
    pos_z = entity_def.get('pos_z', 0)
    color = entity_def.get('color', 'white')
    source_file = entity_def.get('source_file', '')
    
    # Create entity
    entity = EmojiEntity(entity_id, x=pos_x, y=pos_y, z=pos_z, color=color)
    
    # Load the voxel model from the specific file
    if source_file and Path(source_file).exists():
        # Determine directory path from source file
        dir_path = Path(source_file).parent
        entity.load_voxel_model(dir_path)
    elif source_file:  # file doesn't exist in expected location, try to find it
        # For some entities like avocado and convenience_store, try to load directly from file path
        try:
            entity.load_specific_voxel_file(source_file)
        except:
            # If all else fails, create minimal fallback
            entity.voxel_model = [(3.0, 3.0, 3.0, 128, 128, 128), (4.0, 4.0, 4.0, 150, 150, 150)]
            entity.loaded = True
    
    return entity


def generate_emoji_csvs_if_needed(emoji_names, emoji_base_path='./data/entities/emoji'):
    """Generate necessary emoji CSV files by calling the C executable."""
    print("Checking for emoji CSVs...")
    
    import os
    original_dir = os.getcwd()
    
    try:
        # Use the original working approach that extracts from the main atlas
        # Use relative paths to make it portable
        ref_dir = Path(original_dir) / 'ref'
        executable_path = ref_dir / 'extract_emoji'
        atlas_path = ref_dir / 'emoji_atlas.png'  # Use the large atlas with real colors
        list_path = ref_dir / 'parsed_emojis.txt'

        emojis_to_generate = []
        emoji_base_path = Path(emoji_base_path)
        
        # Check which emoji entities are needed and only generate those that don't exist
        for emoji_name in emoji_names:
            emoji_path = emoji_base_path / emoji_name
            emoji_csv_path = emoji_path / f"{emoji_name}.csv"
            if not emoji_csv_path.exists():
                emojis_to_generate.append(emoji_name)
        
        if not emojis_to_generate:
            print("All required emoji CSVs already exist. Skipping generation.")
            return
        
        # Ensure the base directory for emojis exists
        emoji_base_path.mkdir(parents=True, exist_ok=True)
        
        for emoji_name in emojis_to_generate:
            emoji_csv_path = emoji_base_path / emoji_name / f"{emoji_name}.csv"
            if emoji_csv_path.exists():
                print(f"CSV for '{emoji_name}' already exists. Skipping generation.")
                continue

            print(f"Generating CSV for '{emoji_name}' from main atlas...")
            
            # Get the emoji directory to store the final location
            emoji_dir = emoji_base_path / emoji_name
            emoji_dir.mkdir(parents=True, exist_ok=True)
            
            try:
                # Execute from the original directory to ensure paths resolve correctly
                result = subprocess.run(
                    [str(executable_path), str(atlas_path), str(list_path), emoji_name],
                    cwd=original_dir,  # Execute from project root directory
                    check=True,
                    capture_output=True,
                    text=True
                )
                
                print(f"Successfully generated CSV for '{emoji_name}'.")
                
                # Move the generated files from the root directory to the proper location
                generated_dir = Path(original_dir) / emoji_name
                target_dir = emoji_base_path / emoji_name
                
                if generated_dir.exists() and generated_dir.is_dir():
                    # Move the CSV file to the target location
                    generated_csv = generated_dir / f"{emoji_name}.csv"
                    target_csv = target_dir / f"{emoji_name}.csv"
                    
                    if generated_csv.exists():
                        # Move the file
                        import shutil
                        shutil.move(str(generated_csv), str(target_csv))
                        print(f"Moved {emoji_name}.csv to correct location: {target_csv}")
                    
                    # Remove the temporary directory from root
                    import shutil
                    shutil.rmtree(generated_dir)
                    print(f"Removed temporary directory: {generated_dir}")
                else:
                    print(f"Warning: Generated directory {generated_dir} not found after extraction")
                
            except FileNotFoundError:
                print(f"Error: The executable '{executable_path}' was not found.")
            except subprocess.CalledProcessError as e:
                print(f"Error generating CSV for '{emoji_name}':")
                print(f"Return code: {e.returncode}")
                print(f"Output: {e.stdout}")
                print(f"Error output: {e.stderr}")
                
    finally:
        # Change back to original directory if needed (though subprocess call should preserve it)
        os.chdir(original_dir)