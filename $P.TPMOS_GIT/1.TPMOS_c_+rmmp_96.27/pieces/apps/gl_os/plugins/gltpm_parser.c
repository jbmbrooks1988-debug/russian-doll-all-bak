/*
 * gltpm_parser.c - minimal GLTPM scene loader
 * Included directly by gl_desktop.c for the first vertical slice.
 */

#define MAX_GLTPM_VARS 256
#define MAX_GLTPM_VALUE 65536
#define MAX_GLTPM_LEGEND 128
#define MAX_GLTPM_BUFFER 1048576

typedef struct {
    char name[64];
    char value[MAX_GLTPM_VALUE];
} GLTPMVar;

typedef struct {
    char glyph;
    char type[16]; /* "tile" or "sprite" */
    char asset_id[64];
} GLTPMLegend;

static void gltpm_scene_reset(GLTPMScene *scene) {
    if (!scene) return;
    memset(scene, 0, sizeof(*scene));
    strcpy(scene->title, "GLTPM Window");
    scene->camera_mode = 4;
    scene->bg_color[0] = 0.08f;
    scene->bg_color[1] = 0.10f;
    scene->bg_color[2] = 0.16f;
}

static char* gltpm_trim(char *s) {
    char *end = NULL;
    if (!s) return s;

    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (*s == '\0') return s;

    end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end-- = '\0';
    }
    return s;
}

static void gltpm_load_vars_from_file(const char *path, GLTPMVar vars[], int *var_count) {
    FILE *f = NULL;
    char line[MAX_LINE];

    if (!path || !vars || !var_count) return;
    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f) && *var_count < MAX_GLTPM_VARS) {
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        strncpy(vars[*var_count].name, gltpm_trim(line), sizeof(vars[*var_count].name) - 1);
        strncpy(vars[*var_count].value, gltpm_trim(eq + 1), MAX_GLTPM_VALUE - 1);
        vars[*var_count].name[sizeof(vars[*var_count].name) - 1] = '\0';
        vars[*var_count].value[MAX_GLTPM_VALUE - 1] = '\0';
        (*var_count)++;
    }

    fclose(f);
}

static const char* gltpm_get_var(GLTPMVar vars[], int var_count, const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) return vars[i].value;
    }
    return "";
}

static void gltpm_substitute_vars(const char *src, char *dst, size_t dst_size,
                                  GLTPMVar vars[], int var_count) {
    const char *p = src;
    char *out = dst;
    size_t remaining = dst_size;

    if (!src || !dst || dst_size == 0) return;
    dst[0] = '\0';

    while (*p && remaining > 1) {
        if (p[0] == '$' && p[1] == '{') {
            const char *end = strchr(p, '}');
            if (end) {
                char key[64];
                size_t len = (size_t)(end - (p + 2));
                const char *value = NULL;

                if (len >= sizeof(key)) len = sizeof(key) - 1;
                memcpy(key, p + 2, len);
                key[len] = '\0';
                value = gltpm_get_var(vars, var_count, key);

                while (*value && remaining > 1) {
                    if (*value == '\\' && *(value+1) == 'n') {
                        *out++ = '\n';
                        value += 2;
                    } else {
                        *out++ = *value++;
                    }
                    remaining--;
                }
                p = end + 1;
                continue;
            }
        }

        *out++ = *p++;
        remaining--;
    }

    *out = '\0';
}

static int gltpm_extract_attr(const char *line, const char *name, char *dst, size_t dst_size) {
    char needle[64];
    const char *start = NULL;
    const char *end = NULL;
    size_t len = 0;

    if (!line || !name || !dst || dst_size == 0) return 0;
    snprintf(needle, sizeof(needle), "%s=\"", name);
    start = strstr(line, needle);
    if (!start) return 0;

    start += strlen(needle);
    end = strchr(start, '"');
    if (!end) return 0;

    len = (size_t)(end - start);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, start, len);
    dst[len] = '\0';
    return 1;
}

static void gltpm_parse_rgb(const char *value, float rgb[3], float fallback_r,
                            float fallback_g, float fallback_b) {
    int r = 0, g = 0, b = 0;

    rgb[0] = fallback_r;
    rgb[1] = fallback_g;
    rgb[2] = fallback_b;

    if (!value) return;
    if (sscanf(value, "%d,%d,%d", &r, &g, &b) == 3) {
        rgb[0] = (float)r / 255.0f;
        rgb[1] = (float)g / 255.0f;
        rgb[2] = (float)b / 255.0f;
    }
}

