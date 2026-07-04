"""
Terrain Generator Module
Generates Minecraft-style terrain using CSV data from 3d_output directory.
Can be used by the Project Manager to load terrain defined in project files.
"""

import os
import csv
import re
from pathlib import Path

class TerrainBlock:
    """Represents a terrain block loaded from voxel data"""
    def __init__(self, block_type, x=0, y=0, z=0):
        self.block_type = block_type  # e.g., 'cobblestone', 'dirt', 'grass'
        self.x = x
        self.y = y
        self.z = z
        self.voxel_model = []
        self.loaded = False

    def load_voxel_model(self, csv_path):
        """Load the voxel model from 2D tile CSV file or 3D voxel CSV file"""
        try:
            # Check if the file exists first
            if not os.path.exists(str(csv_path)):
                print(f"CSV file not found at {csv_path}")
                return
                
            # Check if it's a 2D tile CSV (has multiple rows with comma-separated values) or 3D voxel CSV
            with open(str(csv_path), 'r') as f:
                first_line = f.readline().strip()
            
            # Check if first line contains comma-separated r,g,b values indicating 2D tile format
            if '"0,0,0"' in first_line or first_line.count('","') > 0:
                # This is a 2D tile, convert to 3D using 6-face duplication
                raw_voxel_data = self._load_2d_tile_to_3d_cube(csv_path)
            else:
                # This is a 3D voxel CSV, load normally
                raw_voxel_data = []
                with open(str(csv_path), 'r') as f:
                    reader = csv.DictReader(f)
                    for row in reader:
                        x = float(row['x'])
                        y = float(row['y'])
                        z = float(row['z'])
                        r = int(row['r'])
                        g = int(row['g'])
                        b = int(row['b'])
                        
                        # Only include non-black voxels (avoid empty spaces)
                        if not (r == 0 and g == 0 and b == 0):
                            raw_voxel_data.append((x, y, z, r, g, b))
            
            if not raw_voxel_data:
                print(f"No voxel data found for {self.block_type}")
                return

            # Normalize coordinates to fit within a unit cube for proper rendering
            min_x = min(v[0] for v in raw_voxel_data)
            max_x = max(v[0] for v in raw_voxel_data)
            min_y = min(v[1] for v in raw_voxel_data)
            max_y = max(v[1] for v in raw_voxel_data)
            min_z = min(v[2] for v in raw_voxel_data)
            max_z = max(v[2] for v in raw_voxel_data)

            size_x = max_x - min_x if max_x != min_x else 1
            size_y = max_y - min_y if max_y != min_y else 1
            size_z = max_z - min_z if max_z != min_z else 1

            normalized_voxel_data = []
            for x, y, z, r, g, b in raw_voxel_data:
                # Normalize to 0-7 range to match entity model normalization
                norm_x = 7.0 * (x - min_x) / size_x
                norm_y = 7.0 * (y - min_y) / size_y
                norm_z = 7.0 * (z - min_z) / size_z
                
                # Invert Y for correct display as done in entity rendering
                inverted_norm_y = 7.0 - norm_y

                normalized_voxel_data.append((norm_x, inverted_norm_y, norm_z, r, g, b))

            self.voxel_model = normalized_voxel_data
            self.loaded = True
            print(f"Successfully loaded and normalized voxel model for {self.block_type} with {len(self.voxel_model)} voxels")
        except FileNotFoundError:
            print(f"Voxel model CSV not found for {self.block_type} at {csv_path}")
        except Exception as e:
            print(f"Error loading voxel model for {self.block_type}: {e}")

    def _load_2d_tile_to_3d_cube(self, csv_path):
        """Load 2D 8x8 tile CSV and convert to 3D cube with 6-face duplication"""
        tile_data = []
        try:
            with open(csv_path, 'r', newline='', encoding='utf-8') as csvfile:
                for line in csvfile:
                    if line.strip():  # Skip empty lines
                        # Find all "r,g,b" patterns in quotes
                        matches = re.findall(r'"(\d+,\d+,\d+)"', line)
                        row_colors = []
                        for match in matches:
                            r, g, b = map(int, match.split(','))
                            row_colors.append((r, g, b))
                        
                        if row_colors:  # Only add non-empty rows
                            tile_data.append(row_colors)
            
            # Generate 3D points by mapping the 2D grid onto all 6 faces of a cube
            points_3d = []
            
            # Define the 6 faces of the cube with their fixed coordinate positions
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
                        # Check if we have data for this position
                        if grid_y < len(tile_data) and grid_x < len(tile_data[grid_y]):
                            r, g, b = tile_data[grid_y][grid_x]
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
        except Exception as e:
            print(f"Error loading 2D tile as 3D cube for {self.block_type}: {e}")
            return []


