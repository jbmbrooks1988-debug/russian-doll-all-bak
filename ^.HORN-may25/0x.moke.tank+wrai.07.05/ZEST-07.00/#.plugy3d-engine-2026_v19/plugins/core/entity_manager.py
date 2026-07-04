"""
Entity Manager Module
Provides common entity loading functionality that can be shared across plugins.
Designed to work with the Project Manager for unified entity handling.
"""

import os
import csv
import re

class Entity3D:
    """Represents a 3D entity loaded from voxel data"""
    def __init__(self, entity_id, x=0, y=0, z=0, color='red'):
        self.id = entity_id
        self.x = x
        self.y = y
        self.z = z
        self.color = color
        self.voxel_model = []
        self.loaded = False

    def load_voxel_model(self, directory_path):
        """Load the 8x8x8 voxel CSV file or 8x8 2D tile and convert to 3D, then normalize coordinates"""
        try:
            raw_voxel_data = []
            for file_name in os.listdir(directory_path):
                if file_name.endswith('.csv'):
                    csv_path = os.path.join(directory_path, file_name)
                    # Check if it's a 2D tile CSV (has multiple rows with comma-separated values) or 3D voxel CSV
                    with open(csv_path, 'r') as f:
                        first_line = f.readline().strip()
                        
                    # Check if first line contains comma-separated r,g,b values indicating 2D tile format
                    if '"0,0,0"' in first_line or first_line.count('","') > 0:
                        # This is a 2D tile, convert to 3D using deep extrusion for Adam/Eve entities, face-copying for others
                        if self.id in ['adam', 'eve']:
                            # Use deep extrusion for Adam/Eve entities (like emoji entities)
                            raw_voxel_data = self._load_2d_tile_deep_extrude(csv_path)
                        else:
                            # Use face-copying algorithm for other entities
                            raw_voxel_data = self._load_2d_faces_to_3d(csv_path)
                    else:
                        # This is a 3D voxel CSV, load normally
                        with open(csv_path, 'r') as f:
                            reader = csv.DictReader(f)
                            for row in reader:
                                x = float(row['x'])
                                y = float(row['y'])
                                z = float(row['z'])
                                r = int(row['r'])
                                g = int(row['g'])
                                b = int(row['b'])
                                if not (r == 0 and g == 0 and b == 0):
                                    raw_voxel_data.append((x, y, z, r, g, b))
                    break
            
            if not raw_voxel_data:
                print(f"No voxel data found for {self.id}")
                return

            # Normalize coordinates
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
                norm_x = 7.0 * (x - min_x) / size_x
                norm_y = 7.0 * (y - min_y) / size_y
                norm_z = 7.0 * (z - min_z) / size_z
                
                # Invert Y for correct display
                inverted_norm_y = 7.0 - norm_y

                normalized_voxel_data.append((norm_x, inverted_norm_y, norm_z, r, g, b))

            self.voxel_model = normalized_voxel_data
            self.loaded = True
            print(f"Successfully loaded and normalized voxel model for {self.id} with {len(self.voxel_model)} voxels")
        except FileNotFoundError:
            print(f"Voxel model CSV not found for {self.id} at {directory_path}")
        except Exception as e:
            print(f"Error loading voxel model for {self.id}: {e}")

    def load_specific_voxel_file(self, file_path):
        """Load a specific voxel file by file path"""
        try:
            # Check if file is a 2D tile format or 3D voxel format
            with open(file_path, 'r') as f:
                first_line = f.readline().strip()
                
            # Check if first line contains comma-separated r,g,b values indicating 2D tile format
            if '"0,0,0"' in first_line or first_line.count('","') > 0:
                # This is a 2D tile, convert to 3D using deep extrusion for Adam/Eve entities, face-copying for others
                if self.id in ['adam', 'eve']:
                    # Use deep extrusion for Adam/Eve entities (like emoji entities)
                    raw_voxel_data = self._load_2d_tile_deep_extrude(file_path)
                else:
                    # Use face-copying algorithm for other entities
                    raw_voxel_data = self._load_2d_faces_to_3d(file_path)
            else:
                # This is a 3D voxel CSV, load normally
                raw_voxel_data = []
                with open(file_path, 'r') as f:
                    reader = csv.DictReader(f)
                    for row in reader:
                        x = float(row['x'])
                        y = float(row['y'])
                        z = float(row['z'])
                        r = int(row['r'])
                        g = int(row['g'])
                        b = int(row['b'])
                        if not (r == 0 and g == 0 and b == 0):
                            raw_voxel_data.append((x, y, z, r, g, b))
            
            if not raw_voxel_data:
                print(f"No voxel data found for {self.id}")
                return

            # Normalize coordinates
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
                norm_x = 7.0 * (x - min_x) / size_x
                norm_y = 7.0 * (y - min_y) / size_y
                norm_z = 7.0 * (z - min_z) / size_z
                
                # Invert Y for correct display
                inverted_norm_y = 7.0 - norm_y

                normalized_voxel_data.append((norm_x, inverted_norm_y, norm_z, r, g, b))

            self.voxel_model = normalized_voxel_data
            self.loaded = True
            print(f"Successfully loaded and normalized voxel model for {self.id} with {len(self.voxel_model)} voxels")
        except FileNotFoundError:
            print(f"Voxel model CSV not found at {file_path}")
        except Exception as e:
            print(f"Error loading voxel model for {self.id}: {e}")

    def _load_2d_tile_to_3d(self, csv_path):
        """Load 2D 8x8 tile CSV and convert to 3D voxel representation with deep extrusion"""
        tile_data = []
        try:
            with open(csv_path, 'r', newline='', encoding='utf-8') as csvfile:
                for line in csvfile:
                    if line.strip():  # Skip empty lines
                        # The format is like: "0,0,0","1,5,9","11,39,61",...
                        # So we find all "r,g,b" patterns in quotes
                        matches = re.findall(r'"(\d+,\d+,\d+)"', line)
                        row_colors = []
                        for match in matches:
                            r, g, b = map(int, match.split(','))
                            row_colors.append((r, g, b))
                        
                        if row_colors:  # Only add non-empty rows
                            tile_data.append(row_colors)
            
            # Generate 3D points from 2D tile using deep extrusion (like emoji entities)
            points_3d = []
            depth = 8  # Create 8 layers deep for 3D effect
            
            for y in range(len(tile_data)):
                for x in range(len(tile_data[y])):
                    r, g, b = tile_data[y][x]
                    # Only add points that have actual color (not pure black background)
                    if r != 0 or g != 0 or b != 0:
                        # Add points at different depths to create a 3D voxel representation
                        for z in range(depth):
                            # Normalize coordinates to 0-7 range to match entity model format
                            norm_x = float(x)
                            
                            # For Adam/Eve entities, don't invert Y to fix upside-down issue
                            if self.id in ['adam', 'eve']:
                                norm_y = float(y)  # Don't invert Y for Adam/Eve
                            else:
                                norm_y = 7.0 - float(y)  # Invert Y for other entities (like emoji)
                                
                            norm_z = float(z)
                            
                            points_3d.append((norm_x, norm_y, norm_z, r, g, b))
            
            return points_3d
        except Exception as e:
            print(f"Error loading 2D tile as 3D for {self.id}: {e}")
            return []

    def _load_2d_tile_deep_extrude(self, csv_path):
        """Load 2D 8x8 tile CSV and convert to 3D voxel representation by extruding back in Z direction (like emoji entities)"""
        tile_data = []
        try:
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
                                        norm_x = float(x)
                                        
                                        # For Adam/Eve entities, don't invert Y to fix upside-down issue
                                        if self.id in ['adam', 'eve']:
                                            norm_y = float(y)  # Don't invert Y for Adam/Eve
                                        else:
                                            norm_y = 7.0 - float(y)  # Invert Y for other entities (like emoji)
                                            
                                        norm_z = float(z)
                                        
                                        tile_data.append((norm_x, norm_y, norm_z, r, g, b))
        
            return tile_data
        except Exception as e:
            print(f"Error loading 2D tile as deep extruded 3D for {self.id}: {e}")
            return []

    def _load_2d_faces_to_3d(self, csv_path):
        """Load 2D 8x8 emoji data from CSV and generate 3D points by mapping onto all 6 faces of a cube"""
        tile_data = []
        try:
            with open(csv_path, 'r', newline='', encoding='utf-8') as csvfile:
                for line in csvfile:
                    if line.strip():  # Skip empty lines
                        # The format is like: "0,0,0","1,5,9","11,39,61",...
                        # So we find all "r,g,b" patterns in quotes
                        matches = re.findall(r'"(\d+,\d+,\d+)"', line)
                        row_colors = []
                        for match in matches:
                            r, g, b = map(int, match.split(','))
                            row_colors.append((r, g, b))
                        
                        if row_colors:  # Only add non-empty rows
                            tile_data.append(row_colors)
            
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
                for grid_y in range(len(tile_data)):
                    for grid_x in range(len(tile_data[grid_y])):
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
            print(f"Error loading 2D tile as face-copied 3D for {self.id}: {e}")
            return []

    def load_from_mc_texture(self, texture_name):
        """Load entity from Minecraft texture extracted CSVs using appropriate algorithm"""
        # Look for the texture file in the mc_extracted_csvs_8x8 directory
        # Use absolute path to the mc_extracted_csvs_8x8 directory
        mc_2d_path = f'/home/no/Desktop/qwen/#.mc&rmmv-jpg2csv_c2/!.project3d-2-2d/fix/plugy3d-engine-2026_v16.0004/^.fin_mc_8&16_c2--]FIXD/mc_extracted_csvs_8x8/{texture_name}/{texture_name}.csv'
        
        # Also try relative path in case absolute path doesn't work
        if not os.path.exists(mc_2d_path):
            mc_2d_path = os.path.join(os.path.dirname(__file__), '..', '..', '..', '^.fin_mc_8&16_c2--]FIXD', 'mc_extracted_csvs_8x8', texture_name, f'{texture_name}.csv')
        
        # Also try even simpler relative path
        if not os.path.exists(mc_2d_path):
            mc_2d_path = os.path.join('^.fin_mc_8&16_c2--]FIXD', 'mc_extracted_csvs_8x8', texture_name, f'{texture_name}.csv')
        
        if os.path.exists(mc_2d_path):
            # Use deep extrusion for Adam/Eve entities, face-copying for others
            if self.id in ['adam', 'eve']:
                raw_voxel_data = self._load_2d_tile_deep_extrude(mc_2d_path)
            else:
                raw_voxel_data = self._load_2d_faces_to_3d(mc_2d_path)
        else:
            print(f"Minecraft texture {texture_name} not found at any expected location")
            return False

        if not raw_voxel_data:
            print(f"No voxel data found for {self.id}")
            return False

        # Normalize coordinates
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
            norm_x = 7.0 * (x - min_x) / size_x
            norm_y = 7.0 * (y - min_y) / size_y
            norm_z = 7.0 * (z - min_z) / size_z
            
            # Invert Y for correct display
            inverted_norm_y = 7.0 - norm_y

            normalized_voxel_data.append((norm_x, inverted_norm_y, norm_z, r, g, b))

        self.voxel_model = normalized_voxel_data
        self.loaded = True
        print(f"Successfully loaded Minecraft texture {texture_name} as 6-faced 3D model for {self.id} with {len(self.voxel_model)} voxels")
        return True


