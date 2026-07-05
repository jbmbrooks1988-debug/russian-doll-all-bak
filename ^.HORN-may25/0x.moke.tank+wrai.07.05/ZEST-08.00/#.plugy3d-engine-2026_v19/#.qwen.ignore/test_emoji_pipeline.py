#!/usr/bin/env python3
"""
Test script to verify the new emoji entity manager works with the custom atlas approach.
"""

import sys
import os

# Add the plugins directory to the Python path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'plugins/entities'))

from emoji_entity_manager import EmojiEntityManagerPlugin
from pathlib import Path

# Simulate a main app instance
class MockApp:
    pass

# Simulate a voxel grid widget
class MockGridWidget:
    def __init__(self):
        self.entities = {}
        self.terrain_blocks = {}

def test_emoji_manager():
    print("Testing EmojiEntityManager with custom atlas approach...")
    
    # Create mock app and grid
    mock_app = MockApp()
    mock_grid = MockGridWidget()
    
    # Create the emoji manager
    emoji_manager = EmojiEntityManagerPlugin(mock_app)
    
    # Override session emojis to include pig that we want to test
    emoji_manager.session_emojis = ['cow', 'pig']  # Include both cow and pig
    
    # Test the generation of custom atlas and emoji CSVs
    print("Generating custom emoji atlas and CSV files...")
    emoji_manager.initialize(mock_grid)
    
    print("Test completed successfully!")

if __name__ == "__main__":
    test_emoji_manager()