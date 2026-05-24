import os
import sys

_paths = {}

def init_paths():
    global _paths
    
    if _paths:
        return True
    
    # Try multiple locations
    locations = [
        'pieces/locations/location_kvp',
        '../pieces/locations/location_kvp',
        '../../locations/location_kvp',
        '../../../locations/location_kvp',
    ]
    
    found = False
    for loc_path in locations:
        if os.path.exists(loc_path):
            with open(loc_path, 'r') as f:
                for line in f:
                    if '=' in line:
                        k, v = line.strip().split('=', 1)
                        _paths[k] = v
            found = True
            break
    
    if not found:
        # Emergency fallback: current directory
        _paths['project_root'] = os.getcwd()
        _paths['pieces_dir'] = os.path.join(os.getcwd(), 'pieces')
    
    return True

def get_path(key):
    if not _paths:
        init_paths()
    return _paths.get(key)

def get_project_root():
    return get_path('project_root')

def get_pieces_dir():
    return get_path('pieces_dir')

def build_path(relative_path):
    """Build absolute path from project root"""
    root = get_project_root()
    if not root:
        return None
    return os.path.join(root, relative_path)

def get_piece_path(piece_id, filename=""):
    """TPM-WISE path resolution"""
    # Special cases for macro folders
    if piece_id in ['fuzzpet', 'clock', 'world']:
        base = get_path(f"{piece_id}_dir")
        if base:
            return os.path.join(base, filename) if filename else base
            
    # Default fallback
    pieces = get_pieces_dir()
    if pieces:
        return os.path.join(pieces, piece_id, filename)
    return None