def generate_initial_terrain(base_path=None):
    """Generate a small, optimized terrain area to test VBO performance."""
    # Use default path if none provided
    if base_path is None:
        base_path = Path('^.fin_mc_8&16_c2--]FIXD') / 'mc_extracted_csvs_8x8'
    else:
        base_path = Path(base_path)
    
    terrain_blocks = {}
    
    # Create a 2x2 area with layered terrain: bedrock -> stone -> dirt -> grass surface
    world_size = 2  # Reduced from 5 to 2 to improve performance while testing
    height_layers = 4  # Reduced from 6 to 4 layers to improve performance
    
    for x in range(world_size):
        for z in range(world_size):
            # Stack of blocks from bottom to top: bedrock, stone, dirt, grass
            block_types = ['bedrock', 'stone', 'dirt', 'grass']  # Reduced to 4 layers
            
            for layer_idx, block_type in enumerate(block_types):
                cy = layer_idx  # y position based on layer
                cx, cz = x, z
                
                # Build correct path for each terrain type
                csv_path = base_path / block_type / f'{block_type}.csv'
                
                if not os.path.exists(str(csv_path)):
                    print(f"CSV file not found at {csv_path}")
                    # Try alternative: use the computed absolute path
                    csv_path = base_path / block_type / f'{block_type}.csv'
                    
                    if not os.path.exists(str(csv_path)):
                        print(f"Unable to find {block_type} CSV at any path, creating fallback...")
                        # Create minimal fallback model
                        pos_key = f"{cx},{cy},{cz}"
                        # Create a simple minimal voxel model (just a few voxels for visualization)
                        voxel_model = [
                            (2.0, 2.0, 2.0, 128, 128, 128),  # Simple voxel at corner
                            (5.0, 2.0, 2.0, 150, 150, 150),
                            (2.0, 5.0, 2.0, 140, 140, 140),
                            (2.0, 2.0, 5.0, 130, 130, 130),
                            (5.0, 5.0, 5.0, 160, 160, 160)  # Top opposite corner
                        ]
                        
                        terrain_blocks[pos_key] = {
                            'id': f"terrain_{block_type}_{cx}_{cy}_{cz}",
                            'x': cx,
                            'y': cy,
                            'z': cz,
                            'block_type': block_type,
                            'voxel_model': voxel_model,
                            'obj_type': 'terrain',
                            'scale_x': 1.0,
                            'scale_y': 1.0,
                            'scale_z': 1.0,
                            'render_style': 'six-faced',
                            'color': 'neutral',
                            'source_file': str(csv_path)
                        }
                        continue  # Continue to the next block
                
                # Load the voxel model for this terrain type
                terrain_block = TerrainBlock(block_type, x=cx, y=cy, z=cz)
                terrain_block.load_voxel_model(csv_path)
                
                if not terrain_block.loaded:
                    print(f"Failed to load voxel model for {block_type}, creating minimal fallback...")
                    # Create minimal fallback
                    pos_key = f"{cx},{cy},{cz}"
                    voxel_model = [
                        (2.0, 2.0, 2.0, 128, 128, 128),
                        (5.0, 2.0, 2.0, 150, 150, 150),
                        (2.0, 5.0, 2.0, 140, 140, 140),
                        (2.0, 2.0, 5.0, 130, 130, 130),
                        (5.0, 5.0, 5.0, 160, 160, 160)
                    ]
                    
                    terrain_blocks[pos_key] = {
                        'id': f"terrain_{block_type}_{cx}_{cy}_{cz}",
                        'x': cx,
                        'y': cy,
                        'z': cz,
                        'block_type': block_type,
                        'voxel_model': voxel_model,
                        'obj_type': 'terrain',
                        'scale_x': 1.0,
                        'scale_y': 1.0,
                        'scale_z': 1.0,
                        'render_style': 'six-faced',
                        'color': 'neutral',
                        'source_file': str(csv_path)
                    }
                    continue
                    
                # Store in terrain blocks dict with project-compatible format
                pos_key = f"{cx},{cy},{cz}"
                terrain_blocks[pos_key] = {
                    'id': f"terrain_{block_type}_{cx}_{cy}_{cz}",
                    'x': cx,
                    'y': cy,
                    'z': cz,
                    'block_type': block_type,
                    'voxel_model': terrain_block.voxel_model,
                    'obj_type': 'terrain',
                    'scale_x': 1.0,
                    'scale_y': 1.0,
                    'scale_z': 1.0,
                    'render_style': 'six-faced',
                    'color': 'neutral',
                    'source_file': str(csv_path)
                }
    
    total_blocks = world_size * world_size * height_layers
    print(f"Generated small-scale terrain: {world_size}x{world_size} area with {height_layers} layers = {total_blocks} total blocks")
    
    return terrain_blocks


def load_terrain_block(block_def):
    """Load a terrain block based on definition from project file"""
    # Extract properties from block definition
    block_type = block_def.get('block_type', block_def.get('id', 'unknown'))
    pos_x = block_def.get('x', block_def.get('pos_x', 0))
    pos_y = block_def.get('y', block_def.get('pos_y', 0))
    pos_z = block_def.get('z', block_def.get('pos_z', 0))
    color = block_def.get('color', 'neutral')
    source_file = block_def.get('source_file', '')
    
    # Create block
    block = TerrainBlock(block_type, x=pos_x, y=pos_y, z=pos_z)
    
    # Load the voxel model
    if source_file:
        block.load_voxel_model(source_file)
    
    return block