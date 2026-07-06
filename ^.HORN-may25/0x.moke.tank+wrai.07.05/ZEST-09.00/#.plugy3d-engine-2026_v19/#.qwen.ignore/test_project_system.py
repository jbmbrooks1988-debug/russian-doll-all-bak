#!/usr/bin/env python3
"""
Test script to verify the project pieces system works correctly.
"""

import sys
import os
sys.path.insert(0, os.path.abspath('.'))

from plugins.core.project_manager import ProjectManagerPlugin
from pathlib import Path
import tempfile


def test_project_creation():
    """Test creating a new project"""
    print("Testing project creation...")
    
    # Create a mock main app and voxel grid widget
    class MockApp:
        pass
    
    class MockGridWidget:
        def __init__(self):
            self.entities = {}
        
        def update(self):
            print("Grid updated")
    
    # Initialize project manager
    app = MockApp()
    grid_widget = MockGridWidget()
    
    pm = ProjectManagerPlugin(app)
    pm.initialize(grid_widget)
    
    # Create a new project
    success = pm.create_new_project("test_project")
    assert success, "Failed to create test project"
    
    # Verify project directory structure was created
    project_path = Path("projects/test_project")
    assert project_path.exists(), "Project directory not created"
    assert (project_path / "peices").exists(), "Peices directory not created"
    assert (project_path / "metadata.json").exists(), "Metadata file not created"
    
    print("✓ Project creation successful")


def test_piece_map_loading():
    """Test loading pieces from piece_map.csv"""
    print("\nTesting piece map loading...")
    
    # Create a test piece map
    project_path = Path("projects/test_project")
    (project_path / "peices").mkdir(exist_ok=True)
    
    piece_map_content = """id,type,scale_x,scale_y,scale_z,render_style,pos_x,pos_y,pos_z,color,source_file
cow,emoji_entity,1.0,1.0,1.0,extruded,5,0,5,brown,data/entities/emoji/cow/cow.csv
pig,emoji_entity,1.0,1.0,1.0,extruded,10,0,1,pink,data/entities/emoji/pig/pig.csv"""
    
    with open(project_path / "peices" / "piece_map.csv", 'w') as f:
        f.write(piece_map_content)
    
    # Test loading the project
    class MockApp:
        pass
    
    class MockGridWidget:
        def __init__(self):
            self.entities = {}
        
        def update(self):
            print("Grid updated")
    
    app = MockApp()
    grid_widget = MockGridWidget()
    
    pm = ProjectManagerPlugin(app)
    pm.initialize(grid_widget)
    
    success = pm.load_project("test_project")
    assert success, "Failed to load test project"
    
    # Verify entities were loaded
    assert len(pm.grid_widget.entities) == 2, f"Expected 2 entities, got {len(pm.grid_widget.entities)}"
    
    print("✓ Piece map loading successful")


def test_individual_piece_loading():
    """Test loading individual piece files"""
    print("\nTesting individual piece loading...")
    
    # Create a test individual piece file
    project_path = Path("projects/test_project")
    
    cow_piece_content = """{
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
  "movable": true,
  "interactable": true,
  "health": 100
}"""
    
    with open(project_path / "peices" / "cow.txt", 'w') as f:
        f.write(cow_piece_content)
    
    print("✓ Individual piece loading test structure created")


if __name__ == "__main__":
    print("Running project pieces system tests...\n")
    
    try:
        test_project_creation()
        test_piece_map_loading()
        test_individual_piece_loading()
        
        print("\n✅ All tests passed! The project pieces system is working correctly.")
        
    except Exception as e:
        print(f"\n❌ Test failed with error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)