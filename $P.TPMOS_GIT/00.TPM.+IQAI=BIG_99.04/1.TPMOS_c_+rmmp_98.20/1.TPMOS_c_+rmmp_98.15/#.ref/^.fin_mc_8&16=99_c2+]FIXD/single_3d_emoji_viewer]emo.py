#!/usr/bin/env python3
"""
Simple Single 3D Emoji Viewer
Displays a single 3D emoji with rotation controls
"""

import os
import sys
import csv
import math

try:
    import pygame
    from pygame.locals import *
    from OpenGL.GL import *
    from OpenGL.GLU import *
except ImportError as e:
    print(f"Missing required modules: {e}")
    print("Install with: pip install pygame PyOpenGL PyOpenGL_accelerate")
    sys.exit(1)


class Single3DEmojiViewer:
    def __init__(self, csv_file_path, window_size=(800, 800)):
        self.csv_file_path = csv_file_path
        self.window_size = window_size
        self.points_3d = []
        self.rotation_x = 0
        self.rotation_y = 0
        self.rotation_z = 0
        self.zoom = -5  # Distance from camera
        
        # Load the 3D emoji
        self.load_3d_emoji()
        
    def load_3d_emoji(self):
        """Load 3D emoji data from a CSV file."""
        try:
            with open(self.csv_file_path, 'r', newline='', encoding='utf-8') as csvfile:
                reader = csv.DictReader(csvfile)
                for row in reader:
                    x = float(row['x'])
                    y = float(row['y'])
                    z = float(row['z'])
                    r = int(row['r']) / 255.0
                    g = int(row['g']) / 255.0
                    b = int(row['b']) / 255.0
                    self.points_3d.append((x, y, z, r, g, b))
                    
            print(f"Loaded {len(self.points_3d)} 3D points from {self.csv_file_path}")
        except Exception as e:
            print(f"Error loading {self.csv_file_path}: {e}")
            # Add a few default points if there's an error
            self.points_3d = [(0, 0, 0, 1, 0, 1), (0.5, 0, 0, 0, 1, 0)]

    def draw_cube(self, x, y, z, r, g, b, size=0.05):
        """Draw a single colored cube (voxel) at the given position."""
        # Define the 8 vertices of a cube
        vertices = [
            [x - size, y - size, z - size],
            [x + size, y - size, z - size],
            [x + size, y + size, z - size],
            [x - size, y + size, z - size],
            [x - size, y - size, z + size],
            [x + size, y - size, z + size],
            [x + size, y + size, z + size],
            [x - size, y + size, z + size]
        ]
        
        # Define the 6 faces of a cube
        faces = [
            [0, 1, 2, 3],  # Front face
            [5, 4, 7, 6],  # Back face
            [4, 0, 3, 7],  # Left face
            [1, 5, 6, 2],  # Right face
            [3, 2, 6, 7],  # Top face
            [4, 5, 1, 0]   # Bottom face
        ]
        
        glColor3f(r, g, b)
        for face in faces:
            glBegin(GL_QUADS)
            for vertex in face:
                glVertex3fv(vertices[vertex])
            glEnd()

    def draw_3d_emoji(self):
        """Draw the 3D emoji with current rotation."""
        # Push a transformation matrix
        glPushMatrix()
        
        # Apply rotations
        glRotatef(self.rotation_x, 1, 0, 0)
        glRotatef(self.rotation_y, 0, 1, 0)
        glRotatef(self.rotation_z, 0, 0, 1)
        
        # Draw each voxel as a cube
        for x, y, z, r, g, b in self.points_3d:
            self.draw_cube(x, y, z, r, g, b)
        
        glPopMatrix()

    def init_pygame(self):
        """Initialize Pygame and OpenGL."""
        pygame.init()
        self.screen = pygame.display.set_mode(
            self.window_size, 
            DOUBLEBUF | OPENGL
        )
        pygame.display.set_caption(f"3D Emoji Viewer - {os.path.basename(self.csv_file_path)}")
        
        # Set up OpenGL
        glClearColor(0.1, 0.1, 0.2, 1.0)  # Dark blue-gray background
        glEnable(GL_DEPTH_TEST)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
        
        # Set up perspective
        glMatrixMode(GL_PROJECTION)
        gluPerspective(45, (self.window_size[0] / self.window_size[1]), 0.1, 50.0)
        
        glMatrixMode(GL_MODELVIEW)
        glTranslatef(0.0, 0.0, self.zoom)

    def run(self):
        """Run the viewer loop."""
        self.init_pygame()
        clock = pygame.time.Clock()
        
        print("Single 3D Emoji Viewer Controls:")
        print("- Mouse drag: Rotate the 3D emoji")
        print("- Scroll wheel: Zoom in/out")
        print("- Arrow keys: Rotate the 3D emoji")
        print("- WASD keys: Rotate the 3D emoji")
        print("- Q/E keys: Rotate around Z axis")
        print("- R: Reset rotation")
        print("- ESC: Quit")
        print("")
        
        old_mouse_pos = None
        rotation_speed = 2.0
        zoom_speed = 0.5
        
        running = True
        while running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE:
                        running = False
                    elif event.key == pygame.K_q:  # Rotate counter-clockwise around Z
                        self.rotation_z -= rotation_speed * 2
                    elif event.key == pygame.K_e:  # Rotate clockwise around Z
                        self.rotation_z += rotation_speed * 2
                    elif event.key == pygame.K_w:  # Rotate around X
                        self.rotation_x += rotation_speed
                    elif event.key == pygame.K_s:  # Rotate around X (opposite)
                        self.rotation_x -= rotation_speed
                    elif event.key == pygame.K_a:  # Rotate around Y
                        self.rotation_y -= rotation_speed
                    elif event.key == pygame.K_d:  # Rotate around Y (opposite)
                        self.rotation_y += rotation_speed
                    elif event.key == pygame.K_r:  # Reset rotation
                        self.rotation_x = 0
                        self.rotation_y = 0
                        self.rotation_z = 0
                elif event.type == pygame.MOUSEBUTTONDOWN:
                    if event.button == 4:  # Scroll up - zoom in
                        glTranslatef(0, 0, zoom_speed)
                        self.zoom += zoom_speed
                    elif event.button == 5:  # Scroll down - zoom out
                        glTranslatef(0, 0, -zoom_speed)
                        self.zoom -= zoom_speed
                    elif event.button == 1:  # Left click - start dragging
                        old_mouse_pos = pygame.mouse.get_pos()
                elif event.type == pygame.MOUSEBUTTONUP:
                    if event.button == 1:  # Left button released
                        old_mouse_pos = None
                elif event.type == pygame.MOUSEMOTION:
                    if event.buttons[0]:  # Left mouse button is held down
                        if old_mouse_pos is not None:
                            dx = event.pos[0] - old_mouse_pos[0]
                            dy = event.pos[1] - old_mouse_pos[1]
                            
                            # Apply rotation based on mouse movement
                            self.rotation_y += dx * 0.5
                            self.rotation_x += dy * 0.5
                        old_mouse_pos = event.pos
            
            # Continuous rotation with arrow keys
            keys = pygame.key.get_pressed()
            if keys[pygame.K_UP]:
                self.rotation_x += rotation_speed
            if keys[pygame.K_DOWN]:
                self.rotation_x -= rotation_speed
            if keys[pygame.K_LEFT]:
                self.rotation_y -= rotation_speed
            if keys[pygame.K_RIGHT]:
                self.rotation_y += rotation_speed
            
            # Clear screen and draw
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
            self.draw_3d_emoji()
            
            pygame.display.flip()
            clock.tick(60)  # Cap at 60 FPS

        pygame.quit()


