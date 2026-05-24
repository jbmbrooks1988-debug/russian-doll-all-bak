#!/usr/bin/env python3
"""
PDLO Parser - Parses Piece Definition Language files with executable routing
"""

import os


def parse_pdl_file(pdl_path):
    """
    Parse a PDLO file and return structured data.
    
    Returns:
        dict: {
            'meta': {piece_id, version, determinism},
            'state': {key: value, ...},
            'methods': {name: {return_type, executables: {lang: path}}, ...},
            'event_in': [event_names],
            'event_out': {event: type, ...},
            'responses': {event: message, ...}
        }
    """
    result = {
        'meta': {},
        'state': {},
        'methods': {},
        'event_in': [],
        'event_out': {},
        'responses': {}
    }
    
    if not os.path.exists(pdl_path):
        return None
    
    current_section = None
    
    with open(pdl_path, 'r') as f:
        for line in f:
            line = line.strip()
            
            # Skip empty lines, comments, and headers
            if not line or line.startswith('#') or line.startswith('-'):
                continue
            
            # Skip SECTION header line
            if line.startswith('SECTION'):
                continue
            
            # Parse: SECTION | KEY | VALUE
            parts = line.split('|')
            if len(parts) < 2:
                continue
            
            section = parts[0].strip()
            key = parts[1].strip() if len(parts) > 1 else ''
            value = parts[2].strip() if len(parts) > 2 else ''
            
            # Handle section changes
            if section != current_section:
                current_section = section
            
            # Parse based on section
            if section == 'META':
                result['meta'][key] = value
            
            elif section == 'STATE':
                result['state'][key] = value
            
            elif section == 'METHOD':
                # Parse: METHOD | name | return_type | lang:path | lang:path
                return_type = value
                executables = {}
                
                # Parse language:path pairs
                for part in parts[3:]:
                    part = part.strip()
                    if ':' in part:
                        lang, path = part.split(':', 1)
                        executables[lang.strip()] = path.strip()
                
                result['methods'][key] = {
                    'return_type': return_type,
                    'executables': executables
                }
            
            elif section == 'EVENT_IN':
                result['event_in'].append(key)
            
            elif section == 'EVENT_OUT':
                result['event_out'][key] = value
            
            elif section == 'RESPONSE':
                result['responses'][key] = value
    
    return result


def parse_method_line(line):
    """
    Parse a single METHOD line.
    Format: METHOD | name | return_type | lang:path | lang:path
    
    Returns:
        dict: {'name': str, 'return_type': str, 'executables': {lang: path}} or None
    """
    parts = line.split('|')
    if len(parts) < 3 or parts[0].strip() != 'METHOD':
        return None
    
    method_name = parts[1].strip()
    return_type = parts[2].strip()
    
    # Parse language:path pairs
    executables = {}
    for part in parts[3:]:
        part = part.strip()
        if ':' in part:
            lang, path = part.split(':', 1)
            executables[lang.strip()] = path.strip()
    
    return {
        'name': method_name,
        'return_type': return_type,
        'executables': executables
    }


def get_method_executable(method_data, preferred_langs=None):
    """
    Get the best executable path for a method.
    
    Args:
        method_data: dict from parse_pdl_file()['methods'][method_name]
        preferred_langs: List of preferred languages in order (default: ['python', 'c', 'js'])
    
    Returns:
        tuple: (lang, path) or (None, None) if no executable found
    """
    if preferred_langs is None:
        preferred_langs = ['python', 'c', 'js']
    
    executables = method_data.get('executables', {})
    
    for lang in preferred_langs:
        if lang in executables:
            return lang, executables[lang]
    
    # Return first available if preferred not found
    for lang, path in executables.items():
        return lang, path
    
    return None, None


# Test
if __name__ == '__main__':
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: pdlo_parser.py <pdl_file>")
        sys.exit(1)
    
    pdl_file = sys.argv[1]
    result = parse_pdl_file(pdl_file)
    
    if result:
        print(f"Piece: {result['meta'].get('piece_id', 'unknown')}")
        print(f"Version: {result['meta'].get('version', 'unknown')}")
        print(f"\nState ({len(result['state'])} keys):")
        for key, value in result['state'].items():
            print(f"  {key}: {value}")
        
        print(f"\nMethods ({len(result['methods'])}):")
        for name, data in result['methods'].items():
            execs = ', '.join(f"{lang}:{path}" for lang, path in data['executables'].items())
            print(f"  {name} ({data['return_type']}) -> {execs}")
        
        print(f"\nEvent Listeners: {result['event_in']}")
        print(f"Event Emitters: {list(result['event_out'].keys())}")
    else:
        print(f"Failed to parse {pdl_file}")