def load_entity_from_definition(entity_def):
    """Load an entity based on definition from project file"""
    # Extract properties from entity definition
    entity_id = entity_def.get('id', 'unknown')
    pos_x = entity_def.get('pos_x', entity_def.get('x', 0))
    pos_y = entity_def.get('pos_y', entity_def.get('y', 0))
    pos_z = entity_def.get('pos_z', entity_def.get('z', 0))
    color = entity_def.get('color', 'white')
    source_file = entity_def.get('source_file', '')
    
    # Create entity instance
    entity = Entity3D(entity_id, x=pos_x, y=pos_y, z=pos_z, color=color)
    
    # Load from source file if provided
    if source_file:
        if os.path.isfile(source_file):
            entity.load_specific_voxel_file(source_file)
        elif os.path.isdir(os.path.dirname(source_file)):
            # Try loading from directory
            entity.load_voxel_model(os.path.dirname(source_file))
    
    return entity


class EntityManagerPlugin:
    """Plugin for loading and managing entities that works with the Project Manager."""
    def __init__(self, main_app):
        self.main_app = main_app
        self.grid_widget = None
        self.entities = {}
        self.entities_base_path = 'data/entities'

    def initialize(self, voxel_grid_widget):
        """Initialize the entity manager."""
        self.grid_widget = voxel_grid_widget
        if self.grid_widget:
            self.load_all_entities()
            # Store reference to this plugin in the grid widget so other plugins can access it
            self.grid_widget.entity_manager_plugin = self
            self.sync_grid_entities()
            print("Entity Manager Plugin initialized")
        else:
            print("Entity Manager Plugin: Could not find voxel_grid_widget.")

    def load_all_entities(self):
        """Load all available entities with default positions."""
        
        # Load entities from MC extracted textures first, then fall back to original locations
        # Create sample entities using Minecraft blocks as examples
        cobblestone_entity = Entity3D('cobblestone', x=0, y=0, z=0, color='gray')
        if cobblestone_entity.load_from_mc_texture('cobblestone'):
            self.entities['cobblestone'] = cobblestone_entity
        
        dirt_entity = Entity3D('dirt', x=1, y=0, z=0, color='brown')
        if dirt_entity.load_from_mc_texture('dirt'):
            self.entities['dirt'] = dirt_entity
            
        stone_entity = Entity3D('stone', x=2, y=0, z=0, color='gray')
        if stone_entity.load_from_mc_texture('stone'):
            self.entities['stone'] = stone_entity
            
        grass_entity = Entity3D('grass', x=3, y=0, z=0, color='green')
        if grass_entity.load_from_mc_texture('grass'):
            self.entities['grass'] = grass_entity
            
        wood_entity = Entity3D('wood', x=4, y=0, z=0, color='brown')
        if wood_entity.load_from_mc_texture('planks'):
            self.entities['wood'] = wood_entity

    def sync_grid_entities(self):
        """Sync loaded entities with the voxel_grid's entity dictionary."""
        if not self.grid_widget:
            return
        
        # First, clear all entities and start fresh
        self.grid_widget.entities.clear()
        
        # Add entities
        for entity_id, entity in self.entities.items():
            pos_key = f"{entity.x},{entity.y},{entity.z}"
            # Add entity to grid with its specific properties
            self.grid_widget.entities[pos_key] = {
                'id': entity.id,
                'obj_type': 'entity',  # Mark as entity
                'x': entity.x,
                'y': entity.y,
                'z': entity.z,
                'color': entity.color,
                'voxel_model': entity.voxel_model
            }
        
        # After adding entities, also add any terrain blocks that exist
        # (Note: terrain blocks may not exist yet if terrain generator runs after entity manager)
        self.combine_with_terrain_blocks()
        
        self.grid_widget.update()
        print(f"Synced {len(self.entities)} entities with the grid and combined with terrain blocks if they exist.")

    def combine_with_terrain_blocks(self):
        """Combine terrain blocks with entities so both are selectable."""
        if not self.grid_widget or not hasattr(self.grid_widget, 'terrain_blocks'):
            return
            
        # Add terrain blocks to the entities dictionary so they're selectable too
        for pos_key, terrain_data in self.grid_widget.terrain_blocks.items():
            # Only add if position is not already occupied by an entity
            if pos_key not in self.grid_widget.entities:
                self.grid_widget.entities[pos_key] = {
                    'id': terrain_data['id'],
                    'obj_type': 'terrain',  # Mark as terrain
                    'x': terrain_data['x'],  # Keep the same x/y/z structure
                    'y': terrain_data['y'],
                    'z': terrain_data['z'],
                    'block_type': terrain_data['block_type'],
                    'color': 'neutral',  # Provide a default color for compatibility with minimap
                    'voxel_model': terrain_data['voxel_model']
                }
    
    def update_combined_view(self):
        """Method that can be called externally to refresh the combined view."""
        if not self.grid_widget:
            return
        
        # Start fresh with entities
        temp_entities = {}
        for entity_id, entity in self.entities.items():
            pos_key = f"{entity.x},{entity.y},{entity.z}"
            temp_entities[pos_key] = {
                'id': entity.id,
                'obj_type': 'entity',
                'x': entity.x,
                'y': entity.y,
                'z': entity.z,
                'color': entity.color,
                'voxel_model': entity.voxel_model
            }
        
        # Add terrain blocks
        if hasattr(self.grid_widget, 'terrain_blocks'):
            for pos_key, terrain_data in self.grid_widget.terrain_blocks.items():
                if pos_key not in temp_entities:  # Only add if no entity is there
                    temp_entities[pos_key] = {
                        'id': terrain_data['id'],
                        'obj_type': 'terrain',
                        'x': terrain_data['x'],
                        'y': terrain_data['y'],
                        'z': terrain_data['z'],
                        'block_type': terrain_data['block_type'],
                        'voxel_model': terrain_data['voxel_model']
                    }
        
        # Replace the grid's entities
        self.grid_widget.entities = temp_entities
        self.grid_widget.update()

# Export the plugin class
Plugin = EntityManagerPlugin
