# Minecraft Texture to CSV Converter and Viewer

A suite of tools for converting Minecraft textures to CSV format and viewing them in both 2D galleries and 3D viewers.

## Features

- Convert individual 16x16 Minecraft textures to CSV format
- Support for both 8x8 (downscaled) and 16x16 output formats
- Gallery viewer for browsing all textures in a tile grid
- Interactive 3D block viewer with rotation and texture cycling
- Keyboard navigation for easy texture exploration
- Convert 2D textures to 3D voxel models

## Prerequisites

- Python 3.6+
- Required packages: `pygame`, `PyOpenGL`, `PyOpenGL_accelerate`, `PIL` (Pillow), `numpy`

Install dependencies:
```bash
pip install pygame PyOpenGL PyOpenGL_accelerate Pillow numpy
```

## Directory Structure

The tools expect this structure:
```
project_root/
├── textures+labled=indi=16x16=93/
│   ├── texture1.png
│   ├── texture2.png
│   └── ...
├── mc_texture_to_csv_converter+.py
├── mc_block_gallery_viewer+.py
├── mc_single_3d_block_viewer+.py
├── convert_2d_to_3d.py
└── create_atlas_mapping+.py
```

## Usage

### 1. Converting Textures to CSV

Convert all textures to 8x8 CSV files:
```bash
python3 mc_texture_to_csv_converter+.py
```

Convert to 16x16 CSV files:
```bash
python3 mc_texture_to_csv_converter+.py --size 16
```

Specify custom input/output directories:
```bash
python3 mc_texture_to_csv_converter+.py --input-dir ./custom_textures --output-dir ./output --size 8
```

Options:
- `--input-dir` or `-i`: Input directory with PNG textures (default: `textures+labled=indi=16x16=93`)
- `--output-dir` or `-o`: Output directory base (default: current directory)
- `--size` or `-s`: Output size (8 or 16) (default: 8)

### 2. Gallery Viewer

Browse all converted textures in a grid:
```bash
# View all textures in current directory
python3 mc_block_gallery_viewer+.py

# View textures from a specific directory
python3 mc_block_gallery_viewer+.py --dir ./mc_extracted_csvs_8x8

# View specific grid size
python3 mc_block_gallery_viewer+.py --grid-size 8x8

# Customize display
python3 mc_block_gallery_viewer+.py --cols 10 --size 1200 800
```

Options:
- `--dir` or `-d`: Base directory containing block folders (default: `.`)
- `--size` or `-s`: Window size as WIDTH HEIGHT (default: 1000 700)
- `--cols` or `-c`: Number of columns to display (default: 8)
- `--grid-size` or `-g`: Grid size to display (8x8 or 16x16)

### 3. 3D Block Viewer

View a single texture in 3D with rotation and texture cycling:
```bash
# View first available texture
python3 mc_single_3d_block_viewer+.py

# View a specific texture
python3 mc_single_3d_block_viewer+.py --file ./mc_extracted_csvs_8x8/block_name/block_name.csv

# Specify directory and grid size
python3 mc_single_3d_block_viewer+.py --dir ./mc_extracted_csvs_16x16 --grid-size 16x16
```

Controls in 3D Viewer:
- Mouse drag: Rotate the 3D block
- Scroll wheel: Zoom in/out
- Arrow keys: Rotate the 3D block (up/down/left/right)
- WASD keys: Alternative rotation controls
- Q/E keys: Rotate around Z axis
- R: Reset rotation
- Left/Right Arrow Keys: Cycle through available textures
- ESC: Quit

### 4. Converting 2D to 3D Voxel Models

Convert 2D texture files to 3D voxel representations:
```bash
# Convert from default 8x8 textures
python3 convert_2d_to_3d.py

# Convert from 16x16 textures
python3 convert_2d_to_3d.py --input ./mc_extracted_csvs_16x16 --output ./3d_voxels
```

Options:
- `--input` or `-i`: Input directory with 2D texture CSVs (default: `mc_extracted_csvs_8x8`)
- `--output` or `-o`: Output directory for 3D textures (default: `3d_output`)

## Shell Scripts for Easy Access

For convenience, you can also use the bundled shell scripts:

- `convert_textures.sh` - Converts textures to CSV
- `view_gallery.sh` - Opens the gallery viewer
- `view_3d_block.sh` - Opens the 3D block viewer
- `convert_2d_to_3d.sh` - Converts 2D textures to 3D voxel models
- `test_setup.sh` - Verifies that the tools are properly set up

Make them executable first:
```bash
chmod +x *.sh
```

Then run:
```bash
./convert_textures.sh       # Convert textures to CSV
./view_gallery.sh           # View gallery
./view_3d_block.sh          # View 3D blocks
./convert_2d_to_3d.sh       # Convert 2D to 3D voxels
./test_setup.sh             # Test the setup
```

## Output Structure

After conversion, the directory will contain:
```
mc_extracted_csvs_8x8/
├── block_name1/
│   └── block_name1.csv
├── block_name2/
│   └── block_name2.csv
└── ...

mc_extracted_csvs_16x16/
├── block_name1/
│   └── block_name1.csv
└── ...

3d_output/
├── block_name1/
│   └── block_name1.csv
└── ...
```

Each CSV file in the mc_extracted directories contains the RGB values for the corresponding texture.
Each CSV file in the 3d_output directory contains 3D coordinates and RGB values for voxel models.

## Notes

- Textures in the input directory should be 16x16 pixels
- The tool preserves color information using nearest neighbor scaling for better color variety
- Large texture collections may require significant RAM when viewing in the gallery
- The 3D viewer automatically detects whether each CSV is 8x8 or 16x16 and adjusts visualization accordingly
- The 2D to 3D converter supports both 8x8 and 16x16 input textures