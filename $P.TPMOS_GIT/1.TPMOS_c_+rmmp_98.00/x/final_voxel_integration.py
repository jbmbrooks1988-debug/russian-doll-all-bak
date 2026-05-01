import sys

with open('pieces/apps/gl_os/plugins/gl_desktop.c', 'r') as f:
    lines = f.readlines()

# Add Voxel DNA Definitions and draw_voxel_piece function implementation
voxel_code_definitions = """
/* 
 * PIECE DNA: 8x8x8 Voxel Bitmasks 
 * Each array is [Z-level][Y-row] with 8 bits per row (X)
 */
unsigned char DNA_XEL[8][8] = {
    {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF}, // Bottom frame
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81},
    {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF}  // Top frame
};

unsigned char DNA_PET[8][8] = {
    {0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00}, // Body base
    {0x00, 0x7E, 0x81, 0x81, 0x81, 0x81, 0x7E, 0x00},
    {0x00, 0x7E, 0xA5, 0x81, 0x81, 0xA5, 0x7E, 0x00}, // Eyes level
    {0x00, 0x7E, 0x81, 0x81, 0x81, 0x81, 0x7E, 0x00},
    {0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00},
    {0x00, 0x00, 0x24, 0x00, 0x00, 0x24, 0x00, 0x00}, // Ears
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

void draw_voxel_piece(float x, float y, float z, float size, unsigned char dna[8][8], float r, float g, float b, int wireframe) {
    float v_size = size / 8.0f;
    float offset = -size / 2.0f + v_size / 2.0f;

    for (int iz = 0; iz < 8; iz++) {
        for (int iy = 0; iy < 8; iy++) {
            unsigned char row = dna[iz][iy];
            for (int ix = 0; ix < 8; ix++) {
                if (row & (1 << (7 - ix))) {
                    float vx = x + offset + (ix * v_P); // NOTE: The original code had some issues here. Let's fix it.
                    float vy = y + (iz * v_size); // iz is height in our local stack
                    float vz = z + offset + (iy * v_size);
                    
                    glPushMatrix();
                    glTranslatef(vx, vy, vz);
                    if (wireframe) {
                        glColor3f(r, g, b);
                        glutWireCube(v_size);
                    } else {
                        draw_tile(0, 0, 0, v_size, r, g, b);
                    }
                    glPopMatrix();
                }
            }
        }
    }
}
"""

# Find the end of draw_tile function to insert draw_voxel_piece
insert_point = -1
brace_count = 0
draw_tile_found = False
for i, line in enumerate(content):
    if 'void draw_tile(float x, float y, float z, float size, float r, float g, float b)' in line:
        start_brace_idx = -1
        for j in range(i, len(content)):
            if content[j] == '{':
                start_brace_idx = j
                break
        if start_brace_idx != -1:
            brace_count = 1
            for j in range(start_brace_idx + 1, len(content)):
                brace_count += content[j].count('{')
                brace_count -= content[j].count('}')
                if brace_count == 0:
                    insert_point = j + 1
                    break
            if insert_point != -1:
                break

if insert_point != -1:
    content.insert(insert_point, voxel_code_definitions + '\n')

# Ensure #include <sys/time.h> is present
if '#include <sys/time.h>' not in content:
    include_pos = content.find('#include <time.h>')
    if include_pos != -1:
        content.insert(include_pos + len('#include <time.h>\\n'), '#include <sys/time.h>\\n')

# Update draw_window to use draw_voxel_piece for sprites and Xelector
old_xel_block = """        /* Draw Xelector (Grid Cursor) */
        glPushMatrix();
        float sel_gl_x = win->xelector_pos[0] * 1.2f;
        float sel_gl_y = win->xelector_pos[2] * 0.5f; /* Elevation */
        float sel_gl_z = win->xelector_pos[1] * 1.2f; /* Depth */
        glTranslatef(sel_gl_x, sel_gl_y, sel_gl_z);
        
        /* Add Blinking Logic (KISS) */
        if ((time(NULL) % 2) == 0) {
            draw_xelector(1.2f);
        }
        glPopMatrix();"""

new_xel_block = """        /* Draw Xelector (Voxel-XEL) - Sovereign Position */
        float sel_gl_x = win->xelector_pos[0] * 1.2f;
        float sel_gl_z = win->xelector_pos[1] * 1.2f;
        float sel_gl_y = 1.1f + (win->xelector_pos[2] * 0.5f); 
        struct timeval tv; gettimeofday(&tv, NULL);
        if ((tv.tv_usec / 200000) % 2 == 0) {
            draw_voxel_piece(sel_gl_x, sel_gl_y, sel_gl_z, 1.2f, DNA_XEL, 1.0f, 1.0f, 0.0f, 1);
        }"""
content = "".join(content)
content = content.replace(old_xel_block, new_xel_block)

old_sprite_block = """        for (int i = 0; i < win->gltpm_scene.sprite_count; i++) {
            GLTPMSprite *sprite = &win->gltpm_scene.sprites[i];
            glPushMatrix();
            glTranslatef(sprite->x * 1.2f, 1.0f + sprite->z, sprite->y * 1.2f);
            draw_tile(0, 0, 0, 0.7f, sprite->color[0], sprite->color[1], sprite->color[2]);
            glPopMatrix();
        }"""

new_sprite_block = """        for (int i = 0; i < win->gltpm_scene.sprite_count; i++) {
            GLTPMSprite *sprite = &win->gltpm_scene.sprites[i];
            draw_voxel_piece(sprite->x * 1.2f, 1.0f + sprite->z, sprite->y * 1.2f, 0.8f, sprite->voxel_dna, 
                             sprite->color[0], sprite->color[1], sprite->color[2], 0);
        }"""
content = content.replace(old_sprite_block, new_sprite_block)


with open('pieces/apps/gl_os/plugins/gl_desktop.c', 'w') as f:
    f.write(content)

# Compile to verify
print("Attempting compilation after integrating voxel engine...")