static void gltpm_build_abs_path(char *dst, size_t dst_size,
                                 const char *root, const char *rel) {
    if (!dst || dst_size == 0) return;
    if (!root || !rel) {
        dst[0] = '\0';
        return;
    }

    if (rel[0] == '/') {
        strncpy(dst, rel, dst_size - 1);
        dst[dst_size - 1] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s/%s", root, rel);
}

static void gltpm_load_tile_metadata(const char *project_root, const char *project_id,
                                     GLTPMTile *tile) {
    char path[MAX_PATH];
    FILE *f = NULL;
    char line[MAX_LINE];

    if (!project_root || !project_id || !tile) return;

    snprintf(path, sizeof(path), "%s/projects/%s/assets/tiles/%s.tile.txt",
             project_root, project_id, tile->tile_id);

    strcpy(tile->ascii, "?");
    strcpy(tile->unicode, "?");
    tile->extrude = 1.0f;
    tile->color[0] = 0.35f;
    tile->color[1] = 0.45f;
    tile->color[2] = 0.65f;

    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        char *key = NULL;
        char *value = NULL;
        if (!eq) continue;
        *eq = '\0';
        key = gltpm_trim(line);
        value = gltpm_trim(eq + 1);

        if (strcmp(key, "ascii") == 0) {
            strncpy(tile->ascii, value, sizeof(tile->ascii) - 1);
        } else if (strcmp(key, "unicode") == 0) {
            strncpy(tile->unicode, value, sizeof(tile->unicode) - 1);
        } else if (strcmp(key, "rgb_top") == 0) {
            gltpm_parse_rgb(value, tile->color, tile->color[0], tile->color[1], tile->color[2]);
        } else if (strcmp(key, "extrude") == 0) {
            tile->extrude = (float)atof(value);
            if (tile->extrude <= 0.0f) tile->extrude = 1.0f;
        }
    }

    fclose(f);
}

static void gltpm_load_artifact(const char *path, unsigned char mask[8][8]) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = gltpm_trim(line);
        char *val = gltpm_trim(eq + 1);
        if (key[0] == 'z' && isdigit(key[1])) {
            int z = atoi(key + 1);
            if (z >= 0 && z < 8) {
                /* Parse 8 hex bytes: FF,81,00... */
                char *saveptr;
                char *token = strtok_r(val, ",", &saveptr);
                for (int i = 0; i < 8 && token; i++) {
                    mask[z][i] = (unsigned char)strtol(token, NULL, 16);
                    token = strtok_r(NULL, ",", &saveptr);
                }
            }
        }
    }
    fclose(f);
}

