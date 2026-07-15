#!/usr/bin/env python3
"""
Parse emoji-test_14.0=BEST.txt and extract emoji data into a simpler format.
"""

import os
import re
import sys

def parse_emoji_file(input_file, output_file, max_emojis=None):
    """
    Parse emoji test file and extract fully qualified emojis to a simpler format.
    
    Args:
        input_file: Path to the emoji-test.txt file
        output_file: Path to the output file for simplified emoji data
        max_emojis: Maximum number of emojis to extract (None for no limit)
    """
    emoji_data = []
    
    with open(input_file, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            
            # Skip empty lines and comments
            if not line or line.startswith('#'):
                continue
            
            # Match lines with the format: codepoints ; status # emoji name
            # Example: 1F600                          ; fully-qualified     # 😀 grinning face
            match = re.match(r'^([0-9A-F]+(?: [0-9A-F]+)*)\s*;\s*(fully-qualified|component|minimally-qualified|unqualified)\s*#\s*(.*)$', line)
            if match:
                codepoints_str = match.group(1).strip()
                status = match.group(2).strip()
                remainder = match.group(3).strip()
                
                # Only process fully-qualified emojis for now
                if status in ('fully-qualified', 'component'):
                    # Extract the emoji character and name from the remainder
                    # Format is usually: emoji_character Eversion name
                    parts = remainder.split(' ', 2)  # Split into at most 3 parts
                    
                    if len(parts) >= 2:
                        emoji_char = parts[0]
                        emoji_name = parts[-1] if len(parts) == 3 else ''
                        
                        # Convert space-separated hex codepoints to actual emoji string
                        codepoints = codepoints_str.split()
                        emoji_str = ''.join([chr(int(cp, 16)) for cp in codepoints])
                        
                        # Use the actual emoji character if available, otherwise use the reconstructed one
                        if emoji_char and len(emoji_char) > 0 and not emoji_char.startswith('E'):
                            emoji_str = emoji_char
                            
                        emoji_data.append({
                            'codepoints': codepoints_str.replace(' ', ''),
                            'emoji_str': emoji_str,
                            'emoji_name': emoji_name
                        })
                        
                        if max_emojis and len(emoji_data) >= max_emojis:
                            break
    
    # Write to output file in a format that's easy to parse with C
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(f"# Extracted {len(emoji_data)} fully qualified emojis\n")
        f.write("# Format: codepoint_hex_string|emoji_string|emoji_name\n")
        
        for data in emoji_data:
            f.write(f"{data['codepoints']}|{data['emoji_str']}|{data['emoji_name']}\n")
    
    print(f"Successfully extracted {len(emoji_data)} emojis to {output_file}")

def main():
    # Check if input file exists - use relative paths from the script location
    import os
    script_dir = os.path.dirname(os.path.abspath(__file__))
    input_file = os.path.join(script_dir, "emoji-test_14.0=BEST.txt")
    output_file = os.path.join(script_dir, "parsed_emojis.txt")
    
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found!")
        sys.exit(1)
    
    # Extract ALL emojis (no limit)
    parse_emoji_file(input_file, output_file, max_emojis=None)
    print(f"Processing complete! Output saved to {output_file}")

if __name__ == "__main__":
    main()