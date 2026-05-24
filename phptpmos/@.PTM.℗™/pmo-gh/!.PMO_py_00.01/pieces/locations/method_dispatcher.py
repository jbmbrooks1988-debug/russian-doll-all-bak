#!/usr/bin/env python3
"""
Method Dispatcher - Routes method calls to executables based on PDLO contracts
"""

import os
import subprocess
from pdlo_parser import parse_pdl_file, get_method_executable


class MethodDispatcher:
    """Dispatches method calls to executables based on PDLO routing information."""
    
    def __init__(self, pieces_dir='pieces'):
        self.pieces_dir = pieces_dir
        self.pdl_cache = {}  # Cache parsed PDLO files
    
    def _get_pdl_path(self, piece_id):
        """Get path to piece's PDLO file."""
        return os.path.join(self.pieces_dir, piece_id, f"{piece_id}.pdl")
    
    def _load_pdl(self, piece_id):
        """Load and cache PDLO file."""
        if piece_id not in self.pdl_cache:
            pdl_path = self._get_pdl_path(piece_id)
            self.pdl_cache[piece_id] = parse_pdl_file(pdl_path)
        return self.pdl_cache[piece_id]
    
    def call(self, piece_id, method_name, *args, preferred_langs=None):
        """
        Call a method on a piece, routing to correct executable.
        
        Args:
            piece_id: The piece to call the method on
            method_name: The method to call
            *args: Arguments to pass to the executable
            preferred_langs: List of preferred languages in order (default: ['python', 'c', 'js'])
        
        Returns:
            str: stdout from executable, or None if failed
        
        Raises:
            PieceNotFound: If piece PDLO file doesn't exist
            MethodNotFound: If method not found in PDLO
            NoExecutableFound: If no executable found for method
            ExecutableFailed: If executable returns non-zero exit code
        """
        # Load PDLO
        pdl_data = self._load_pdl(piece_id)
        if not pdl_data:
            raise PieceNotFound(f"Piece '{piece_id}' not found")
        
        # Find method
        methods = pdl_data.get('methods', {})
        if method_name not in methods:
            raise MethodNotFound(f"Method '{method_name}' not found in piece '{piece_id}'")
        
        method = methods[method_name]
        executables = method.get('executables', {})
        
        if not executables:
            raise NoExecutableFound(f"No executables defined for {piece_id}.{method_name}")
        
        # Get best executable
        lang, path = get_method_executable(method, preferred_langs)
        if not lang:
            raise NoExecutableFound(f"No suitable executable for {piece_id}.{method_name}")
        
        # Build full path
        full_path = os.path.join(self.pieces_dir, piece_id, path)
        
        # Execute
        try:
            if lang == 'python':
                cmd = ['python3', full_path] + list(args)
            elif lang == 'c':
                cmd = [full_path] + list(args)
            elif lang == 'js':
                cmd = ['node', full_path] + list(args)
            else:
                raise ExecutableFailed(f"Unknown language: {lang}")
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            if result.returncode != 0:
                raise ExecutableFailed(f"Executable failed with code {result.returncode}: {result.stderr}")
            
            return result.stdout.strip()
        
        except FileNotFoundError:
            raise ExecutableFailed(f"Executable not found: {full_path}")
        except subprocess.TimeoutExpired:
            raise ExecutableFailed(f"Executable timed out: {full_path}")


class PieceNotFound(Exception):
    pass


class MethodNotFound(Exception):
    pass


class NoExecutableFound(Exception):
    pass


class ExecutableFailed(Exception):
    pass


# Test
if __name__ == '__main__':
    import sys
    
    dispatcher = MethodDispatcher('pieces')
    
    if len(sys.argv) < 3:
        print("Usage: method_dispatcher.py <piece_id> <method_name> [args...]")
        sys.exit(1)
    
    piece_id = sys.argv[1]
    method_name = sys.argv[2]
    args = sys.argv[3:]
    
    try:
        result = dispatcher.call(piece_id, method_name, *args)
        print(f"Result: {result}")
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