static void gltpm_load_sprite_metadata(const char *project_root, const char *project_id,
                                       GLTPMSprite *sprite) {
    char path[MAX_PATH];
    FILE *f = NULL;
    char line[MAX_LINE];

    if (!project_root || !project_id || !sprite) return;

    snprintf(path, sizeof(path), "%s/projects/%s/assets/sprites/%s.sprite.txt",
             project_root, project_id, sprite->sprite_id);

    strcpy(sprite->ascii, "@");
    strcpy(sprite->unicode, "@");
    if (sprite->label[0] == '\0') strcpy(sprite->label, "Sprite");
    sprite->color[0] = 0.95f;
    sprite->color[1] = 0.85f;
    sprite->color[2] = 0.25f;
    memset(sprite->artifact_mask, 0, 64);

    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            char *key = NULL;
            char *value = NULL;
            if (!eq) continue;
            *eq = '\0';
            key = gltpm_trim(line);
            value = gltpm_trim(eq + 1);

            if (strcmp(key, "ascii") == 0) {
                strncpy(sprite->ascii, value, sizeof(sprite->ascii) - 1);
            } else if (strcmp(key, "unicode") == 0) {
                strncpy(sprite->unicode, value, sizeof(sprite->unicode) - 1);
            } else if (strcmp(key, "label") == 0) {
                strncpy(sprite->label, value, sizeof(sprite->label) - 1);
            } else if (strcmp(key, "rgb") == 0) {
                gltpm_parse_rgb(value, sprite->color, sprite->color[0], sprite->color[1], sprite->color[2]);
            }
        }
        fclose(f);
    }

    /* Load Artifact Mask from piece directory */
    char artifact_path[MAX_PATH];
    snprintf(artifact_path, sizeof(artifact_path), "%s/projects/%s/pieces/%s/artifact.txt",
             project_root, project_id, sprite->sprite_id);
    if (access(artifact_path, F_OK) != 0) {
        /* Fallback to asset directory if piece doesn't have custom artifact */
        snprintf(artifact_path, sizeof(artifact_path), "%s/projects/%s/assets/sprites/%s.artifact.txt",
                 project_root, project_id, sprite->sprite_id);
    }
    gltpm_load_artifact(artifact_path, sprite->artifact_mask);

    /* Load Face Colors from piece state.txt */
    char state_path[MAX_PATH];
    snprintf(state_path, sizeof(state_path), "%s/projects/%s/pieces/%s/state.txt",
             project_root, project_id, sprite->sprite_id);
    FILE *sf = fopen(state_path, "r");
    if (sf) {
        char line[MAX_LINE];
        sprite->has_face_colors = 1;
        /* Default all faces to sprite base color first */
        for(int i=0; i<6; i++) {
            sprite->face_colors[i][0] = sprite->color[0];
            sprite->face_colors[i][1] = sprite->color[1];
            sprite->face_colors[i][2] = sprite->color[2];
        }
        while (fgets(line, sizeof(line), sf)) {
            char *eq = strchr(line, '='); if (!eq) continue;
            *eq = '\0'; char *k = gltpm_trim(line); char *v = gltpm_trim(eq + 1);
            if (strcmp(k, "face_top_rgb") == 0) gltpm_parse_rgb(v, sprite->face_colors[0], 1, 1, 1);
            else if (strcmp(k, "face_bottom_rgb") == 0) gltpm_parse_rgb(v, sprite->face_colors[1], 1, 1, 1);
            else if (strcmp(k, "face_front_rgb") == 0) gltpm_parse_rgb(v, sprite->face_colors[2], 1, 1, 1);
            else if (strcmp(k, "face_back_rgb") == 0) gltpm_parse_rgb(v, sprite->face_colors[3], 1, 1, 1);
            else if (strcmp(k, "face_right_rgb") == 0) gltpm_parse_rgb(v, sprite->face_colors[4], 1, 1, 1);
            else if (strcmp(k, "face_left_rgb") == 0) gltpm_parse_rgb(v, sprite->face_colors[5], 1, 1, 1);
        }
        fclose(sf);
    }
}

static GLTPMLegend* gltpm_lookup_legend(GLTPMLegend legend[], int legend_count, char glyph) {
    for (int i = 0; i < legend_count; i++) {
        if (legend[i].glyph == glyph) return &legend[i];
    }
    return NULL;
}

static void gltpm_load_legend(const char *path, GLTPMLegend legend[], int *legend_count) {
    FILE *f = NULL;
    char line[MAX_LINE];

    if (!path || !legend || !legend_count) return;
    f = fopen(path, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f) && *legend_count < MAX_GLTPM_LEGEND) {
        char *eq = strchr(line, '=');
        char *value = NULL;
        char *trimmed = gltpm_trim(line);
        if (!eq || trimmed[0] == '#' || trimmed[0] == '\0') continue;

        *eq = '\0';
        value = gltpm_trim(eq + 1);
        legend[*legend_count].glyph = gltpm_trim(line)[0];
        
        if (strncmp(value, "tile:", 5) == 0) {
            strcpy(legend[*legend_count].type, "tile");
            strncpy(legend[*legend_count].asset_id, value + 5, sizeof(legend[*legend_count].asset_id) - 1);
        } else if (strncmp(value, "sprite:", 7) == 0) {
            strcpy(legend[*legend_count].type, "sprite");
            strncpy(legend[*legend_count].asset_id, value + 7, sizeof(legend[*legend_count].asset_id) - 1);
        } else {
            strcpy(legend[*legend_count].type, "tile");
            strncpy(legend[*legend_count].asset_id, value, sizeof(legend[*legend_count].asset_id) - 1);
        }
        
        legend[*legend_count].asset_id[sizeof(legend[*legend_count].asset_id) - 1] = '\0';
        (*legend_count)++;
    }

    fclose(f);
}

