"""
Entity Renderer Plugin with VBO Support
Handles rendering of all entities and terrain blocks with VBO optimization while maintaining selection functionality.
"""

from OpenGL.GL import *
import numpy as np
from OpenGL.arrays import vbo
import ctypes


class EntityRendererPlugin:
    """Plugin for rendering entities and terrain blocks on the voxel grid."""
    def __init__(self, main_app):
        self.main_app = main_app
        self.grid_widget = None
        self.vbos = {}  # Cache VBOs by unique voxel model ID
        self.vertex_cache = {}  # Cache generated vertex data

    def initialize(self, voxel_grid_widget):
        """Initialize the entity renderer."""
        self.grid_widget = voxel_grid_widget
        if self.grid_widget:
            # Patch the draw_entities method
            self.grid_widget.draw_entities = self.draw_entities
            print("Entity Renderer Plugin initialized with VBO support")
        else:
            print("Entity Renderer Plugin: Could not find voxel_grid_widget.")

    def generate_voxel_vertices(self, x, y, z, size, color):
        """Generate vertex data for a single voxel cube at position (x,y,z)."""
        r, g, b = color
        half = size / 2.0
        
        # Define the 6 faces (12 triangles) of the cube using vertex positions
        vertex_data = []
        
        # Front face (facing +Z)
        vertex_data.extend([
            x - half, y - half, z + half, r, g, b,  # Bottom-left
            x + half, y - half, z + half, r, g, b,  # Bottom-right
            x + half, y + half, z + half, r, g, b,  # Top-right
            x - half, y - half, z + half, r, g, b,  # Bottom-left
            x + half, y + half, z + half, r, g, b,  # Top-right
            x - half, y + half, z + half, r, g, b   # Top-left
        ])
        
        # Back face (facing -Z)
        vertex_data.extend([
            x - half, y - half, z - half, r, g, b,  # Bottom-left
            x + half, y + half, z - half, r, g, b,  # Top-right
            x + half, y - half, z - half, r, g, b,  # Bottom-right
            x - half, y - half, z - half, r, g, b,  # Bottom-left
            x - half, y + half, z - half, r, g, b,  # Top-left
            x + half, y + half, z - half, r, g, b   # Top-right
        ])
        
        # Top face (facing +Y)
        vertex_data.extend([
            x - half, y + half, z - half, r, g, b,  # Top-left
            x + half, y + half, z - half, r, g, b,  # Top-right
            x + half, y + half, z + half, r, g, b,  # Bottom-right
            x - half, y + half, z - half, r, g, b,  # Top-left
            x + half, y + half, z + half, r, g, b,  # Bottom-right
            x - half, y + half, z + half, r, g, b   # Bottom-left
        ])
        
        # Bottom face (facing -Y)
        vertex_data.extend([
            x - half, y - half, z - half, r, g, b,  # Top-left
            x + half, y - half, z + half, r, g, b,  # Bottom-right
            x + half, y - half, z - half, r, g, b,  # Bottom-left
            x - half, y - half, z - half, r, g, b,  # Top-left
            x - half, y - half, z + half, r, g, b,  # Top-right
            x + half, y - half, z + half, r, g, b   # Bottom-right
        ])
        
        # Right face (facing +X)
        vertex_data.extend([
            x + half, y - half, z - half, r, g, b,  # Bottom-left
            x + half, y + half, z - half, r, g, b,  # Top-left
            x + half, y + half, z + half, r, g, b,  # Top-right
            x + half, y - half, z - half, r, g, b,  # Bottom-left
            x + half, y + half, z + half, r, g, b,  # Top-right
            x + half, y - half, z + half, r, g, b   # Bottom-right
        ])
        
        # Left face (facing -X)
        vertex_data.extend([
            x - half, y - half, z - half, r, g, b,  # Bottom-right
            x - half, y + half, z + half, r, g, b,  # Top-left
            x - half, y + half, z - half, r, g, b,  # Top-right
            x - half, y - half, z - half, r, g, b,  # Bottom-right
            x - half, y - half, z + half, r, g, b,  # Bottom-left
            x - half, y + half, z + half, r, g, b   # Top-left
        ])
        
        return np.array(vertex_data, dtype=np.float32)

    def get_voxel_model_key(self, voxel_model):
        """Create a unique key for a voxel model based on its content."""
        # Create a hash based on the sorted content of the voxel model
        sorted_model = tuple(sorted([(int(x), int(y), int(z), r, g, b) for x, y, z, r, g, b in voxel_model]))
        return hash(sorted_model)

    def get_or_create_vbo_for_model(self, voxel_model, voxel_size):
        """Get or create a VBO for a specific voxel model to avoid recreating identical geometry."""
        # Create a unique key for this voxel model
        model_key = self.get_voxel_model_key(voxel_model)
        cache_key = f"{model_key}_{voxel_size}"
        
        # Return cached VBO if it exists
        if cache_key in self.vbos:
            return self.vbos[cache_key]
        
        # Generate vertex data for the entire model
        all_vertices = []
        
        for voxel in voxel_model:
            vx, vy, vz, r, g, b = voxel
            px = vx * voxel_size
            py = vy * voxel_size
            pz = vz * voxel_size
            
            # Generate vertices for this voxel
            voxel_vertices = self.generate_voxel_vertices(px, py, pz, voxel_size, (r/255.0, g/255.0, b/255.0))
            all_vertices.extend(voxel_vertices)
        
        if not all_vertices:
            return None
            
        # Convert to numpy array
        vertex_array = np.array(all_vertices, dtype=np.float32)
        
        # Create VBO
        vbo_obj = vbo.VBO(vertex_array)
        self.vbos[cache_key] = vbo_obj
        
        return vbo_obj

    def draw_entities(self):
        """Draw all entities and terrain blocks in the grid using VBOs."""
        if not self.grid_widget:
            return

        # Enable vertex arrays
        glEnableClientState(GL_VERTEX_ARRAY)
        glEnableClientState(GL_COLOR_ARRAY)
        
        # Draw objects from the unified entities dictionary (contains both entities and terrain blocks)
        # This ensures that moved/selected terrain blocks are properly removed
        for pos_key, entity_data in self.grid_widget.entities.items():
            # Check if this is an entity or terrain block
            obj_type = entity_data.get('obj_type', 'entity')
            if obj_type == 'terrain':
                # Draw as terrain block (no foundation)
                self.draw_single_terrain_block_vbo(entity_data)
            else:
                # Draw as entity (with foundation)
                self.draw_single_entity_vbo(entity_data)
        
        # Also draw the selected entity if one is being held
        if self.grid_widget and self.grid_widget.is_selected:
             selected_entity_data = self.grid_widget.selected_entity_data
             selector_pos = self.grid_widget.selector_plugin.get_selector_position()
             
             # Create a temporary entity object to draw at selector's position
             temp_entity = selected_entity_data.copy()
             temp_entity['x'] = selector_pos[0]
             temp_entity['y'] = selector_pos[1]
             temp_entity['z'] = selector_pos[2]
             # Draw the held object based on its type
             obj_type = selected_entity_data.get('obj_type', 'entity')
             if obj_type == 'terrain':
                 temp_entity['obj_type'] = 'terrain'
                 self.draw_single_terrain_block_vbo(temp_entity)
             else:
                 temp_entity['obj_type'] = 'entity'
                 self.draw_single_entity_vbo(temp_entity)
        
        # Also draw the selected entity if one is being held
        if self.grid_widget and self.grid_widget.is_selected:
             selected_entity_data = self.grid_widget.selected_entity_data
             selector_pos = self.grid_widget.selector_plugin.get_selector_position()
             
             # Create a temporary entity object to draw at selector's position
             temp_entity = selected_entity_data.copy()
             temp_entity['x'] = selector_pos[0]
             temp_entity['y'] = selector_pos[1]
             temp_entity['z'] = selector_pos[2]
             self.draw_single_entity_vbo(temp_entity)
        
        # Disable vertex arrays
        glDisableClientState(GL_VERTEX_ARRAY)
        glDisableClientState(GL_COLOR_ARRAY)

    def draw_single_entity_vbo(self, entity):
        """Draw a single entity using VBOs."""
        if not self.grid_widget:
            return

        cell_size = self.grid_widget.cell_size
        x_base = entity['x'] * cell_size
        y_base = entity['y'] * cell_size  
        z_base = entity['z'] * cell_size

        # Draw foundation - keep this as immediate mode since it's simple
        # Only draw foundation for entities, not for terrain blocks
        if entity.get('obj_type', 'entity') == 'entity':  # Entities get colored foundations
            glPushMatrix()
            glTranslatef(x_base, y_base, z_base)
            if entity.get('color') == 'red':
                glColor4f(1.0, 0.3, 0.3, 0.3)
            elif entity.get('color') == 'blue':
                glColor4f(0.0, 0.5, 1.0, 0.3)
            else:
                glColor4f(0.5, 0.5, 0.5, 0.3)
            self.draw_flat_cube_immediate(cell_size)
            glPopMatrix()
        # For terrain blocks, no foundation is drawn (they blend into the terrain)

        # Draw voxel model using VBOs
        if 'voxel_model' in entity and entity['voxel_model']:
            self.draw_3d_model_vbo(x_base, y_base, z_base, entity['voxel_model'], cell_size)

    def draw_single_terrain_block_vbo(self, terrain_block):
        """Draw a single terrain block using VBOs."""
        if not self.grid_widget:
            return

        cell_size = self.grid_widget.cell_size
        x_base = terrain_block['x'] * cell_size
        y_base = terrain_block['y'] * cell_size
        z_base = terrain_block['z'] * cell_size

        # Draw voxel model only for terrain (no colored foundation like entities)
        if 'voxel_model' in terrain_block and terrain_block['voxel_model']:
            self.draw_3d_model_vbo(x_base, y_base, z_base, terrain_block['voxel_model'], cell_size)

    def draw_3d_model_vbo(self, base_x, base_y, base_z, voxel_model, cell_size):
        """Draw a 3D voxel model using VBOs."""
        # Calculate the voxel size as normal
        voxel_size = cell_size / 8.0
        
        # Get the current entity being rendered to check for scaling parameters
        # This is a bit tricky since we need the actual entity data
        # For now, we'll just get the voxel_size adjusted by any scaling factors
        
        # We'll modify the voxel_size based on the scaling mode if drawing an entity at this position
        # Note: we need to reference the actual entity data to get scaling params
        # Let's pass entity info to this function
        
        # First implementation: assume we can access the entity data from somewhere
        # For this, we'll need to modify the calling function to pass entity data
        # So we'll need to temporarily modify the approach

        # For now, we'll implement it based on our knowledge from the context
        # Since this is called from draw_single_entity_vbo, let's modify that function to handle scaling
        # But in this function, we'll make it more general by accepting an optional scaling parameter
        
        # This will be handled by the calling function now, with a simple implementation
        self._draw_3d_model_with_scaling(base_x, base_y, base_z, voxel_model, voxel_size)

    def _draw_3d_model_with_scaling(self, base_x, base_y, base_z, voxel_model, base_voxel_size):
        """Draw a 3D voxel model with scaling support."""
        # For now, we need to get the entity information to determine scaling
        # This is a bit complex since we might not have the entity object here
        # Let's assume that this function is only called for entities that are already properly positioned
        
        # Since the calling function (draw_single_entity_vbo) has access to the full entity data,
        # we'll modify it to handle the scaling calculation before calling this
        
        # Get or create VBO for this model
        vbo_obj = self.get_or_create_vbo_for_model(voxel_model, base_voxel_size)
        if vbo_obj is None:
            return
            
        # Push matrix to translate to the correct position
        glPushMatrix()
        glTranslatef(base_x, base_y, base_z)
        
        # Bind the VBO
        vbo_obj.bind()
        
        # Specify the layout of the vertex data
        stride = 6 * 4  # 6 floats (x, y, z, r, g, b) * 4 bytes each
        glVertexPointer(3, GL_FLOAT, stride, None)  # Position starts at offset 0
        glColorPointer(3, GL_FLOAT, stride, ctypes.c_void_p(3 * 4))  # Color starts after 3 floats
        
        # Calculate number of vertices to draw
        num_vertices = len(voxel_model) * 36  # Each voxel approximates to 12 triangles * 3 vertices = 36 vertices
        
        # Draw the arrays
        if num_vertices > 0:
            glDrawArrays(GL_TRIANGLES, 0, num_vertices)
        
        # Unbind the VBO
        vbo_obj.unbind()
        glPopMatrix()

    def draw_single_entity_vbo(self, entity):
        """Draw a single entity using VBOs with scaling support."""
        if not self.grid_widget:
            return

        cell_size = self.grid_widget.cell_size
        x_base = entity['x'] * cell_size
        y_base = entity['y'] * cell_size  
        z_base = entity['z'] * cell_size

        # Draw foundation - keep this as immediate mode since it's simple
        # Only draw foundation for entities, not for terrain blocks
        if entity.get('obj_type', 'entity') == 'entity':  # Entities get colored foundations
            glPushMatrix()
            glTranslatef(x_base, y_base, z_base)
            if entity.get('color') == 'red':
                glColor4f(1.0, 0.3, 0.3, 0.3)
            elif entity.get('color') == 'blue':
                glColor4f(0.0, 0.5, 1.0, 0.3)
            elif entity.get('color') == 'green':
                glColor4f(0.0, 1.0, 0.0, 0.3)
            elif entity.get('color') == 'pink':
                glColor4f(1.0, 0.7, 0.7, 0.3)
            elif entity.get('color') == 'brown':
                glColor4f(0.6, 0.4, 0.2, 0.3)
            else:
                glColor4f(0.5, 0.5, 0.5, 0.3)
            self.draw_flat_cube_immediate(cell_size)
            glPopMatrix()

        # Calculate adjusted voxel size based on scaling parameters
        scale_x = entity.get('scale_x', 1.0)
        scale_y = entity.get('scale_y', 1.0) 
        scale_z = entity.get('scale_z', 1.0)
        
        # For advanced scaling, we may need to adjust how voxels are spaced
        base_voxel_size = cell_size / 8.0
        
        # Apply entity-level scaling
        adjusted_voxel_size = base_voxel_size * min(scale_x, scale_y, scale_z)  # Use minimum to maintain proportions
        
        # Draw voxel model using VBOs
        if 'voxel_model' in entity and entity['voxel_model']:
            # Need to apply any special scaling modes for micro/macro rendering
            self._draw_scaled_entity(x_base, y_base, z_base, entity, adjusted_voxel_size)

    def _draw_scaled_entity(self, base_x, base_y, base_z, entity, voxel_size):
        """Handle advanced scaling modes for entity rendering."""
        # Check if this entity has special scaling
        scale_mode = entity.get('scale_mode', '')
        
        if scale_mode == 'micro':
            # Micro scaling: make voxels smaller to pack more into same space
            voxel_density = entity.get('voxel_density', 1.0)
            adjusted_voxel_size = voxel_size / voxel_density
            self._draw_3d_model_with_scaling(base_x, base_y, base_z, entity['voxel_model'], adjusted_voxel_size)
        elif scale_mode == 'macro':
            # Macro scaling: make each voxel occupy more space
            voxel_expansion = entity.get('voxel_expansion', 1.0)
            adjusted_voxel_size = voxel_size * (1.0 / voxel_expansion)  # Inverse since expansion is about grid space
            self._draw_3d_model_with_scaling(base_x, base_y, base_z, entity['voxel_model'], adjusted_voxel_size)
        else:
            # Normal scaling
            self._draw_3d_model_with_scaling(base_x, base_y, base_z, entity['voxel_model'], voxel_size)

    def draw_single_terrain_block_vbo(self, terrain_block):
        """Draw a single terrain block using VBOs."""
        if not self.grid_widget:
            return

        cell_size = self.grid_widget.cell_size
        x_base = terrain_block['x'] * cell_size
        y_base = terrain_block['y'] * cell_size
        z_base = terrain_block['z'] * cell_size

        # Calculate adjusted voxel size based on scaling parameters
        scale_x = terrain_block.get('scale_x', 1.0)
        scale_y = terrain_block.get('scale_y', 1.0) 
        scale_z = terrain_block.get('scale_z', 1.0)
        
        # Apply entity-level scaling
        base_voxel_size = cell_size / 8.0
        adjusted_voxel_size = base_voxel_size * min(scale_x, scale_y, scale_z)
        
        # Draw voxel model only for terrain (no colored foundation like entities)
        if 'voxel_model' in terrain_block and terrain_block['voxel_model']:
            # For terrain blocks, use normal scaling (no micro/macro modes typically)
            self._draw_3d_model_with_scaling(x_base, y_base, z_base, terrain_block['voxel_model'], adjusted_voxel_size)

    def draw_flat_cube_immediate(self, size):
        """Draw a thin flat cube for the foundation using immediate mode."""
        height = size * 0.1
        glBegin(GL_QUADS)
        # Top face
        glVertex3f(0, 0, 0)
        glVertex3f(size, 0, 0)
        glVertex3f(size, 0, size)
        glVertex3f(0, 0, size)
        glEnd()

# Export the plugin class
Plugin = EntityRendererPlugin