#!/usr/bin/env python3
"""
Minecraft Block Gallery Viewer
Displays all Minecraft blocks in a tiled grid layout with names underneath, similar to a file browser.
Includes scrollbars for navigating through many blocks.
"""

import os
import sys
import csv
import math
from PIL import Image, ImageFont, ImageDraw
import numpy as np

try:
    import pygame
    from pygame.locals import *
    from OpenGL.GL import *
    from OpenGL.GLU import *
except ImportError as e:
    print(f"Missing required modules: {e}")
    print("Install with: pip install pygame PyOpenGL PyOpenGL_accelerate")
    sys.exit(1)


class BlockGallery:
    def __init__(self, base_dir, window_size=(1000, 700)):
        self.base_dir = base_dir
        self.window_size = window_size
        self.block_csv_paths = []  # List of (name, csv_path) tuples
        self.tiles_per_row = 8
        self.tile_size = 64  # Size of each tile including name area (48x48 grid + name space)
        self.margin = 10
        self.scroll_offset = 0
        self.max_scroll = 0
        self.dragging_scrollbar = False
        self.scrollbar_handle_y = 0
        self.selected_block = None
        self.font_texture_cache = {}
        self.grid_cache = {}  # Cache for loaded block grids
        self.grid_access_order = []  # Track access order for LRU
        self.texture_cache = {}  # Cache for rendered block textures
        self.visible_tiles_cache = {}  # Cache for visible tiles
        
        # Load block metadata (not the actual grids)
        self.load_block_metadata()
        self.calculate_scroll_params()
        
    def load_block_from_csv(self, csv_file_path):
        """Load block data from a CSV file."""
        grid = []
        try:
            with open(csv_file_path, 'r', newline='', encoding='utf-8') as csvfile:
                reader = csv.reader(csvfile)
                for row in reader:
                    grid_row = []
                    for cell in row:
                        # Parse "r,g,b" format
                        r, g, b = map(int, cell.strip('"').split(','))
                        grid_row.append((r/255.0, g/255.0, b/255.0))  # Normalize to 0-1
                    grid.append(grid_row)
        except Exception as e:
            print(f"Error loading {csv_file_path}: {e}")
            # Return a default grid if there's an error (default to 8x8)
            grid = [[(0.5, 0.5, 0.5) for _ in range(8)] for _ in range(8)]
        return grid

    def load_block_metadata(self):
        """Load block metadata (names and file paths) without loading the actual grids."""
        print("Loading block metadata...")
        csv_files = []
        
        # Look for CSV files in the given base directory structure
        print(f"Looking for blocks in: {self.base_dir}")
        
        # Check if the base_dir is already a specific size directory (like mc_extracted_csvs_8x8)
        if os.path.basename(self.base_dir).endswith(('8x8', '16x16')):
            # If the base_dir is already pointing to a specific size directory
            for item in os.listdir(self.base_dir):
                item_path = os.path.join(self.base_dir, item)
                if os.path.isdir(item_path):
                    # Look for CSV file inside each block directory
                    csv_file = os.path.join(item_path, f"{item}.csv")
                    if os.path.exists(csv_file):
                        csv_files.append((item, csv_file))
        else:
            # Look for different size directories within the base directory
            possible_dirs = []
            for item in os.listdir(self.base_dir):
                item_path = os.path.join(self.base_dir, item)
                if os.path.isdir(item_path):
                    if item.startswith("mc_extracted_csvs") and ('8x8' in item or '16x16' in item):
                        possible_dirs.append(item_path)
            
            # If we found specific size directories, use those
            if possible_dirs:
                for block_dir in possible_dirs:
                    print(f"Looking for blocks in: {block_dir}")
                    for item in os.listdir(block_dir):
                        item_path = os.path.join(block_dir, item)
                        if os.path.isdir(item_path):
                            # Look for CSV file inside each block directory
                            csv_file = os.path.join(item_path, f"{item}.csv")
                            if os.path.exists(csv_file):
                                csv_files.append((item, csv_file))
            else:
                # If no size-specific directories found, check the base directory directly
                for item in os.listdir(self.base_dir):
                    item_path = os.path.join(self.base_dir, item)
                    if os.path.isdir(item_path):
                        # Look for CSV file inside each block directory
                        csv_file = os.path.join(item_path, f"{item}.csv")
                        if os.path.exists(csv_file):
                            csv_files.append((item, csv_file))
        
        # Sort alphabetically
        csv_files.sort(key=lambda x: x[0])
        
        for name, csv_file in csv_files:
            self.block_csv_paths.append((name, csv_file))
        
        print(f"Loaded metadata for {len(self.block_csv_paths)} blocks")

    def get_block_grid(self, name, csv_file):
        """Get block grid with LRU caching."""
        # Move to end of access order if already in cache
        if name in self.grid_cache:
            if name in self.grid_access_order:
                self.grid_access_order.remove(name)
            self.grid_access_order.append(name)
            return self.grid_cache[name]
        
        # Load and cache the grid
        grid = self.load_block_from_csv(csv_file)
        self.grid_cache[name] = grid
        self.grid_access_order.append(name)
        
        # Limit cache size to prevent memory overflow
        if len(self.grid_cache) > 100:  # Keep only the most recently used 100 grids
            oldest_key = self.grid_access_order.pop(0)
            del self.grid_cache[oldest_key]
        
        return grid

    def calculate_scroll_params(self):
        """Calculate scrolling parameters based on number of blocks."""
        num_blocks = len(self.block_csv_paths)
        rows_needed = math.ceil(num_blocks / self.tiles_per_row)
        total_height = rows_needed * (self.tile_size + self.margin) + self.margin
        
        # Maximum scroll is how much we can scroll given window size
        self.max_scroll = max(0, total_height - self.window_size[1] + 50)  # Extra space for scrollbar
        self.scroll_offset = 0  # Reset scroll when recalculating

    def init_pygame(self):
        """Initialize Pygame and OpenGL."""
        # Center the window by setting SDL environment variable before initializing pygame
        os.environ['SDL_VIDEO_CENTERED'] = '1'
        pygame.init()
        self.screen = pygame.display.set_mode(
            self.window_size, 
            DOUBLEBUF | OPENGL
        )
        pygame.display.set_caption("Minecraft Block Gallery - All Blocks")
        
        # Set up OpenGL
        glClearColor(0.15, 0.15, 0.15, 1.0)  # Dark gray background
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
        
        gluOrtho2D(0, self.window_size[0], 0, self.window_size[1])

    def draw_tile(self, name, csv_file, pos_x, pos_y, is_selected=False):
        """Draw a single block tile with name underneath using cached grid."""
        # Get grid with caching
        grid = self.get_block_grid(name, csv_file)
        
        # Determine grid size dynamically from the loaded grid
        grid_dimension = len(grid)  # This will be 8 for 8x8 or 16 for 16x16
        
        # Calculate positions for the grid within the tile
        grid_size = 48  # Fixed size for the display area
        grid_start_x = pos_x + (self.tile_size - grid_size) // 2
        grid_start_y = pos_y + self.tile_size - 15 - grid_size  # Leave space for name
        
        # Draw the block grid based on actual dimensions
        pixel_size = grid_size / grid_dimension
        for y, row in enumerate(grid):
            for x, color in enumerate(row):
                # Skip drawing pixels that are completely black (transparent)
                r, g, b = color
                if r == 0.0 and g == 0.0 and b == 0.0:
                    continue
                    
                pixel_x = grid_start_x + x * pixel_size
                # Flip the y-coordinate to correct for OpenGL coordinate system
                pixel_y = grid_start_y + (grid_dimension - 1 - y) * pixel_size
                
                glColor3f(*color)
                glBegin(GL_QUADS)
                glVertex2f(pixel_x, pixel_y)
                glVertex2f(pixel_x + pixel_size, pixel_y)
                glVertex2f(pixel_x + pixel_size, pixel_y + pixel_size)
                glVertex2f(pixel_x, pixel_y + pixel_size)
                glEnd()
        
        # Draw grid lines - only draw for 8x8 or 16x16 grids
        if grid_dimension in [8, 16]:
            glColor3f(0.3, 0.3, 0.3)  # Dark gray
            glLineWidth(0.5)
            for i in range(grid_dimension + 1):  # Lines for each cell plus one
                # Vertical lines
                glBegin(GL_LINES)
                glVertex2f(grid_start_x + i * pixel_size, grid_start_y)
                glVertex2f(grid_start_x + i * pixel_size, grid_start_y + grid_size)
                glEnd()
                
                # Horizontal lines
                glBegin(GL_LINES)
                glVertex2f(grid_start_x, grid_start_y + i * pixel_size)
                glVertex2f(grid_start_x + grid_size, grid_start_y + i * pixel_size)
                glEnd()
        
        # Draw selection highlight if selected
        if is_selected:
            glColor3f(1.0, 1.0, 0.0)  # Yellow for selection
            glLineWidth(2.0)
            glBegin(GL_LINE_LOOP)
            glVertex2f(pos_x + 1, pos_y + 1)
            glVertex2f(pos_x + self.tile_size - 2, pos_y + 1)
            glVertex2f(pos_x + self.tile_size - 2, pos_y + self.tile_size - 2)
            glVertex2f(pos_x + 1, pos_y + self.tile_size - 2)
            glEnd()
        
        # Render block name below the grid
        self.render_text(name[:15],  # Truncate long names  
                         pos_x + self.tile_size // 2, 
                         pos_y + 3,
                         alignment="center")

    def render_text(self, text, x, y, alignment="left", color=(1.0, 1.0, 1.0)):
        """Render text using pygame font surfaces transferred to OpenGL textures."""
        # Create font texture if it doesn't exist or if we've changed text
        texture_key = (text, alignment)
        if texture_key not in self.font_texture_cache:
            # Use pygame to render text to a surface
            pygame.font.init()
            font = pygame.font.SysFont(None, 16)  # Default font, 16pt
            
            # Create surface with text
            text_surface = font.render(text, True, (int(color[0]*255), int(color[1]*255), int(color[2]*255)))
            text_data = pygame.image.tostring(text_surface, "RGBA", True)
            
            # Generate OpenGL texture
            texture_id = glGenTextures(1)
            glBindTexture(GL_TEXTURE_2D, texture_id)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, text_surface.get_width(), text_surface.get_height(),
                        0, GL_RGBA, GL_UNSIGNED_BYTE, text_data)
            
            # Store in cache
            self.font_texture_cache[texture_key] = {
                'id': texture_id,
                'width': text_surface.get_width(),
                'height': text_surface.get_height()
            }
        
        # Get cached texture
        tex_info = self.font_texture_cache[texture_key]
        tex_width = tex_info['width']
        tex_height = tex_info['height']
        
        # Calculate position based on alignment
        if alignment == "center":
            x = x - tex_width // 2
        elif alignment == "right":
            x = x - tex_width
        
        # Enable texturing and bind the texture
        glEnable(GL_TEXTURE_2D)
        glBindTexture(GL_TEXTURE_2D, tex_info['id'])
        
        # Draw textured quad for text
        glColor3f(1.0, 1.0, 1.0)  # White for texture
        glBegin(GL_QUADS)
        glTexCoord2f(0, 0); glVertex2f(x, y)
        glTexCoord2f(1, 0); glVertex2f(x + tex_width, y)
        glTexCoord2f(1, 1); glVertex2f(x + tex_width, y + tex_height)
        glTexCoord2f(0, 1); glVertex2f(x, y + tex_height)
        glEnd()
        
        # Disable texturing
        glDisable(GL_TEXTURE_2D)

    def draw_gallery(self):
        """Draw the entire block gallery."""
        # Calculate visible area based on scrolling
        rows_per_screen = math.ceil(self.window_size[1] / (self.tile_size + self.margin))
        
        # Calculate which row index the scroll offset puts us at
        first_visible_row = max(0, int(self.scroll_offset / (self.tile_size + self.margin)))
        
        # Calculate the visible range with extra buffer for smooth scrolling
        last_visible_row = first_visible_row + rows_per_screen + 8  # Increased buffer to ensure more rows are processed
        
        # Calculate the index range that corresponds to these rows
        start_index = first_visible_row * self.tiles_per_row
        end_index = min(len(self.block_csv_paths), (last_visible_row + 1) * self.tiles_per_row)  # Include next row as well
        
        # Iterate only through the block indices that could be visible
        for i in range(start_index, end_index):
            if i >= len(self.block_csv_paths):
                break
                
            name, csv_file = self.block_csv_paths[i]
            row = i // self.tiles_per_row
            col = i % self.tiles_per_row
            
            # Calculate x position normally
            pos_x = self.margin + col * (self.tile_size + self.margin)
            
            # Calculate y position with proper scrolling:
            # In OpenGL with gluOrtho2D(0, width, 0, height), (0,0) is bottom-left corner
            # So for a top-down layout, we start from the top (height) and subtract positions
            pos_y = self.window_size[1] - (row * (self.tile_size + self.margin)) + self.scroll_offset
            
            # Adjust to be the bottom of the tile for drawing
            pos_y -= (self.tile_size + self.margin)
            
            # Only draw if the tile is within visible bounds
            if -self.tile_size <= pos_y <= self.window_size[1]:
                is_selected = self.selected_block is not None and self.selected_block == name
                self.draw_tile(name, csv_file, pos_x, pos_y, is_selected)
        
        
        # Draw scrollbar
        self.draw_scrollbar()

    def draw_scrollbar(self):
        """Draw the vertical scrollbar."""
        scrollbar_width = 15
        scrollbar_x = self.window_size[0] - scrollbar_width - 5
        scrollbar_y = 20
        scrollbar_height = self.window_size[1] - 40
        
        # Background
        glColor3f(0.2, 0.2, 0.2)
        glBegin(GL_QUADS)
        glVertex2f(scrollbar_x, scrollbar_y)
        glVertex2f(scrollbar_x + scrollbar_width, scrollbar_y)
        glVertex2f(scrollbar_x + scrollbar_width, scrollbar_y + scrollbar_height)
        glVertex2f(scrollbar_x, scrollbar_y + scrollbar_height)
        glEnd()
        
        # Calculate scrollbar handle position and size
        if self.max_scroll > 0:
            scrollbar_range = scrollbar_height - 20  # Minimum handle size
            handle_height = max(20, int((self.window_size[1] / (self.max_scroll + self.window_size[1])) * scrollbar_height))
            handle_y_ratio = self.scroll_offset / self.max_scroll if self.max_scroll > 0 else 0
            handle_y = scrollbar_y + int((1 - handle_y_ratio) * (scrollbar_height - handle_height))
            
            # Handle
            glColor3f(0.4, 0.4, 0.4)
            glBegin(GL_QUADS)
            glVertex2f(scrollbar_x + 2, handle_y)
            glVertex2f(scrollbar_x + scrollbar_width - 2, handle_y)
            glVertex2f(scrollbar_x + scrollbar_width - 2, handle_y + handle_height)
            glVertex2f(scrollbar_x + 2, handle_y + handle_height)
            glEnd()
    
    def get_block_at_position(self, mouse_x, mouse_y):
        """Get which block tile is at the given mouse position."""
        # Convert mouse y coordinate from pygame (top-left origin) to OpenGL (bottom-left origin)
        opengl_mouse_y = self.window_size[1] - mouse_y
        
        # Account for scrolling - use same calculation as in draw_gallery method
        for i, (name, csv_file) in enumerate(self.block_csv_paths):
            row = i // self.tiles_per_row
            col = i % self.tiles_per_row
            
            pos_x = self.margin + col * (self.tile_size + self.margin)
            # Calculate with scrolling - use same formula as draw_gallery to ensure consistency
            pos_y = self.window_size[1] - (row * (self.tile_size + self.margin)) + self.scroll_offset
            pos_y -= (self.tile_size + self.margin)
            
            # Check if mouse is over this tile (compare with converted y coordinate)
            if (pos_x <= mouse_x <= pos_x + self.tile_size and 
                pos_y <= opengl_mouse_y <= pos_y + self.tile_size):
                return name
        
        return None

    def handle_scroll(self, delta):
        """Handle scrolling by delta amount."""
        self.scroll_offset = max(0, min(self.max_scroll, self.scroll_offset + delta))
    
    def handle_scrollbar_drag(self, mouse_y):
        """Handle dragging the scrollbar."""
        scrollbar_width = 15
        scrollbar_x = self.window_size[0] - scrollbar_width - 5
        scrollbar_y = 20
        scrollbar_height = self.window_size[1] - 40
        
        if self.max_scroll > 0:
            handle_height = max(20, int((self.window_size[1] / (self.max_scroll + self.window_size[1])) * scrollbar_height))
            scrollbar_track_height = scrollbar_height - handle_height
            
            # Calculate how far along the scrollbar track the mouse is
            relative_y = max(0, min(scrollbar_track_height, mouse_y - (scrollbar_y + handle_height // 2)))
            scrollbar_ratio = relative_y / scrollbar_track_height if scrollbar_track_height > 0 else 0
            
            self.scroll_offset = int(min(self.max_scroll, (1 - scrollbar_ratio) * self.max_scroll))

    def cleanup_cache(self):
        """Clean up memory by removing older cached items if cache is too large."""
        # Clean up grid cache if it gets too large
        if len(self.grid_cache) > 100:
            # Remove half of the oldest items (LRU-style)
            items_to_remove = self.grid_access_order[:len(self.grid_access_order)//2]
            for key in items_to_remove:
                if key in self.grid_cache:
                    del self.grid_cache[key]
            # Update access order
            self.grid_access_order = [key for key in self.grid_access_order if key in self.grid_cache]

    def run(self):
        """Run the gallery viewer loop."""
        self.init_pygame()
        clock = pygame.time.Clock()
        
        print("Minecraft Block Gallery Viewer Controls:")
        print("- Scroll wheel: Scroll up/down")
        print("- Click on block: Select block")
        print("- Arrow keys: Scroll up/down")
        print("- Page Up/Page Down: Scroll faster")
        print("- ESC or Q: Quit")
        print(f"- Showing {self.tiles_per_row} columns of {len(self.block_csv_paths)} blocks")
        print("")
        
        running = True
        while running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key in [pygame.K_ESCAPE, pygame.K_q]:
                        running = False
                    elif event.key == pygame.K_PAGEUP:
                        self.handle_scroll(self.window_size[1] * 0.8)
                    elif event.key == pygame.K_PAGEDOWN:
                        self.handle_scroll(-self.window_size[1] * 0.8)
                    elif event.key in [pygame.K_UP, pygame.K_DOWN]:
                        scroll_amount = 50 if event.key == pygame.K_UP else -50
                        self.handle_scroll(scroll_amount)
                elif event.type == pygame.MOUSEBUTTONDOWN:
                    if event.button == 4:  # Scroll up
                        self.handle_scroll(30)
                    elif event.button == 5:  # Scroll down
                        self.handle_scroll(-30)
                    elif event.button == 1:  # Left click
                        # Check if click is on scrollbar
                        scrollbar_width = 15
                        scrollbar_x = self.window_size[0] - scrollbar_width
                        if event.pos[0] >= scrollbar_x:
                            # Start dragging scrollbar
                            self.dragging_scrollbar = True
                            self.handle_scrollbar_drag(event.pos[1])
                        else:
                            # Check if click is on a block
                            clicked_block = self.get_block_at_position(event.pos[0], event.pos[1])
                            if clicked_block:
                                self.selected_block = clicked_block
                                print(f"Selected block: {clicked_block}")
                                # Open individual viewer for this block
                                self.open_individual_viewer(clicked_block)
                elif event.type == pygame.MOUSEBUTTONUP:
                    if event.button == 1:  # Left button released
                        self.dragging_scrollbar = False
                elif event.type == pygame.MOUSEMOTION:
                    if self.dragging_scrollbar:
                        self.handle_scrollbar_drag(event.pos[1])
            
            # Continuous scrolling with keys
            keys = pygame.key.get_pressed()
            scroll_speed = 5
            if keys[pygame.K_UP] or keys[pygame.K_w]:
                self.handle_scroll(scroll_speed)
            if keys[pygame.K_DOWN] or keys[pygame.K_s]:
                self.handle_scroll(-scroll_speed)
            
            # Periodically clean up cache to manage memory
            if pygame.time.get_ticks() % 5000 < 1000:  # Clean up approximately every 5 seconds
                self.cleanup_cache()
            
            # Clear screen and draw
            glClear(GL_COLOR_BUFFER_BIT)
            self.draw_gallery()
            
            pygame.display.flip()
            clock.tick(60)  # Cap at 60 FPS

        pygame.quit()
    
    def open_individual_viewer(self, block_name):
        """Open the individual block viewer for the selected block."""
        print(f"Opening individual viewer for: {block_name}")
        # In a real implementation, this would instantiate and run the individual viewer
        # For now, we'll just print the instruction
        print(f"To view in detail: python block_opengl_viewer.py --block {block_name}")


def main():
    """Main function."""
    import argparse
    parser = argparse.ArgumentParser(description="Minecraft Block Gallery Viewer - Browse all blocks")
    parser.add_argument("--dir", "-d", default=".", 
                       help="Base directory containing block folders (default: current directory)")
    parser.add_argument("--size", "-s", type=int, nargs=2, default=[1000, 700],
                       help="Window size as WIDTH HEIGHT (default: 1000 700)")
    parser.add_argument("--cols", "-c", type=int, default=8,
                       help="Number of columns to display (default: 8)")
    parser.add_argument("--grid-size", "-g", type=str, choices=['8x8', '16x16'], default=None,
                       help="Grid size to display (8x8 or 16x16), if not specified, will auto-detect")
    
    args = parser.parse_args()
    
    # Check if user wants specific grid size
    base_dir = args.dir
    if args.grid_size:
        # Look for specific size directory in the base directory
        specific_size_dir = f"mc_extracted_csvs_{args.grid_size}"
        if os.path.exists(os.path.join(base_dir, specific_size_dir)):
            base_dir = os.path.join(base_dir, specific_size_dir)
        elif os.path.exists(specific_size_dir):
            base_dir = specific_size_dir
        else:
            print(f"Warning: Could not find directory '{specific_size_dir}', using base directory instead")
    
    if not os.path.exists(base_dir):
        print(f"Directory {base_dir} does not exist!")
        sys.exit(1)
    
    print(f"Loading block gallery from: {base_dir}")
    
    # Create and run the gallery
    try:
        gallery = BlockGallery(base_dir, window_size=(args.size[0], args.size[1]))
        gallery.tiles_per_row = args.cols
        gallery.run()
    except Exception as e:
        print(f"Error running gallery: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()