def main():
    """Main function."""
    import argparse
    parser = argparse.ArgumentParser(description="Single 3D Emoji Viewer - View and rotate a single 3D emoji")
    parser.add_argument("--file", "-f", 
                       help="CSV file containing 3D emoji data (default: first emoji in 3d_output)")
    parser.add_argument("--size", "-s", type=int, nargs=2, default=[800, 800],
                       help="Window size as WIDTH HEIGHT (default: 800 800)")
    
    args = parser.parse_args()
    
    # If no file specified, look for default 3D emoji
    if not args.file:
        # Look for the first available emoji file
        default_file = "3d_output/emoji_0/emoji_0.csv"
        if os.path.exists(default_file):
            args.file = default_file
            print(f"Using default file: {default_file}")
        else:
            # Look for any available emoji file
            import glob
            emoji_files = glob.glob("3d_output/emoji_*/emoji_*.csv")
            if emoji_files:
                args.file = sorted(emoji_files)[0]
                print(f"Using first available file: {args.file}")
            else:
                print("No 3D emoji files found! Please generate them first.")
                sys.exit(1)
    
    if not os.path.exists(args.file):
        print(f"File {args.file} does not exist!")
        sys.exit(1)
    
    # Create and run the viewer
    viewer = Single3DEmojiViewer(args.file, window_size=(args.size[0], args.size[1]))
    viewer.run()


if __name__ == "__main__":
    main()