static void gltpm_add_tile(GLTPMScene *scene, const char *project_root, const char *project_id,
                           const char *tile_id, float x, float y, float z) {
    GLTPMTile *tile = NULL;
    if (!scene || !tile_id || scene->tile_count >= MAX_GLTPM_TILES) return;

    tile = &scene->tiles[scene->tile_count++];
    memset(tile, 0, sizeof(*tile));
    tile->x = x;
    tile->y = y;
    tile->z = z;
    strncpy(tile->tile_id, tile_id, sizeof(tile->tile_id) - 1);
    gltpm_load_tile_metadata(project_root, project_id, tile);
}

static void gltpm_add_sprite(GLTPMScene *scene, const char *project_root, const char *project_id,
                             const char *sprite_id, float x, float y, float z) {
    GLTPMSprite *sprite = NULL;
    if (!scene || !sprite_id || scene->sprite_count >= MAX_GLTPM_SPRITES) return;

    sprite = &scene->sprites[scene->sprite_count++];
    memset(sprite, 0, sizeof(*sprite));
    sprite->x = x;
    sprite->y = y;
    sprite->z = z;
    strncpy(sprite->sprite_id, sprite_id, sizeof(sprite->sprite_id) - 1);
    gltpm_load_sprite_metadata(project_root, project_id, sprite);
}

/* Updated Fix 1 & 3: Persistent row tracking to prevent overlaps */
static int gltpm_process_canvas_block(GLTPMScene *scene, const char *project_root, const char *project_id,
                                      const char *block, float origin_x, float origin_y, int start_row, float z_level) {
    GLTPMLegend legend[MAX_GLTPM_LEGEND];
    int legend_count = 0;
    char legend_path[MAX_PATH];
    int rows_processed = 0;
    
    snprintf(legend_path, sizeof(legend_path), "%s/projects/%s/assets/tiles/registry.txt", project_root, project_id);
    gltpm_load_legend(legend_path, legend, &legend_count);

    char *copy = strdup(block);
    char *saveptr;
    char *line = strtok_r(copy, "\n", &saveptr);
    
    while (line) {
        /* Skip ASCII border lines and Z-level status lines */
        if (strstr(line, "+====") != NULL || 
            strstr(line, "----") != NULL || 
            strstr(line, "[Z-LEVEL]") != NULL ||
            strstr(line, "LAYOUT ID:") != NULL) { 
            line = strtok_r(NULL, "\n", &saveptr); 
            continue; 
        }

        const char *p = line;
        /* Strip leading sidebar '| ' if present */
        if (*p == '|') {
            p++;
            while (*p == ' ') p++;
        }

        for (int col = 0; p[col] != '\0'; col++) {
            /* Stop at trailing sidebar ' |' or end of map block */
            if (p[col] == '|' || p[col] == '\r') break;
            if (p[col] == ' ') continue;
            
            /* SKIP XELECTOR: The yellow wireframe is drawn by the Host, 
               don't render a generic cube for the ASCII 'X' */
            if (p[col] == 'X') continue;

            GLTPMLegend *l = gltpm_lookup_legend(legend, legend_count, p[col]);
            if (l) {
                if (strcmp(l->type, "tile") == 0) {
                    gltpm_add_tile(scene, project_root, project_id, l->asset_id,
                                   origin_x + (float)col, origin_y + (float)(start_row + rows_processed), z_level);
                } else if (strcmp(l->type, "sprite") == 0) {
                    /* SOVEREIGN FIX: Automatically place a base tile (grass) under sprites 
                       so they don't leave holes in the floor when using ASCII glyphs */
                    gltpm_add_tile(scene, project_root, project_id, "grass",
                                   origin_x + (float)col, origin_y + (float)(start_row + rows_processed), z_level);

                    gltpm_add_sprite(scene, project_root, project_id, l->asset_id,
                                     origin_x + (float)col, origin_y + (float)(start_row + rows_processed), z_level);
                }
            }
        }
        line = strtok_r(NULL, "\n", &saveptr);
        rows_processed++;
    }
    free(copy);
    return rows_processed;
}

