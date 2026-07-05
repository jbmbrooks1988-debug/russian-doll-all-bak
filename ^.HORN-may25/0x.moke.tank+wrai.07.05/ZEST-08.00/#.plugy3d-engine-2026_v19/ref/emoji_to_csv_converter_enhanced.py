import os
import csv
import unicodedata
from PIL import Image, ImageDraw, ImageFont
import math

def get_emoji_image(emoji, size=64):
    """
    Create an image representation of an emoji.
    This is a simplified approach - in practice you'd render the actual emoji using
    platform-specific emoji rendering.
    """
    # Create a blank image with white background
    img = Image.new('RGB', (size, size), color=(255, 255, 255))
    draw = ImageDraw.Draw(img)
    
    # Get emoji name to determine appropriate color/pattern
    try:
        char = emoji[0]  # Take the first character
        name = unicodedata.name(char, '').lower()
    except:
        name = ""
    
    # Create a basic representation based on emoji type with more varied colors
    if any(fruit in name for fruit in ['apple', 'cherry', 'strawberry']):
        # Red round fruits with gradient details
        for y in range(size):
            for x in range(size):
                dist_to_center = ((x - size//2)**2 + (y - size//2)**2)**0.5
                if dist_to_center <= size//2 * 0.8:  # Within fruit
                    # Create a red gradient with slight variation
                    base_red = 255
                    var = int(20 * (math.sin(x * 0.3) + math.cos(y * 0.3)))  # Add some texture
                    r = max(150, min(255, base_red + var))
                    g = max(0, min(100, var))  # Less green
                    b = max(0, min(100, var))  # Less blue
                    draw.point((x, y), fill=(int(r), int(g), int(b)))
    elif any(fruit in name for fruit in ['lemon', 'banana']):
        # Yellow fruits with gradient details
        for y in range(size):
            for x in range(size):
                dist_to_center = ((x - size//2)**2 + (y - size//2)**2)**0.5
                if dist_to_center <= size//2 * 0.8:  # Within fruit
                    base_yellow = 255
                    var = int(30 * (math.sin(x * 0.2) + math.cos(y * 0.2)))  # Texture
                    r = min(255, base_yellow + var // 2)
                    g = min(255, base_yellow + var)
                    b = max(0, base_yellow - var * 2)
                    draw.point((x, y), fill=(int(r), int(g), int(b)))
    elif any(fruit in name for fruit in ['tangerine', 'orange']):
        # Orange fruits with gradient details
        for y in range(size):
            for x in range(size):
                dist_to_center = ((x - size//2)**2 + (y - size//2)**2)**0.5
                if dist_to_center <= size//2 * 0.8:  # Within fruit
                    base_orange = 200
                    var = int(30 * (math.sin(x * 0.25) + math.cos(y * 0.25)))  # Texture
                    r = min(255, base_orange + var)
                    g = max(100, base_orange - int(var * 1.5))
                    b = max(0, base_orange - int(var * 2))
                    draw.point((x, y), fill=(int(r), int(g), int(b)))
    elif any(food in name for food in ['carrot']):
        # Carrot with elongated shape and gradient
        for y in range(size):
            for x in range(size):
                # Ellipse rotated vertically for carrot shape
                ellipse_factor = ((x - size//2)**2 / (size//3)**2 + (y - size//2)**2 / (size//1.8)**2)
                if ellipse_factor <= 1:
                    base_orange = 255
                    var = int(30 * (math.sin(x * 0.3) + math.cos(y * 0.2)))  # Texture
                    r = min(255, base_orange + var)
                    g = max(100, 165 + var)
                    b = max(0, var)
                    draw.point((x, y), fill=(int(r), int(g), int(b)))
    elif any(fruit in name for fruit in ['grape']):
        # Cluster of grapes (smaller circles in a larger area)
        grape_size = size // 8
        for j in range(3):
            for i in range(4):
                cx = size//2 - 1.2*grape_size + i*grape_size*0.8
                cy = size//2 - grape_size + j*grape_size*0.9
                for y in range(max(0, int(cy-grape_size)), min(size, int(cy+grape_size))):
                    for x in range(max(0, int(cx-grape_size)), min(size, int(cx+grape_size))):
                        if (x - int(cx))**2 + (y - int(cy))**2 <= int(grape_size)**2:
                            draw.point((x, y), fill=(150, 100, 200))  # Purple grapes
    elif any(fruit in name for fruit in ['kiwi']):
        # Kiwi cross-section with fuzzy brown exterior and green interior with seeds
        for y in range(size):
            for x in range(size):
                dist_to_center = ((x - size//2)**2 + (y - size//2)**2)**0.5
                if dist_to_center <= size//2 * 0.8:  # Within kiwi
                    if dist_to_center < size//2 * 0.6:  # Interior
                        # Green with black seed dots
                        if (abs(x - size//2) < 3 and abs(y - size//2 - 15) < 3) or \
                           (abs(x - size//2 - 10) < 3 and abs(y - size//2 - 10) < 3) or \
                           (abs(x - size//2 + 8) < 3 and abs(y - size//2 + 5) < 3):
                            draw.point((x, y), fill=(0, 0, 0))  # Seed
                        else:
                            draw.point((x, y), fill=(140, 200, 80))  # Green
                    else:  # Exterior (brown skin)
                        draw.point((x, y), fill=(160, 120, 60))
    elif any(food in name for food in ['egg']):
        # Egg shape with slight shading
        for y in range(size):
            for x in range(size):
                # Egg shape is taller than it is wide
                ellipse_factor = ((x - size//2)**2 / (size//3)**2 + (y - size//2)**2 / (size//2.2)**2)
                if ellipse_factor <= 1:
                    base_egg = 250
                    var = int(20 * (math.sin(x * 0.2) + math.cos(y * 0.25)))  # Subtle texture
                    r = min(255, base_egg + var)
                    g = min(255, base_egg + var)
                    b = max(200, base_egg - var)
                    draw.point((x, y), fill=(int(r), int(g), int(b)))
    elif any(food in name for food in ['bread', 'croissant', 'bagel']):
        # Bread/baked goods with grainy texture
        for y in range(size):
            for x in range(size):
                dist_to_center = ((x - size//2)**2 + (y - size//2)**2)**0.5
                if dist_to_center <= size//2 * 0.8:  # Within bread
                    base_brown = 238
                    var = int(30 * (math.sin(x * 0.5) + math.cos(y * 0.5)))  # Grainy texture
                    r = min(255, base_brown - var // 2)
                    g = max(150, base_brown - var)
                    b = max(100, base_brown - var * 2)
                    draw.point((x, y), fill=(int(r), int(g), int(b)))
    elif any(object_name in name for object_name in ['gem', 'diamond']):
        # Gem with faceted appearance
        # Create diamond-like shape with multiple colored facets
        center_x, center_y = size//2, size//2
        diamond_points = [(center_x, 5), (size-5, center_y), (center_x, size-5), (5, center_y)]
        
        # Draw the diamond with different colored facets
        draw.polygon(diamond_points, fill=(100, 200, 220))  # Base color
        
        # Add highlights
        top_half = [(center_x, 5), (size-5, center_y), (center_x, center_y)]
        draw.polygon(top_half, fill=(180, 230, 255))  # Highlight
        
        # Add another facet
        left_facet = [(5, center_y), (center_x, center_y), (center_x, size-5)]
        draw.polygon(left_facet, fill=(80, 180, 200))
    elif any(name_part in name for name_part in ['poop', 'poo']):
        # Brown pile with spiral pattern
        for y in range(size):
            for x in range(size):
                dist_to_center = ((x - size//2)**2 + (y - size//2)**2)**0.5
                if dist_to_center <= size//2 * 0.7:  # Within pile
                    angle = math.atan2(y - size//2, x - size//2)
                    spiral = math.sqrt(dist_to_center) * math.sin(angle * 4 + dist_to_center * 0.2)
                    
                    # Brown with variation
                    base_brown = 139
                    var = int(30 * spiral / 5)
                    r = max(101, min(200, base_brown + var))
                    g = max(67, min(139, base_brown - var))
                    b = max(33, min(90, base_brown - int(var * 2)))
                    draw.point((x, y), fill=(int(r), int(g), int(b)))
    elif any(name_part in name for name_part in ['gem_stone']):
        # Gemstone with crystalline appearance
        center_x, center_y = size//2, size//2
        for y in range(size):
            for x in range(size):
                dist_to_center = ((x - center_x)**2 + (y - center_y)**2)**0.5
                if dist_to_center <= size//2 * 0.8:
                    # Crystal pattern with refractions
                    dx = x - center_x
                    dy = y - center_y
                    refraction = math.sin(dx * 0.3) * math.cos(dy * 0.3) * 50
                    
                    base_teal = 100
                    r = max(0, min(255, base_teal + refraction))
                    g = max(100, min(255, base_teal + refraction + 50))
                    b = max(100, min(255, base_teal + refraction + 50))
                    
                    draw.point((x, y), fill=(int(r), int(g), int(b)))
    elif any(name_part in name for name_part in ['bell']):
        # Bell with yellow color and metallic effect
        center_x, center_y = size // 2, size // 2
        for y in range(size):
            for x in range(size):
                dx, dy = x - center_x, y - center_y
                dist = math.sqrt(dx*dx + dy*dy)
                
                # Bell shape (like a rounded triangle)
                angle = math.atan2(dy, dx)
                bell_width_factor = math.cos(angle) * 0.8  # More narrow sideways
                bell_height_factor = math.sin(angle) * 0.9
                
                if abs(dx) < bell_width_factor * size//3 and abs(dy) < bell_height_factor * size//2:
                    # Metallic gold with highlights
                    base_gold = 200
                    highlight = int(55 * math.sin(dx * 0.3) * math.cos(dy * 0.2))
                    r = min(255, base_gold + highlight)
                    g = min(255, base_gold + highlight + 40)
                    b = min(255, max(0, base_gold - 50 + highlight))
                    draw.point((x, y), fill=(int(r), int(g), int(b)))
                elif dist < size//2 * 0.9:  # Outside bell but within circle
                    draw.point((x, y), fill=(255, 255, 255))  # Background
    elif any(name_part in name for name_part in ['mushroom']):
        # Mushroom with red cap and white stem
        cap_height = size // 2
        stem_height = size // 3
        cap_radius = size // 2.5
        
        for y in range(size):
            for x in range(size):
                dx, dy = x - size//2, y - size//2
                
                # Red cap area (rounded top)
                cap_dist = math.sqrt(dx**2 + (dy * 1.5)**2)  # Elliptical cap
                if cap_dist <= cap_radius and y <= size//2 - stem_height//2:
                    # Red cap with spots
                    base_red = 230
                    var = int(15 * (math.sin(dx * 0.4) + math.cos(dy * 0.4)))
                    r = min(255, base_red + var)
                    g = max(0, 50 + var)
                    b = max(0, 50 + var)
                    
                    # Add white spots
                    if (abs(dx) < 8 and abs(dy + 5) < 8) or (abs(dx - 10) < 5 and abs(dy - 5) < 5):
                        r, g, b = 255, 255, 255
                    draw.point((x, y), fill=(r, g, b))
                # Stem area (white)
                elif abs(dx) < size//6 and y > size//2 - stem_height//2:
                    draw.point((x, y), fill=(250, 250, 240))
                elif math.sqrt(dx**2 + dy**2) < size//2 * 0.9:  # Within circle boundary
                    draw.point((x, y), fill=(255, 255, 255))  # Background
    elif any(name_part in name for name_part in ['heart']):
        # Heart shape
        for y in range(size):
            for x in range(size):
                # Heart equation: transformed circle
                # (x^2 + y^2 - 1)^3 - x^2*y^3 <= 0
                # Adjusted for our coordinate system
                nx = (x - size//2) / (size//3)
                ny = (size - y - size//2) / (size//3)  # Flip y-axis
                
                # Heart equation with scaling and adjustment
                if (nx**2 + ny**2 - 0.8)**3 - nx**2 * ny**3 <= 0.02:
                    # Red with gradient
                    base_red = 255
                    var = int(20 * (math.sin(x * 0.3) + math.cos(y * 0.2)))
                    r = min(255, base_red + var//2)
                    g = max(100, 150 - var)
                    b = max(100, 130 - var)
                    draw.point((x, y), fill=(int(r), int(g), int(b)))
                elif math.sqrt((x - size//2)**2 + (y - size//2)**2) < size//2 * 0.9:  # Within circle
                    draw.point((x, y), fill=(255, 255, 255))  # Background
    else:
        # Default: a colored square based on a hash of the emoji with texture
        emoji_hash = hash(emoji) % (256**3)
        base_r = (emoji_hash >> 16) % 256
        base_g = (emoji_hash >> 8) % 256
        base_b = emoji_hash % 256
        
        # Create a more varied pattern to preserve detail during downsampling
        for y in range(size):
            for x in range(size):
                var = int(40 * (math.sin(x * 0.8) * math.cos(y * 0.8) + math.sin(y * 0.6) * math.cos(x * 0.6)))  # More complex texture
                r = max(0, min(255, base_r + var))
                g = max(0, min(255, base_g + var))
                b = max(0, min(255, base_b + var))
                draw.point((x, y), fill=(int(r), int(g), int(b)))
    
    return img

def downscale_to_8x8(emoji_img):
    """
    Downscale the emoji image to 8x8 pixels with dithering to preserve color variety
    """
    # Use NEAREST resampling to preserve more color variety instead of averaging
    # Use LANCZOS if available, otherwise use NEAREST (older PIL versions)
    try:
        downscaled = emoji_img.resize((8, 8), Image.Resampling.NEAREST)
    except AttributeError:
        # Older version of PIL uses constants directly
        downscaled = emoji_img.resize((8, 8), Image.NEAREST)
    
    # Convert to RGB if needed
    downscaled = downscaled.convert('RGB')
    
    # Get the pixels as a list of RGB tuples
    pixels = list(downscaled.getdata())
    
    # Reshape into 8x8 grid
    grid = []
    for row in range(8):
        grid_row = []
        for col in range(8):
            idx = row * 8 + col
            r, g, b = pixels[idx]
            grid_row.append(f"{r},{g},{b}")
        grid.append(grid_row)
    
    return grid

def create_64bit_emoji_csv(emoji, emoji_name, output_dir):
    """
    Create a 64-bit (8x8) representation of an emoji as a CSV file.
    Each cell contains RGB values representing the downsampled emoji image.
    """
    # Create emoji image
    emoji_img = get_emoji_image(emoji)
    
    # Downscale to 8x8
    grid = downscale_to_8x8(emoji_img)
    
    # Create the output directory if it doesn't exist
    emoji_dir = os.path.join(output_dir, emoji_name)
    os.makedirs(emoji_dir, exist_ok=True)
    
    # Write the grid to a CSV file
    csv_file_path = os.path.join(emoji_dir, f"{emoji_name}.csv")
    with open(csv_file_path, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerows(grid)
    
    print(f"Created 64-bit representation for '{emoji}' ({emoji_name}) at {csv_file_path}")
    return csv_file_path

def process_emoji_list(emoji_list_file, output_dir):
    """
    Process the emoji list file and create CSV representations for each emoji.
    """
    with open(emoji_list_file, 'r', encoding='utf-8') as f:
        emojis = f.readlines()
    
    for i, emoji in enumerate(emojis):
        emoji = emoji.strip()
        if emoji:  # Skip empty lines
            try:
                # Get the character to use for name lookup (first character of emoji)
                char_for_name = emoji[0] if emoji else '?'
                emoji_name = unicodedata.name(char_for_name, '').lower().replace(' ', '_').replace('-', '_')
                if not emoji_name or emoji_name == '':
                    # If unicodedata.name doesn't work, use a generic name based on the emoji itself
                    emoji_name = f"emoji_{i}_{hex(ord(emoji[0]))[2:]}" if emoji else f"emoji_{i}_unknown"
            except (ValueError, TypeError):
                # If unicodedata.name raises ValueError, use a generic name
                emoji_name = f"emoji_{i}_{hex(ord(emoji[0]))[2:]}" if emoji else f"emoji_{i}_unknown"
                
            create_64bit_emoji_csv(emoji, emoji_name, output_dir)

if __name__ == "__main__":
    # Define paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    emoji_list_path = os.path.join(script_dir, "emoji_list.txt")
    output_directory = script_dir  # Store in the same directory as the script
    
    # Process all emojis
    process_emoji_list(emoji_list_path, output_directory)
    print("Processing complete!")