"""
Project Manager Plugin
Handles loading and managing game projects from project directories.
Integrates with terrain and emoji entity systems to provide unified project management.
"""

import os
import json
import csv
from pathlib import Path

# Import from other modules to enable unified entity loading
from plugins.entities.emoji_entity_manager import load_emoji_entity
from plugins.terrain.terrain_generator import load_terrain_block, generate_initial_terrain


class ProjectManagerPlugin:
    """Plugin for loading and managing game projects."""
    def __init__(self, main_app):
        self.main_app = main_app
        self.grid_widget = None
        self.current_project = None
        self.project_directory = "projects"
        
        # Create projects directory if it doesn't exist
        Path(self.project_directory).mkdir(exist_ok=True)

    def initialize(self, voxel_grid_widget):
        """Initialize the project manager."""
        self.grid_widget = voxel_grid_widget
        if self.grid_widget:
            print("Project Manager Plugin initialized")
        else:
            print("Project Manager Plugin: Could not find voxel_grid_widget.")

    def load_project(self, project_name):
        """Load a project from its directory"""
        project_path = Path(self.project_directory) / project_name
        if not project_path.exists():
            print(f"Project {project_name} not found")
            return False
        
        # Load project metadata if it exists
        metadata_path = project_path / "metadata.json"
        if metadata_path.exists():
            with open(metadata_path) as f:
                metadata = json.load(f)
        
        # Load both piece maps and individual pieces from the project directory
        pieces_dir = project_path / "peices"
        
        # Collect all pieces to load
        all_pieces = []
        
        # First, load all piece_map.csv files (can be multiple files with patterns like *.piece_map.csv)
        for piece_map_file in pieces_dir.glob("*piece_map.csv"):
            print(f"Loading piece map: {piece_map_file.name}")
            piece_map_pieces = self._load_piece_map(piece_map_file)
            all_pieces.extend(piece_map_pieces)
        
        # Then, load all individual piece files (.txt files)
        individual_pieces = self._load_individual_pieces(pieces_dir)
        all_pieces.extend(individual_pieces)
        
        # If no pieces were found at all, create default demo pieces
        if not all_pieces:
            print("No pieces found in project, creating default demo pieces...")
            all_pieces = self._create_default_demo_pieces()
        
        # Apply all loaded pieces to the grid
        self._apply_pieces_to_grid(all_pieces)
        
        self.current_project = project_name
        print(f"Loaded project {project_name} with {len(pieces)} pieces")
        return True

    def _create_default_demo_pieces(self):
        """Create default demo pieces if no project pieces exist"""
        # Create demo pieces similar to the original system
        demo_pieces = [
            {
                "id": "cow",
                "type": "emoji_entity",
                "scale_x": 1.0,
                "scale_y": 1.0,
                "scale_z": 1.0,
                "render_style": "extruded",
                "pos_x": 5,
                "pos_y": 0,
                "pos_z": 5,
                "color": "brown",
                "source_file": "data/entities/emoji/cow/cow.csv",
                "rotation_x": 0,
                "rotation_y": 0,
                "rotation_z": 0,
                "movable": True,
                "interactable": True,
                "health": 100
            },
            {
                "id": "pig",
                "type": "emoji_entity",
                "scale_x": 1.0,
                "scale_y": 1.0,
                "scale_z": 1.0,
                "render_style": "extruded",
                "pos_x": 10,
                "pos_y": 0,
                "pos_z": 1,
                "color": "pink",
                "source_file": "data/entities/emoji/pig/pig.csv",
                "rotation_x": 0,
                "rotation_y": 0,
                "rotation_z": 0,
                "movable": True,
                "interactable": True,
                "health": 80
            }
        ]
        
        # Add some terrain blocks as well
        terrain_blocks = [
            {
                "id": "grass_block",
                "type": "terrain",
                "block_type": "grass",
                "pos_x": 0,
                "pos_y": 0,
                "pos_z": 0,
                "color": "green",
                "source_file": "^.fin_mc_8&16_c2--]FIXD/mc_extracted_csvs_8x8/grass/grass.csv",
                "scale_x": 1.0,
                "scale_y": 1.0,
                "scale_z": 1.0,
                "render_style": "six-faced"
            }
        ]
        
        demo_pieces.extend(terrain_blocks)
        return demo_pieces

    def save_project(self, project_name=None):
        """Save current grid state to project directory"""
        project_name = project_name or self.current_project
        if not project_name:
            print("No project currently loaded or specified")
            return False
        
        project_path = Path(self.project_directory) / project_name
        project_path.mkdir(exist_ok=True)
        
        # Ensure peices directory exists
        (project_path / "peices").mkdir(exist_ok=True)
        
        # Save current grid state to piece_map.csv
        piece_map_path = project_path / "peices" / "piece_map.csv"
        self._save_current_pieces(piece_map_path)
        
        print(f"Saved project {project_name}")
        return True

    def create_new_project(self, project_name, template=None):
        """Create a new project directory"""
        project_path = Path(self.project_directory) / project_name
        project_path.mkdir(exist_ok=True)
        
        # Create default structure
        (project_path / "peices").mkdir(exist_ok=True)
        
        # Create default metadata
        self._create_default_metadata(project_path, project_name)
        
        print(f"Created new project {project_name}")
        return True

    def _load_piece_map(self, piece_map_file):
        """Load piece definitions from CSV file"""
        pieces = []
        with open(piece_map_file, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                piece = {}
                for key, value in row.items():
                    # Convert numeric strings to numbers
                    if value.replace('.', '', 1).replace('-', '', 1).isdigit():
                        piece[key] = float(value) if '.' in value else int(value)
                    # Convert boolean strings
                    elif value.lower() in ['true', 'false']:
                        piece[key] = value.lower() == 'true'
                    else:
                        piece[key] = value
                pieces.append(piece)
        return pieces
    
    def _load_individual_pieces(self, pieces_dir):
        """Load piece definitions from individual JSON files"""
        pieces = []
        
        # Load all .txt files in the pieces directory (these are JSON-formatted)
        for file_path in pieces_dir.glob("*.txt"):
            with open(file_path, 'r') as f:
                try:
                    piece = json.load(f)
                    pieces.append(piece)
                except json.JSONDecodeError:
                    print(f"Error parsing JSON in {file_path.name}")
        
        return pieces

    def _apply_pieces_to_grid(self, pieces):
        """Apply loaded pieces to the voxel grid"""
        if not self.grid_widget:
            return
        
        # Preserve existing entities, don't clear them
        existing_entities = self.grid_widget.entities.copy()
        
        # Load each piece according to its type
        for piece in pieces:
            pos_key = f"{piece['pos_x']},{piece['pos_y']},{piece['pos_z']}"
            
            # Handle different entity types
            obj_type = piece.get('type', 'entity')
            
            if obj_type == 'emoji_entity':
                # Load emoji entity
                entity = load_emoji_entity(piece)
                entity_data = {
                    'id': entity.id,
                    'obj_type': 'emoji_entity',
                    'x': entity.x,
                    'y': entity.y,
                    'z': entity.z,
                    'color': entity.color,
                    'voxel_model': entity.voxel_model,
                    'scale_x': piece.get('scale_x', 1.0),
                    'scale_y': piece.get('scale_y', 1.0),
                    'scale_z': piece.get('scale_z', 1.0),
                    'render_style': piece.get('render_style', 'extruded'),
                    'source_file': piece.get('source_file', '')
                }
            elif obj_type == 'terrain':
                # Load terrain block
                entity = load_terrain_block(piece)
                entity_data = {
                    'id': f"terrain_{piece.get('block_type', 'unknown')}_{int(piece.get('pos_x', 0))}_{int(piece.get('pos_y', 0))}_{int(piece.get('pos_z', 0))}",
                    'obj_type': 'terrain',
                    'x': entity.x,
                    'y': entity.y,
                    'z': entity.z,
                    'block_type': entity.block_type,
                    'voxel_model': entity.voxel_model,
                    'scale_x': piece.get('scale_x', 1.0),
                    'scale_y': piece.get('scale_y', 1.0),
                    'scale_z': piece.get('scale_z', 1.0),
                    'render_style': piece.get('render_style', 'six-faced'),
                    'color': piece.get('color', 'neutral'),
                    'source_file': piece.get('source_file', '')
                }
            else:
                # Generic entity handling
                entity_data = {
                    'id': piece['id'],
                    'obj_type': obj_type,
                    'x': piece['pos_x'],
                    'y': piece['pos_y'],
                    'z': piece['pos_z'],
                    'color': piece['color'],
                    'scale_x': piece.get('scale_x', 1.0),
                    'scale_y': piece.get('scale_y', 1.0),
                    'scale_z': piece.get('scale_z', 1.0),
                    'render_style': piece.get('render_style', 'extruded'),
                    'source_file': piece['source_file']
                }
                
                # Add any additional properties
                for key, value in piece.items():
                    if key not in entity_data:
                        entity_data[key] = value
            
            # Add advanced scaling parameters if they exist
            scale_mode = piece.get('scale_mode', '')
            if isinstance(scale_mode, str) and scale_mode and scale_mode.strip():
                entity_data['scale_mode'] = scale_mode
            voxel_density_val = piece.get('voxel_density', '')
            if voxel_density_val is not None and str(voxel_density_val).strip():
                try:
                    entity_data['voxel_density'] = float(voxel_density_val)
                except (ValueError, TypeError):
                    pass  # Skip if not a valid number
            voxel_expansion_val = piece.get('voxel_expansion', '')
            if voxel_expansion_val is not None and str(voxel_expansion_val).strip():
                try:
                    entity_data['voxel_expansion'] = float(voxel_expansion_val)
                except (ValueError, TypeError):
                    pass  # Skip if not a valid number
            
            # Add this new piece to the entities (may overwrite existing at same position)
            existing_entities[pos_key] = entity_data
        
        # Update the grid with combined entities
        self.grid_widget.entities = existing_entities
        
        # If entity manager exists, make sure to update the combined view
        if hasattr(self.grid_widget, 'entity_manager_plugin'):
            if hasattr(self.grid_widget.entity_manager_plugin, 'combine_with_terrain_blocks'):
                self.grid_widget.entity_manager_plugin.combine_with_terrain_blocks()
        
        # Trigger grid update
        if hasattr(self.grid_widget, 'update'):
            self.grid_widget.update()
            
        # Trigger minimap update if the minimap plugin exists
        # This ensures that newly loaded pieces appear in the minimap immediately
        try:
            if hasattr(self.grid_widget, 'minimap_plugin') and self.grid_widget.minimap_plugin:
                # Reset the last selector position to trigger an update since entities changed
                self.grid_widget.minimap_plugin.last_selector_pos = None
                # Then call update to force refresh with new entities
                self.grid_widget.minimap_plugin.update()
        except AttributeError:
            # In case minimap_plugin isn't available yet
            pass

    def _save_current_pieces(self, piece_map_file):
        """Save current grid entities to CSV file"""
        if not self.grid_widget or not hasattr(self.grid_widget, 'entities'):
            return
        
        fieldnames = [
            'id', 'type', 'scale_x', 'scale_y', 'scale_z', 'render_style',
            'pos_x', 'pos_y', 'pos_z', 'color', 'source_file', 'rotation_x', 
            'rotation_y', 'rotation_z', 'movable', 'interactable', 'health'
        ]
        
        with open(piece_map_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            
            for pos_key, entity in self.grid_widget.entities.items():
                # Parse position from key
                pos_parts = pos_key.split(',')
                if len(pos_parts) >= 3:
                    row = {
                        'id': entity.get('id', ''),
                        'type': entity.get('obj_type', 'entity'),
                        'scale_x': entity.get('scale_x', 1.0),
                        'scale_y': entity.get('scale_y', 1.0),
                        'scale_z': entity.get('scale_z', 1.0),
                        'render_style': entity.get('render_style', 'extruded'),
                        'pos_x': float(pos_parts[0]),
                        'pos_y': float(pos_parts[1]),
                        'pos_z': float(pos_parts[2]),
                        'color': entity.get('color', 'white'),
                        'source_file': entity.get('source_file', ''),
                        'rotation_x': entity.get('rotation_x', 0),
                        'rotation_y': entity.get('rotation_y', 0),
                        'rotation_z': entity.get('rotation_z', 0),
                        'movable': entity.get('movable', True),
                        'interactable': entity.get('interactable', True),
                        'health': entity.get('health', 100)
                    }
                    writer.writerow(row)

    def _create_default_metadata(self, project_path, project_name):
        """Create default metadata file for the project"""
        metadata = {
            "name": project_name,
            "author": "Unknown",
            "version": "1.0",
            "created": "2026-01-26",
            "description": f"Default project: {project_name}"
        }
        
        with open(project_path / "metadata.json", 'w') as f:
            json.dump(metadata, f, indent=2)


# Export the plugin class
Plugin = ProjectManagerPlugin