static int gltpm_load_scene(GLTPMScene *scene, const char *project_root,
                            const char *project_id, const char *layout_path) {
    GLTPMVar *vars = NULL;
    int var_count = 0;
    char state_path[MAX_PATH];
    char gui_state_path[MAX_PATH];
    char view_path[MAX_PATH];
    char layout_abs[MAX_PATH];
    char *layout_raw = NULL;
    char *layout_sub = NULL;
    int canvas_row_total = 0;

    if (!scene || !project_root || !project_id || !layout_path) return 0;

    vars = malloc(sizeof(GLTPMVar) * MAX_GLTPM_VARS);
    if (!vars) return 0;

    gltpm_scene_reset(scene);
    memset(vars, 0, sizeof(GLTPMVar) * MAX_GLTPM_VARS);

    snprintf(state_path, sizeof(state_path), "%s/projects/%s/session/state.txt",
             project_root, project_id);
    snprintf(gui_state_path, sizeof(gui_state_path), "%s/projects/%s/manager/gui_state.txt",
             project_root, project_id);
    
    char camera_state_path[MAX_PATH];
    snprintf(camera_state_path, sizeof(camera_state_path), "%s/projects/%s/pieces/camera/state.txt",
             project_root, project_id);

    gltpm_load_vars_from_file(state_path, vars, &var_count);
    gltpm_load_vars_from_file(gui_state_path, vars, &var_count);
    gltpm_load_vars_from_file(camera_state_path, vars, &var_count);
    
    snprintf(view_path, sizeof(view_path), "%s/pieces/apps/player_app/view.txt", project_root);
    FILE *vf = fopen(view_path, "r");
    if (vf) {
        char *map_buf = malloc(MAX_GLTPM_VALUE);
        if (map_buf) {
            map_buf[0] = '\0';
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), vf)) {
                if (strlen(map_buf) + strlen(line) < MAX_GLTPM_VALUE - 1) {
                    strcat(map_buf, line);
                }
            }
            strncpy(vars[var_count].name, "game_map", 63);
            strncpy(vars[var_count].value, map_buf, MAX_GLTPM_VALUE - 1);
            var_count++;
            free(map_buf);
        }
        fclose(vf);
    }

    strncpy(vars[var_count].name, "project_id", 63);
    strncpy(vars[var_count].value, project_id, MAX_GLTPM_VALUE - 1);
    var_count++;

    /* Fixed 4: Extract Dynamic Camera/Xelector state */
    strncpy(scene->last_key, gltpm_get_var(vars, var_count, "last_key"), 31);
    scene->is_map_control = atoi(gltpm_get_var(vars, var_count, "is_map_control"));
    scene->camera_mode = atoi(gltpm_get_var(vars, var_count, "camera_mode"));
    if (scene->camera_mode <= 0) scene->camera_mode = 4;

    scene->cam_pos[0] = (float)atof(gltpm_get_var(vars, var_count, "cam_x"));
    scene->cam_pos[1] = (float)atof(gltpm_get_var(vars, var_count, "cam_y"));
    scene->cam_pos[2] = (float)atof(gltpm_get_var(vars, var_count, "cam_z"));
    scene->cam_rot[0] = (float)atof(gltpm_get_var(vars, var_count, "cam_pitch"));
    scene->cam_rot[1] = (float)atof(gltpm_get_var(vars, var_count, "cam_yaw"));
    scene->xelector_pos[0] = atoi(gltpm_get_var(vars, var_count, "xelector_pos_x"));
    scene->xelector_pos[1] = atoi(gltpm_get_var(vars, var_count, "xelector_pos_y"));
    scene->xelector_pos[2] = atoi(gltpm_get_var(vars, var_count, "current_z"));

    gltpm_build_abs_path(layout_abs, sizeof(layout_abs), project_root, layout_path);
    FILE *f = fopen(layout_abs, "r");
    if (!f) { free(vars); return 0; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    layout_raw = malloc(sz + 1); fread(layout_raw, 1, sz, f); layout_raw[sz] = '\0';
    fclose(f);

    layout_sub = malloc(MAX_GLTPM_BUFFER);
    gltpm_substitute_vars(layout_raw, layout_sub, MAX_GLTPM_BUFFER, vars, var_count);
    free(layout_raw);

    char *p = layout_sub;
    while (*p) {
        char *line_end = strchr(p, '\n');
        if (line_end) *line_end = '\0';

        char *trimmed = gltpm_trim(p);
        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            if (!line_end) break;
            p = line_end + 1;
            continue;
        }

        if (strncmp(trimmed, "<scene", 6) == 0) {
            char title[128] = ""; char bg[64] = ""; char camera[32] = "";
            if (gltpm_extract_attr(trimmed, "title", title, sizeof(title))) strncpy(scene->title, title, MAX_WIN_TITLE - 1); scene->title[MAX_WIN_TITLE-1] = '\0';
            if (gltpm_extract_attr(trimmed, "bg", bg, sizeof(bg))) gltpm_parse_rgb(bg, scene->bg_color, scene->bg_color[0], scene->bg_color[1], scene->bg_color[2]);
            if (gltpm_extract_attr(trimmed, "camera_mode", camera, sizeof(camera))) {
                scene->camera_mode = atoi(camera); if (scene->camera_mode <= 0) scene->camera_mode = 4;
            }
        } else if (strncmp(trimmed, "<camera", 7) == 0) {
            char mode[32] = "", x[32] = "", y[32] = "", z[32] = "", pitch[32] = "", yaw[32] = "";
            if (gltpm_extract_attr(trimmed, "mode", mode, sizeof(mode))) scene->camera_mode = atoi(mode);
            if (gltpm_extract_attr(trimmed, "x", x, sizeof(x))) scene->cam_pos[0] = (float)atof(x);
            if (gltpm_extract_attr(trimmed, "y", y, sizeof(y))) scene->cam_pos[1] = (float)atof(y);
            if (gltpm_extract_attr(trimmed, "z", z, sizeof(z))) scene->cam_pos[2] = (float)atof(z);
            if (gltpm_extract_attr(trimmed, "pitch", pitch, sizeof(pitch))) scene->cam_rot[0] = (float)atof(pitch);
            if (gltpm_extract_attr(trimmed, "yaw", yaw, sizeof(yaw))) scene->cam_rot[1] = (float)atof(yaw);
        } else if (strncmp(trimmed, "<sprite", 7) == 0) {
            char id[64] = "", x[32] = "", y[32] = "", z[32] = "";
            if (gltpm_extract_attr(trimmed, "id", id, sizeof(id))) {
                float sx = (float)atof(gltpm_extract_attr(trimmed, "x", x, sizeof(x)) ? x : "0");
                float sy = (float)atof(gltpm_extract_attr(trimmed, "y", y, sizeof(y)) ? y : "0");
                float sz = (float)atof(gltpm_extract_attr(trimmed, "z", z, sizeof(z)) ? z : "0");
                gltpm_add_sprite(scene, project_root, project_id, id, sx, sy, sz);
            }
        } else if (strncmp(trimmed, "<interact", 9) == 0) {
            char src[MAX_PATH];
            if (gltpm_extract_attr(trimmed, "src", src, sizeof(src))) strncpy(scene->interact_src, src, MAX_PATH-1);
        } else if (strstr(trimmed, "<button") != NULL) {
            if (scene->button_count < MAX_GLTPM_BUTTONS) {
                GLTPMButton *btn = &scene->buttons[scene->button_count++];
                char id[128], lbl[128], clk[128];
                if (gltpm_extract_attr(trimmed, "id", id, 127)) strncpy(btn->id, id, 63);
                if (gltpm_extract_attr(trimmed, "label", lbl, 127)) strncpy(btn->label, lbl, 127);
                if (gltpm_extract_attr(trimmed, "onClick", clk, 127)) strncpy(btn->onClick, clk, 127);
            }
        } else if (strstr(trimmed, "<text") != NULL) {
            if (scene->text_count < MAX_GLTPM_TEXTS) {
                char lbl[256];
                if (gltpm_extract_attr(trimmed, "label", lbl, 255)) strncpy(scene->texts[scene->text_count++].label, lbl, 255);
            }
        } else if (trimmed[0] == '<') {
            /* skip */
        } else {
            /* Raw block - process as map canvas with persistent row tracking */
            canvas_row_total += gltpm_process_canvas_block(scene, project_root, project_id, trimmed, -10.0f, -5.0f, canvas_row_total, 0.0f);
        }

        if (!line_end) break;
        p = line_end + 1;
    }

    free(layout_sub);
    free(vars);
    scene->loaded = 1;
    return 1;
}
