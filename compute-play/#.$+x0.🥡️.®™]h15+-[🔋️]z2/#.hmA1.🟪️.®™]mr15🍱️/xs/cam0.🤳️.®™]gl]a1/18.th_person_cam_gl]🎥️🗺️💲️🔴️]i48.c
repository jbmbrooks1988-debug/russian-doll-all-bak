#include <GL/glut.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <regex.h>
#include <freetype2/ft2build.h>
#include <ctype.h> // For isprint
#include FT_FREETYPE_H

FT_Library ft;
FT_Face face;

#define MAX_LINE 256
#define TRANSLUCENT_BLOCKS 1
#define ENABLE_AR 1
#define CAMERA_WIDTH 640
#define CAMERA_HEIGHT 480
#define INITIAL_MAP_CAPACITY 10
#define MINI_MAP_VIEW_SIZE 10
#define MAX_MAPS 100
#define MAX_MAP_SIZE 1000
#define TARGET_FPS 24
#define MAX_PATH 256
#define MAX_SUPER_STATES 100
#define MAX_PIECES_PER_STATE 100
#define DEFAULT_PIECE_SIZE 0.25f
#define DEFAULT_QUAD_SIZE 0.6f
#define SHAPE_SPHERE 0
#define SHAPE_CUBE 1
#define MAX_COMMAND_LINES 100
#define INITIAL_COMMAND_CAPACITY 20
#define FILE_CHECK_INTERVAL 100000 // 100ms in microseconds

// Forward declarations
void log_to_file(const char *message);
void log_input_to_file(const char *input_type, const char *details);
int is_directory_not_empty(const char *dir_path);
void read_locations();
void yuyv_to_rgb(unsigned char* yuyv, unsigned char* rgb, int width, int height);
int init_camera(int *fd, void* buffers[], unsigned int *num_buffers, const char *device, int width, int height);
void update_camera(int fd, void* buffers[], unsigned int num_buffers, GLuint texture_id, int width, int height);
void cleanup_camera(int *fd, void* buffers[], unsigned int num_buffers, int *enabled, int width, int height);
void free_maps();
void resize_maps();
void resize_super_state_pieces(int needed);
int parse_filename(const char *filename, int *map_x, int *map_y, int *map_z);
void parse_state_file(const char *filename, int is_state_file, int *x, int *y, int *z, int *cam_x, int *cam_y, int *cam_z, int *cursor_pos, char *piece_symbol, int *has_coords);
void read_maps();
void read_state();
void read_super_state();
void check_maps_update();
void init_map_display_lists();
void draw_cube(float x, float y, float z, float size);
void draw_sphere(float x, float y, float z, float size);
void draw_cursor(float x, float y, float z);
void draw_text(float x, float y, const char *string);
void draw_mini_map();
void read_commands();
void draw_terminal();
int check_commands_update();
int check_memory_usage();
void display();
void keyboard(unsigned char key, int x, int y);
void special(int key, int x, int y);
void mouse(int button, int state, int x, int y);
void animate();
void reshape(int w, int h);
void init();

// Global variables
int player_x = 0, player_y = 0, player_z = 0;
int player_coords_valid = 0; // Flag to track if player coordinates are valid
float player_r = 0.0f, player_g = 0.0f, player_b = 1.0f; // Default to blue


int cam_x = 0, cam_y = 0, cam_z = 0;
int cursor_pos[3] = {0, 0, 0};
char piece_symbol[16] = "P";
int window_width = 800, window_height = 600;
time_t last_map_load_time = 0;
time_t last_command_time = 0;
struct timespec last_command_check = {0, 0};
GLuint *map_display_lists = NULL;
int glut_initialized = 0;
int terminal_enabled = 1; // Enable terminal by default

int scroll_offset = 0;
// Add to global variables section
int dragging = 0;

// Dynamic file paths
char state_path[MAX_PATH] = "../state.txt";
char maps_dir[MAX_PATH] = "../maps/";
char commands_path[MAX_PATH] = "commands_gl.txt";
char terminal_input_path[MAX_PATH] = "gl_cli_out.txt"; // New path for reading terminal input
char log_path[MAX_PATH] = "log.txt";
char super_maps_dir[MAX_PATH] = "";
char super_states_dir[MAX_PATH] = "";

// Dynamic map storage
char ***maps = NULL;
int *map_coords = NULL;
int *map_sizes = NULL;
int num_maps = 0;
int map_capacity = 0;

// Super state storage
int *super_state_x = NULL;
int *super_state_y = NULL;
int *super_state_z = NULL;
char **super_state_symbol = NULL;
float *super_state_r = NULL;
float *super_state_g = NULL;
float *super_state_b = NULL;
int *super_state_has_color = NULL;
float *super_state_size = NULL;
int *super_state_shape = NULL;
int num_super_state_pieces = 0;
int super_state_piece_capacity = 0;

// Command terminal storage
char (*command_lines)[MAX_LINE] = NULL;
int num_command_lines = 0;
int command_capacity = 0;

// AR camera variables
int ar_camera_fd = -1;
void* ar_camera_buffers[4];
unsigned int ar_num_buffers = 0;
GLuint ar_texture_id;
int ar_frame_width = CAMERA_WIDTH;
int ar_frame_height = CAMERA_HEIGHT;
int ar_camera_enabled = 0;

int getEmojiCodepoints(const char *str, unsigned int *codepoints, int max_count) {
    int count = 0;
    while (*str && count < max_count) {
        unsigned char c = *str;
        unsigned int codepoint = 0;
        int bytes = 0;

        if (c < 0x80) {
            codepoint = c;
            bytes = 1;
        } else if ((c & 0xE0) == 0xC0) {
            codepoint = c & 0x1F;
            bytes = 2;
        } else if ((c & 0xF0) == 0xE0) {
            codepoint = c & 0x0F;
            bytes = 3;
        } else if ((c & 0xF8) == 0xF0) {
            codepoint = c & 0x07;
            bytes = 4;
        } else {
            str++;
            continue;
        }

        for (int i = 1; i < bytes; i++) {
            if ((str[i] & 0xC0) != 0x80) {
                bytes = 0;
                break;
            }
            codepoint = (codepoint << 6) | (str[i] & 0x3F);
        }

        if (bytes > 0 && codepoint >= 0x1F300 && codepoint <= 0x1FAD6) {
            codepoints[count++] = codepoint;
        }

        str += bytes ? bytes : 1;
    }
    return count;
}

void initFreeType() {
    FT_Error err = FT_Init_FreeType(&ft);
    if (err) {
        log_to_file("Error: Could not init FreeType");
        exit(1);
    }
    const char *font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    err = FT_New_Face(ft, font_path, 0, &face);
    if (err) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Error: Could not load font at %s, error code: %d", font_path, err);
        log_to_file(msg);
        FT_Done_FreeType(ft);
        exit(1);
    }
    if (FT_IS_SCALABLE(face)) {
        err = FT_Set_Pixel_Sizes(face, 0, 24);
        if (err) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Error: Could not set pixel size to 24, error code: %d", err);
            log_to_file(msg);
            FT_Done_Face(face);
            FT_Done_FreeType(ft);
            exit(1);
        }
    } else if (face->num_fixed_sizes > 0) {
        err = FT_Select_Size(face, 0);
        if (err) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Error: Could not select size 0, error code: %d", err);
            log_to_file(msg);
            FT_Done_Face(face);
            FT_Done_FreeType(ft);
            exit(1);
        }
    } else {
        log_to_file("Error: No fixed sizes available in font");
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        exit(1);
    }
    char msg[256];
    snprintf(msg, sizeof(msg), "FreeType initialized, font loaded: %s, scalable: %d", font_path, FT_IS_SCALABLE(face));
    log_to_file(msg);
}

void renderEmoji(unsigned int codepoint, float x, float y, float scale) {
    FT_Error err = FT_Load_Char(face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR);
    if (err) {
        char debug_msg[256];
        snprintf(debug_msg, sizeof(debug_msg), "Error: Could not load glyph for codepoint U+%04X, error code: %d", codepoint, err);
        log_to_file(debug_msg);
        return;
    }

    FT_GlyphSlot slot = face->glyph;
    if (!slot->bitmap.buffer) {
        char debug_msg[256];
        snprintf(debug_msg, sizeof(debug_msg), "Error: No bitmap for glyph U+%04X", codepoint);
        log_to_file(debug_msg);
        return;
    }
    char debug_msg[256];
    snprintf(debug_msg, sizeof(debug_msg), "Glyph loaded: U+%04X, width=%d, height=%d", codepoint, slot->bitmap.width, slot->bitmap.rows);
    log_to_file(debug_msg);

    unsigned char *buffer = slot->bitmap.buffer;
    int width = slot->bitmap.width;
    int height = slot->bitmap.rows;

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Scale emoji to match text size (~10-15 pixels in screen space)
    float pixel_height = scale; // scale is 16, 20, or 24 from draw_terminal
    float normalized_height = pixel_height / window_height; // Convert pixels to normalized coords
    float aspect_ratio = (float)width / height;
    float normalized_width = normalized_height * aspect_ratio;
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 1.0); glVertex2f(x, y);
    glTexCoord2f(1.0, 1.0); glVertex2f(x + normalized_width, y);
    glTexCoord2f(1.0, 0.0); glVertex2f(x + normalized_width, y + normalized_height);
    glTexCoord2f(0.0, 0.0); glVertex2f(x, y + normalized_height);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &texture);
}

void log_to_file(const char *message) {
    FILE *fp = fopen(log_path, "a");
    if (!fp) return;
    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = '\0';
    fprintf(fp, "[%s] %s\n", timestamp, message);
    fclose(fp);
}

void log_input_to_file(const char *input_type, const char *details) {
    FILE *fp = fopen(commands_path, "a");
    if (!fp) return;
    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = '\0';
    fprintf(fp, "[%s] %s: %s\n", timestamp, input_type, details);
    fclose(fp);
}

int is_directory_not_empty(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return 0;
    struct dirent *entry;
    int file_count = 0;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_REG) {
            file_count++;
            break;
        }
    }
    closedir(dir);
    return file_count > 0;
}

void read_locations() {
    FILE *fp = fopen("locations.txt", "r");
    if (!fp) {
        log_to_file("Warning: Failed to open locations.txt, using default paths");
        return;
    }
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        char key[MAX_PATH], value[MAX_PATH];
        if (sscanf(line, "%s %s", key, value) != 2) continue;
        if (strcmp(key, "state") == 0) {
            strncpy(state_path, value, MAX_PATH - 1);
            state_path[MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "maps") == 0) {
            strncpy(maps_dir, value, MAX_PATH - 1);
            maps_dir[MAX_PATH - 1] = '\0';
            if (maps_dir[strlen(maps_dir) - 1] != '/') {
                strncat(maps_dir, "/", MAX_PATH - strlen(maps_dir) - 1);
            }
        } else if (strcmp(key, "commands") == 0) {
            strncpy(commands_path, value, MAX_PATH - 1);
            commands_path[MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "terminal_input") == 0) {
            strncpy(terminal_input_path, value, MAX_PATH - 1);
            terminal_input_path[MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "log") == 0) {
            strncpy(log_path, value, MAX_PATH - 1);
            log_path[MAX_PATH - 1] = '\0';
        } else if (strcmp(key, "super_maps") == 0) {
            strncpy(super_maps_dir, value, MAX_PATH - 1);
            super_maps_dir[MAX_PATH - 1] = '\0';
            if (super_maps_dir[strlen(super_maps_dir) - 1] != '/') {
                strncat(super_maps_dir, "/", MAX_PATH - strlen(super_maps_dir) - 1);
            }
        } else if (strcmp(key, "super_states") == 0) {
            strncpy(super_states_dir, value, MAX_PATH - 1);
            super_states_dir[MAX_PATH - 1] = '\0';
            if (super_states_dir[strlen(super_states_dir) - 1] != '/') {
                strncat(super_states_dir, "/", MAX_PATH - strlen(super_states_dir) - 1);
            }
        }
    }
    fclose(fp);
    char msg[MAX_PATH * 7];
    snprintf(msg, sizeof(msg), "Loaded paths: state=%s, maps=%s, commands=%s, terminal_input=%s, log=%s, super_maps=%s, super_states=%s",
             state_path, maps_dir, commands_path, terminal_input_path, log_path, super_maps_dir, super_states_dir);
    log_to_file(msg);
}

void yuyv_to_rgb(unsigned char* yuyv, unsigned char* rgb, int width, int height) {
    for (int i = 0; i < width * height / 2; i++) {
        int y0 = yuyv[i * 4 + 0];
        int v = yuyv[i * 4 + 1];
        int y1 = yuyv[i * 4 + 2];
        int u = yuyv[i * 4 + 3];
        int c = y0 - 16;
        int d = u - 128;
        int e = v - 128;
        int r = (298 * c + 409 * e + 128) >> 8;
        int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
        int b = (298 * c + 516 * d + 128) >> 8;
        rgb[i * 6 + 0] = r < 0 ? 0 : (r > 255 ? 255 : r);
        rgb[i * 6 + 1] = g < 0 ? 0 : (g > 255 ? 255 : g);
        rgb[i * 6 + 2] = b < 0 ? 0 : (b > 255 ? 255 : b);
        c = y1 - 16;
        r = (298 * c + 409 * e + 128) >> 8;
        g = (298 * c - 100 * d - 208 * e + 128) >> 8;
        b = (298 * c + 516 * d + 128) >> 8;
        rgb[i * 6 + 3] = r < 0 ? 0 : (r > 255 ? 255 : r);
        rgb[i * 6 + 4] = g < 0 ? 0 : (g > 255 ? 255 : g);
        rgb[i * 6 + 5] = b < 0 ? 0 : (b > 255 ? 255 : b);
    }
}

int init_camera(int *fd, void* buffers[], unsigned int *num_buffers, const char *device, int width, int height) {
#if ENABLE_AR
    *fd = open(device, O_RDWR);
    if (*fd == -1) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Warning: Failed to open camera at %s", device);
        log_to_file(msg);
        return 0;
    }

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(*fd, VIDIOC_S_FMT, &fmt) == -1) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Warning: Failed to set camera format for %s", device);
        log_to_file(msg);
        close(*fd);
        *fd = -1;
        return 0;
    }

    struct v4l2_requestbuffers req = {0};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(*fd, VIDIOC_REQBUFS, &req) == -1) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Warning: Failed to request buffers for %s", device);
        log_to_file(msg);
        close(*fd);
        *fd = -1;
        return 0;
    }
    *num_buffers = req.count;

    for (unsigned int i = 0; i < *num_buffers; i++) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(*fd, VIDIOC_QUERYBUF, &buf) == -1) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Warning: Failed to query buffer for %s", device);
            log_to_file(msg);
            close(*fd);
            *fd = -1;
            return 0;
        }
        buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, buf.m.offset);
        if (buffers[i] == MAP_FAILED) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Warning: Failed to map buffer for %s", device);
            log_to_file(msg);
            close(*fd);
            *fd = -1;
            return 0;
        }
    }

    for (unsigned int i = 0; i < *num_buffers; i++) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(*fd, VIDIOC_QBUF, &buf) == -1) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Warning: Failed to queue buffer for %s", device);
            log_to_file(msg);
            close(*fd);
            *fd = -1;
            return 0;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(*fd, VIDIOC_STREAMON, &type) == -1) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Warning: Failed to start streaming for %s", device);
        log_to_file(msg);
        close(*fd);
        *fd = -1;
        return 0;
    }

    return 1;
#else
    return 0;
#endif
}

void update_camera(int fd, void* buffers[], unsigned int num_buffers, GLuint texture_id, int width, int height) {
#if ENABLE_AR
    if (fd == -1) return;

    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
        log_to_file("Warning: Failed to dequeue buffer");
        return;
    }

    unsigned char* rgb = malloc(width * height * 3);
    if (!rgb) {
        log_to_file("Error: Memory allocation failed for camera RGB buffer");
        exit(1);
    }
    yuyv_to_rgb(buffers[buf.index], rgb, width, height);

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    free(rgb);
    if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
        log_to_file("Warning: Failed to requeue buffer");
    }
#endif
}

void cleanup_camera(int *fd, void* buffers[], unsigned int num_buffers, int *enabled, int width, int height) {
#if ENABLE_AR
    if (!*enabled) return;

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(*fd, VIDIOC_STREAMOFF, &type);
    for (unsigned int i = 0; i < num_buffers; i++) {
        munmap(buffers[i], width * height * 2);
    }
    close(*fd);
    *fd = -1;
    *enabled = 0;
#endif
}

void free_maps() {
    for (int i = 0; i < num_maps; i++) {
        if (map_display_lists && glut_initialized) {
            glDeleteLists(map_display_lists[i], 1);
        }
        for (int y = 0; y < map_sizes[i*2 + 1]; y++) {
            free(maps[i][y]);
        }
        free(maps[i]);
    }
    if (map_display_lists) {
        free(map_display_lists);
        map_display_lists = NULL;
    }
    free(maps);
    free(map_coords);
    free(map_sizes);
    maps = NULL;
    map_coords = NULL;
    map_sizes = NULL;
    num_maps = 0;
    map_capacity = 0;
    last_map_load_time = 0;
}

void resize_maps() {
    if (num_maps >= map_capacity) {
        int new_capacity = map_capacity == 0 ? INITIAL_MAP_CAPACITY : map_capacity * 2;
        char ***new_maps = realloc(maps, new_capacity * sizeof(char **));
        int *new_coords = realloc(map_coords, new_capacity * 3 * sizeof(int));
        int *new_sizes = realloc(map_sizes, new_capacity * 2 * sizeof(int));
        if (!new_maps || !new_coords || !new_sizes) {
            log_to_file("Error: Memory allocation failed in resize_maps");
            exit(1);
        }
        maps = new_maps;
        map_coords = new_coords;
        map_sizes = new_sizes;
        map_capacity = new_capacity;
    }
}

void resize_super_state_pieces(int needed) {
    if (num_super_state_pieces + needed >= super_state_piece_capacity) {
        int new_capacity = super_state_piece_capacity == 0 ? 10 : super_state_piece_capacity * 2;
        while (new_capacity < num_super_state_pieces + needed) new_capacity *= 2;
        int *new_x = realloc(super_state_x, new_capacity * sizeof(int));
        int *new_y = realloc(super_state_y, new_capacity * sizeof(int));
        int *new_z = realloc(super_state_z, new_capacity * sizeof(int));
        char **new_symbol = realloc(super_state_symbol, new_capacity * sizeof(char *));
        float *new_r = realloc(super_state_r, new_capacity * sizeof(float));
        float *new_g = realloc(super_state_g, new_capacity * sizeof(float));
        float *new_b = realloc(super_state_b, new_capacity * sizeof(float));
        int *new_has_color = realloc(super_state_has_color, new_capacity * sizeof(int));
        float *new_size = realloc(super_state_size, new_capacity * sizeof(float));
        int *new_shape = realloc(super_state_shape, new_capacity * sizeof(int));
        if (!new_x || !new_y || !new_z || !new_symbol || !new_r || !new_g || !new_b || !new_has_color || !new_size || !new_shape) {
            log_to_file("Error: Memory allocation failed in resize_super_state_pieces");
            exit(1);
        }
        if (new_symbol) {
            for (int i = super_state_piece_capacity; i < new_capacity; i++) {
                new_symbol[i] = malloc(16 * sizeof(char));
                if (!new_symbol[i]) {
                    log_to_file("Error: Memory allocation failed for symbol array");
                    exit(1);
                }
                new_symbol[i][0] = '\0';
            }
        }
        super_state_x = new_x;
        super_state_y = new_y;
        super_state_z = new_z;
        super_state_symbol = new_symbol;
        super_state_r = new_r;
        super_state_g = new_g;
        super_state_b = new_b;
        super_state_has_color = new_has_color;
        super_state_size = new_size;
        super_state_shape = new_shape;
        super_state_piece_capacity = new_capacity;
    }
}

void resize_command_lines() {
    int new_capacity = command_capacity == 0 ? INITIAL_COMMAND_CAPACITY : command_capacity * 2;
    if (new_capacity > MAX_COMMAND_LINES) new_capacity = MAX_COMMAND_LINES;
    char (*new_lines)[MAX_LINE] = realloc(command_lines, new_capacity * sizeof(char[MAX_LINE]));
    if (!new_lines) {
        log_to_file("Error: Memory allocation failed in resize_command_lines");
        exit(1);
    }
    command_lines = new_lines;
    command_capacity = new_capacity;
}

void read_commands() {
    num_command_lines = 0;
    FILE *fp = fopen(terminal_input_path, "r");
    if (!fp) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to open terminal input file %s", terminal_input_path);
        log_to_file(msg);
        return;
    }

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (num_command_lines >= MAX_COMMAND_LINES) {
            log_to_file("Warning: MAX_COMMAND_LINES reached, ignoring additional lines");
            break;
        }
        if (num_command_lines >= command_capacity) {
            resize_command_lines();
        }
        strncpy(command_lines[num_command_lines], line, MAX_LINE - 1);
        command_lines[num_command_lines][MAX_LINE - 1] = '\0';
        num_command_lines++;
    }
    fclose(fp);

    char msg[256];
    snprintf(msg, sizeof(msg), "Loaded %d command lines from %s", num_command_lines, terminal_input_path);
    log_to_file(msg);
}

void draw_terminal() {
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1, 0, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float term_width = 0.75f;
    float line_spacing = 0.25f / 12.0f;
    int max_display_lines = 10;
    float term_height = line_spacing * (num_command_lines < max_display_lines ? num_command_lines : max_display_lines);
    float term_x = 0.0125f;
    float term_y = 0.0167f;

    // Draw terminal background
    glColor4f(0.5f, 0.0f, 0.5f, 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(term_x, term_y);
    glVertex2f(term_x + term_width, term_y);
    glVertex2f(term_x + term_width, term_y + term_height);
    glVertex2f(term_x, term_y + term_height);
    glEnd();

    // Draw scrollbar track
    float scroll_width = 0.02f; // Scrollbar width (2% of window width)
    float scroll_x = term_x + term_width; // Right of terminal
    glColor4f(0.3f, 0.3f, 0.3f, 0.7f); // Gray track
    glBegin(GL_QUADS);
    glVertex2f(scroll_x, term_y);
    glVertex2f(scroll_x + scroll_width, term_y);
    glVertex2f(scroll_x + scroll_width, term_y + term_height);
    glVertex2f(scroll_x, term_y + term_height);
    glEnd();

    // Draw scrollbar thumb
    if (num_command_lines > max_display_lines) {
        float thumb_height = term_height * ((float)max_display_lines / num_command_lines);
        float max_scroll = num_command_lines - max_display_lines;
        float thumb_y = term_y + (term_height - thumb_height) * ((float)scroll_offset / max_scroll);
        glColor4f(0.7f, 0.7f, 0.7f, 0.9f); // Light gray thumb
        glBegin(GL_QUADS);
        glVertex2f(scroll_x, thumb_y);
        glVertex2f(scroll_x + scroll_width, thumb_y);
        glVertex2f(scroll_x + scroll_width, thumb_y + thumb_height);
        glVertex2f(scroll_x, thumb_y + thumb_height);
        glEnd();
    }

    // Draw text
    void *font = GLUT_BITMAP_8_BY_13;
    if (window_width > 1200 || window_height > 900) {
        font = GLUT_BITMAP_9_BY_15;
    } else if (window_width < 600 || window_height < 450) {
        font = GLUT_BITMAP_HELVETICA_10;
    }

    glColor3f(1.0f, 1.0f, 1.0f);
    int max_lines = num_command_lines < max_display_lines ? num_command_lines : max_display_lines;
    float text_x = term_x + 0.01f;
    float char_width = 0.008f;
    float emoji_scale = (window_width > 1200 || window_height > 900) ? 24.0f : (window_width < 600 || window_height < 450) ? 16.0f : 20.0f;

    for (int i = 0; i < max_lines; i++) {
        int idx = num_command_lines - 1 - scroll_offset - (max_lines - 1 - i);
        if (idx < 0) break;
        float current_x = text_x;
        float current_y = term_y + term_height - (i + 1) * line_spacing;
        
        unsigned int codepoints[MAX_LINE];
        int emoji_count = getEmojiCodepoints(command_lines[idx], codepoints, MAX_LINE);
        const char *c = command_lines[idx];
        int emoji_idx = 0;

        while (*c) {
            unsigned char ch = *c;
            int bytes = 0;
            unsigned int codepoint = 0;

            if (ch < 0x80) {
                codepoint = ch;
                bytes = 1;
            } else if ((ch & 0xE0) == 0xC0) {
                codepoint = ch & 0x1F;
                bytes = 2;
            } else if ((ch & 0xF0) == 0xE0) {
                codepoint = ch & 0x0F;
                bytes = 3;
            } else if ((ch & 0xF8) == 0xF0) {
                codepoint = ch & 0x07;
                bytes = 4;
            }

            if (bytes > 1) {
                for (int j = 1; j < bytes; j++) {
                    if ((c[j] & 0xC0) != 0x80) {
                        bytes = 0;
                        break;
                    }
                    codepoint = (codepoint << 6) | (c[j] & 0x3F);
                }
            }

            if (bytes > 0 && emoji_idx < emoji_count && codepoint == codepoints[emoji_idx] && codepoint >= 0x1F300 && codepoint <= 0x1FAD6) {
                renderEmoji(codepoint, current_x, current_y - line_spacing * 0.25f, emoji_scale);
                current_x += char_width * 1.2f;
                emoji_idx++;
                c += bytes;
            } else {
                glRasterPos2f(current_x, current_y);
                glutBitmapCharacter(font, *c);
                current_x += char_width;
                c++;
            }
        }
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}

int check_commands_update() {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    long elapsed_ns = (current_time.tv_sec - last_command_check.tv_sec) * 1000000000L +
                      (current_time.tv_nsec - last_command_check.tv_nsec);
    if (elapsed_ns < FILE_CHECK_INTERVAL * 1000) {
        return 0;
    }
    last_command_check = current_time;

    struct stat stat_buf;
    if (stat(terminal_input_path, &stat_buf) == 0) {
        if (stat_buf.st_mtime > last_command_time) {
            last_command_time = stat_buf.st_mtime;
            log_to_file("Terminal input file changed, reloading");
            return 1;
        }
    } else {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to stat terminal input file %s", terminal_input_path);
        log_to_file(msg);
    }
    return 0;
}

int parse_filename(const char *filename, int *map_x, int *map_y, int *map_z) {
    if (strncmp(filename, "x", 1) != 0) return 0;
    char *end;
    *map_x = strtol(filename + 1, &end, 10);
    if (*end != 'y') return 0;
    *map_y = strtol(end + 1, &end, 10);
    if (*end != 'z') return 0;
    *map_z = strtol(end + 1, &end, 10);
    if (strncmp(end, "_layer1.txt", 11) != 0) return 0;
    return 1;
}

void parse_state_file(const char *filename, int is_state_file, int *x, int *y, int *z, int *cam_x, int *cam_y, int *cam_z, int *cursor_pos, char *piece_symbol, int *has_coords) {
    if (has_coords) *has_coords = 0; // Initialize flag
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to open state file %s", filename);
        log_to_file(msg);
        return;
    }

    // Temporary storage for pieces
    int temp_x[MAX_PIECES_PER_STATE];
    int temp_y[MAX_PIECES_PER_STATE];
    int temp_z[MAX_PIECES_PER_STATE];
    char temp_symbol[MAX_PIECES_PER_STATE][16];
    int temp_piece_count = 0;

    // First pass: collect pieces and state data
    char line[MAX_LINE];
    char current_symbol[16] = "P";
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        char key[MAX_LINE];
        if (sscanf(line, "%s", key) != 1) continue;
        if (strcmp(key, "color") == 0 && is_state_file) {
            float r, g, b;
            if (sscanf(line, "%*s %f %f %f", &r, &g, &b) == 3) {
                player_r = r;
                player_g = g;
                player_b = b;
            }
        } else if (strcmp(key, "symbol") == 0) {
            char symbol[16];
            if (sscanf(line, "%*s %15[^\n]", symbol) == 1) {
                strncpy(current_symbol, symbol, 15);
                current_symbol[15] = '\0';
                if (is_state_file) {
                    strncpy(piece_symbol, symbol, 15);
                    piece_symbol[15] = '\0';
                }
            }
        } else if (strcmp(key, "piece") == 0 && temp_piece_count < MAX_PIECES_PER_STATE) {
            int px, py, pz;
            if (sscanf(line, "%*s %d %d %d", &px, &py, &pz) == 3) {
                temp_x[temp_piece_count] = px;
                temp_y[temp_piece_count] = py;
                temp_z[temp_piece_count] = pz;
                strncpy(temp_symbol[temp_piece_count], current_symbol, 15);
                temp_symbol[temp_piece_count][15] = '\0';
                temp_piece_count++;
            }
        } else if (is_state_file) {
            if (strcmp(key, "x") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) {
                    *x = value;
                    if (has_coords) (*has_coords) |= 1; // Set x bit
                }
            } else if (strcmp(key, "y") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) {
                    *y = value;
                    if (has_coords) (*has_coords) |= 2; // Set y bit
                }
            } else if (strcmp(key, "z") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) {
                    *z = value;
                    if (has_coords) (*has_coords) |= 4; // Set z bit
                }
            } else if (strcmp(key, "cam_x") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) *cam_x = value;
            } else if (strcmp(key, "cam_y") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) *cam_y = value;
            } else if (strcmp(key, "cam_z") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) *cam_z = value;
            } else if (strcmp(key, "cursor_x") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) cursor_pos[0] = value;
            } else if (strcmp(key, "cursor_y") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) cursor_pos[1] = value;
            } else if (strcmp(key, "cursor_z") == 0) {
                int value;
                if (sscanf(line, "%*s %d", &value) == 1) cursor_pos[2] = value;
            }
        }
    }

    // Second pass: apply colors, sizes, and shapes
    rewind(fp);
    float current_r = 1.0f, current_g = 0.8f, current_b = 0.8f;
    int current_has_color = 0;
    float current_size = DEFAULT_PIECE_SIZE;
    int current_shape = SHAPE_SPHERE;
    while (fgets(line, MAX_LINE, fp)) {
        line[strcspn(line, "\n")] = '\0';
        char key[MAX_LINE];
        if (sscanf(line, "%s", key) != 1) continue;
        if (strcmp(key, "symbol") == 0) {
            char symbol[16];
            if (sscanf(line, "%*s %15[^\n]", symbol) == 1) {
                strncpy(current_symbol, symbol, 15);
                current_symbol[15] = '\0';
            }
        } else if (strcmp(key, "color") == 0) {
            float r, g, b;
            if (sscanf(line, "%*s %f %f %f", &r, &g, &b) == 3) {
                current_r = r;
                current_g = g;
                current_b = b;
                current_has_color = 1;
            }
        } else if (strcmp(key, "size") == 0) {
            float size;
            if (sscanf(line, "%*s %f", &size) == 1) {
                current_size = size;
            }
        } else if (strcmp(key, "shape") == 0) {
            char shape[16];
            if (sscanf(line, "%*s %15s", shape) == 1) {
                if (strcmp(shape, "cube") == 0) {
                    current_shape = SHAPE_CUBE;
                } else if (strcmp(shape, "sphere") == 0) {
                    current_shape = SHAPE_SPHERE;
                }
            }
        }
    }

    // Store pieces in super_state arrays
    for (int i = 0; i < temp_piece_count; i++) {
        resize_super_state_pieces(1);
        super_state_x[num_super_state_pieces] = temp_x[i];
        super_state_y[num_super_state_pieces] = temp_y[i];
        super_state_z[num_super_state_pieces] = temp_z[i];
        strncpy(super_state_symbol[num_super_state_pieces], temp_symbol[i], 15);
        super_state_symbol[num_super_state_pieces][15] = '\0';
        super_state_r[num_super_state_pieces] = current_has_color && strcmp(temp_symbol[i], current_symbol) == 0 ? current_r : 1.0f;
        super_state_g[num_super_state_pieces] = current_has_color && strcmp(temp_symbol[i], current_symbol) == 0 ? current_g : 0.8f;
        super_state_b[num_super_state_pieces] = current_has_color && strcmp(temp_symbol[i], current_symbol) == 0 ? current_b : 0.8f;
        super_state_has_color[num_super_state_pieces] = current_has_color && strcmp(temp_symbol[i], current_symbol) == 0 ? 1 : 0;
        super_state_size[num_super_state_pieces] = strcmp(temp_symbol[i], current_symbol) == 0 ? current_size : DEFAULT_PIECE_SIZE;
        super_state_shape[num_super_state_pieces] = strcmp(temp_symbol[i], current_symbol) == 0 ? current_shape : SHAPE_SPHERE;
        if (is_state_file) {
            for (int j = 0; j < num_maps; j++) {
                int mx = map_coords[j*3 + 0];
                int my = map_coords[j*3 + 1];
                int mz = map_coords[j*3 + 2];
                int width = map_sizes[j*2 + 0];
                int height = map_sizes[j*2 + 1];
                if (temp_z[i] == mz && temp_x[i] >= mx && temp_x[i] < mx + width && temp_y[i] >= my && temp_y[i] < my + height) {
                    maps[j][temp_y[i] - my][temp_x[i] - mx] = temp_symbol[i][0];
                }
            }
        }
        char debug_msg[256];
        snprintf(debug_msg, sizeof(debug_msg), "Piece %d: symbol=%s, x=%d, y=%d, z=%d, color=(%.1f, %.1f, %.1f), has_color=%d, size=%.2f, shape=%s",
                 num_super_state_pieces, super_state_symbol[num_super_state_pieces],
                 super_state_x[num_super_state_pieces], super_state_y[num_super_state_pieces],
                 super_state_z[num_super_state_pieces], super_state_r[num_super_state_pieces],
                 super_state_g[num_super_state_pieces], super_state_b[num_super_state_pieces],
                 super_state_has_color[num_super_state_pieces], super_state_size[num_super_state_pieces],
                 super_state_shape[num_super_state_pieces] == SHAPE_CUBE ? "cube" : "sphere");
        log_to_file(debug_msg);
        num_super_state_pieces++;
    }

    fclose(fp);
    char msg[MAX_PATH + 50];
    snprintf(msg, sizeof(msg), "Loaded %d pieces from %s", temp_piece_count, filename);
    log_to_file(msg);
}

void read_maps() {
    free_maps();
    DIR *dir = opendir(maps_dir);
    if (!dir) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Error: Failed to open maps directory %s", maps_dir);
        log_to_file(msg);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_REG) continue;
        int map_x, map_y, map_z;
        if (!parse_filename(entry->d_name, &map_x, &map_y, &map_z)) continue;

        if (num_maps >= MAX_MAPS) {
            log_to_file("Error: Maximum number of maps reached");
            closedir(dir);
            return;
        }

        char filename[MAX_PATH];
        snprintf(filename, MAX_PATH, "%s%s", maps_dir, entry->d_name);
        FILE *fp = fopen(filename, "r");
        if (!fp) {
            char msg[MAX_PATH + 50];
            snprintf(msg, sizeof(msg), "Warning: Failed to open map file %s", filename);
            log_to_file(msg);
            continue;
        }

        int width = 0, height = 0;
        char line[MAX_LINE];
        while (fgets(line, MAX_LINE, fp)) {
            line[strcspn(line, "\n")] = '\0';
            int len = strlen(line);
            if (len > width) width = len;
            height++;
        }
        if (width > MAX_MAP_SIZE || height > MAX_MAP_SIZE) {
            log_to_file("Error: Map dimensions exceed maximum allowed size");
            fclose(fp);
            continue;
        }
        rewind(fp);

        resize_maps();
        char **map = malloc(height * sizeof(char *));
        if (!map) {
            log_to_file("Error: Memory allocation failed for map");
            fclose(fp);
            exit(1);
        }
        for (int y = 0; y < height; y++) {
            map[y] = malloc((width + 1) * sizeof(char));
            if (!map[y]) {
                log_to_file("Error: Memory allocation failed for map row");
                for (int j = 0; j < y; j++) free(map[j]);
                free(map);
                fclose(fp);
                exit(1);
            }
            for (int x = 0; x < width; x++) {
                map[y][x] = '.';
            }
            map[y][width] = '\0';
        }

        int row = 0;
        while (row < height && fgets(line, MAX_LINE, fp)) {
            line[strcspn(line, "\n")] = '\0';
            int len = strlen(line);
            if (len > width) len = width;
            for (int x = 0; x < len; x++) {
                map[row][x] = line[x];
            }
            row++;
        }
        fclose(fp);

        maps[num_maps] = map;
        map_coords[num_maps*3 + 0] = map_x;
        map_coords[num_maps*3 + 1] = map_y;
        map_coords[num_maps*3 + 2] = map_z;
        map_sizes[num_maps*2 + 0] = width;
        map_sizes[num_maps*2 + 1] = height;
        num_maps++;
    }
    closedir(dir);
}

void read_state() {
    int temp_x, temp_y, temp_z;
    int has_coords = 0;
    parse_state_file(state_path, 1, &temp_x, &temp_y, &temp_z, &cam_x, &cam_y, &cam_z, cursor_pos, piece_symbol, &has_coords);
    if (has_coords == 7) { // All coordinates (x, y, z) present
        player_x = temp_x;
        player_y = temp_y;
        player_z = temp_z;
        player_coords_valid = 1;
    } else {
        player_coords_valid = 0;
    }
}
void read_super_state() {
    if (super_states_dir[0] == '\0') {
        log_to_file("super_states directory not specified in locations.txt, skipping");
        return;
    }

    DIR *dir = opendir(super_states_dir);
    if (!dir) {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to open super_states directory %s", super_states_dir);
        log_to_file(msg);
        return;
    }

    // Clear existing super state pieces
    for (int i = 0; i < num_super_state_pieces; i++) {
        free(super_state_symbol[i]);
    }
    free(super_state_x);
    free(super_state_y);
    free(super_state_z);
    free(super_state_symbol);
    free(super_state_r);
    free(super_state_g);
    free(super_state_b);
    free(super_state_has_color);
    free(super_state_size);
    free(super_state_shape);
    super_state_x = NULL;
    super_state_y = NULL;
    super_state_z = NULL;
    super_state_symbol = NULL;
    super_state_r = NULL;
    super_state_g = NULL;
    super_state_b = NULL;
    super_state_has_color = NULL;
    super_state_size = NULL;
    super_state_shape = NULL;
    num_super_state_pieces = 0;
    super_state_piece_capacity = 0;

    struct dirent *entry;
    int file_count = 0;
    regex_t regex;
    if (regcomp(&regex, "^state_.*\\.txt$", REG_EXTENDED) != 0) {
        log_to_file("Error: Failed to compile regex for super_states files");
        closedir(dir);
        return;
    }

    while ((entry = readdir(dir)) && file_count < MAX_SUPER_STATES) {
        if (entry->d_type != DT_REG) continue;
        if (regexec(&regex, entry->d_name, 0, NULL, 0) != 0) continue;

        char filename[MAX_PATH];
        snprintf(filename, MAX_PATH, "%s%s", super_states_dir, entry->d_name);
        parse_state_file(filename, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        file_count++;
    }
    regfree(&regex);
    closedir(dir);

    char msg[256];
    snprintf(msg, sizeof(msg), "Total super_states pieces loaded: %d", num_super_state_pieces);
    log_to_file(msg);
}

void check_maps_update() {
    struct stat dir_stat;
    if (stat(maps_dir, &dir_stat) == 0) {
        if (dir_stat.st_mtime > last_map_load_time) {
            read_maps();
            if (glut_initialized) {
                init_map_display_lists();
            }
            last_map_load_time = dir_stat.st_mtime;
            log_to_file("Maps reloaded due to directory change");
        }
    } else {
        char msg[MAX_PATH + 50];
        snprintf(msg, sizeof(msg), "Warning: Failed to stat maps directory %s", maps_dir);
        log_to_file(msg);
    }
}

void init_map_display_lists() {
    if (!glut_initialized) {
        log_to_file("Warning: Skipping init_map_display_lists; GLUT not initialized");
        return;
    }
    if (map_display_lists) {
        for (int i = 0; i < num_maps; i++) {
            glDeleteLists(map_display_lists[i], 1);
        }
        free(map_display_lists);
    }
    map_display_lists = malloc(num_maps * sizeof(GLuint));
    if (!map_display_lists) {
        log_to_file("Error: Memory allocation failed for display lists");
        exit(1);
    }
    for (int i = 0; i < num_maps; i++) {
        map_display_lists[i] = glGenLists(1);
        glNewList(map_display_lists[i], GL_COMPILE);
        int mx = map_coords[i*3 + 0];
        int my = map_coords[i*3 + 1];
        int mz = map_coords[i*3 + 2];
        int width = map_sizes[i*2 + 0];
        int height = map_sizes[i*2 + 1];
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (maps[i][y][x] == '#') {
                    glColor4f(0.5f, 0.5f, 0.5f, TRANSLUCENT_BLOCKS ? 0.3f : 1.0f);
                    glPushMatrix();
                    glTranslatef(mx + x + 0.5f, my + y + 0.5f, mz + 0.5f);
                    glutSolidCube(1.0);
                    glPopMatrix();
                } else if (maps[i][y][x] == piece_symbol[0]) {
                    int found = 0;
                    for (int j = 0; j < num_super_state_pieces; j++) {
                        if (super_state_x[j] == mx + x &&
                            super_state_y[j] == my + y &&
                            super_state_z[j] == mz &&
                            strcmp(super_state_symbol[j], piece_symbol) == 0) {
                            if (super_state_has_color[j]) {
                                glColor3f(super_state_r[j], super_state_g[j], super_state_b[j]);
                            } else {
                                glColor3f(1.0f, 0.8f, 0.8f);
                            }
                            glPushMatrix();
                            glTranslatef(mx + x + 0.5f, my + y + 0.5f, mz + 0.5f);
                            if (super_state_shape[j] == SHAPE_CUBE) {
                                glutSolidCube(super_state_size[j] * 2.0f);
                            } else {
                                glutSolidSphere(super_state_size[j], 16, 16);
                            }
                            glPopMatrix();
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        glColor3f(1.0f, 0.8f, 0.8f);
                        glPushMatrix();
                        glTranslatef(mx + x + 0.5f, my + y + 0.5f, mz + 0.5f);
                        glutSolidSphere(DEFAULT_PIECE_SIZE, 16, 16);
                        glPopMatrix();
                    }
                }
            }
        }
        glDisable(GL_BLEND);
        glEndList();
    }
}

void draw_cube(float x, float y, float z, float size) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glutSolidCube(size);
    glPopMatrix();
}

void draw_sphere(float x, float y, float z, float size) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glutSolidSphere(size, 16, 16);
    glPopMatrix();
}

void draw_cursor(float x, float y, float z) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 0.0f, 0.3f);
    glPushMatrix();
    glTranslatef(x + 0.5f, y + 0.5f, z + 0.5f);
    glScalef(1.1f, 1.1f, 1.1f);
    glutSolidCube(1.0);
    glPopMatrix();
    glDisable(GL_BLEND);
}

void draw_text(float x, float y, const char *string) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, window_width, 0, window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glRasterPos2f(x, y);
    for (const char *c = string; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void draw_mini_map() {
    glPushAttrib(GL_VIEWPORT_BIT | GL_ENABLE_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    float cx = floor((float)cursor_pos[0]);
    float cy = floor((float)cursor_pos[1]);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Mini-map: Raw cursor pos: (%d, %d, %d), Snapped: (%.0f, %.0f)", 
             cursor_pos[0], cursor_pos[1], cursor_pos[2], cx, cy);
    log_to_file(log_msg);

    int mini_size = (window_width < window_height ? window_width : window_height) * 0.2;
    glViewport(window_width - mini_size, window_height - mini_size, mini_size, mini_size);

    float half_view = MINI_MAP_VIEW_SIZE / 2.0f;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(cx - half_view, cx + half_view, cy - half_view, cy + half_view, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(cx - half_view, cy - half_view);
    glVertex2f(cx + half_view, cy - half_view);
    glVertex2f(cx + half_view, cy + half_view);
    glVertex2f(cx - half_view, cy + half_view);
    glEnd();
    glDisable(GL_BLEND);

    int min_z_diff = INT_MAX;
    int closest_z_map_idx = -1;
    for (int i = 0; i < num_maps; i++) {
        int mz = map_coords[i*3 + 2];
        int z_diff = abs(cursor_pos[2] - mz);
        if (z_diff < min_z_diff) {
            min_z_diff = z_diff;
            closest_z_map_idx = i;
        }
    }

    if (closest_z_map_idx >= 0) {
        for (int i = 0; i < num_maps; i++) {
            int mx = map_coords[i*3 + 0];
            int my = map_coords[i*3 + 1];
            int mz = map_coords[i*3 + 2];
            int width = map_sizes[i*2 + 0];
            int height = map_sizes[i*2 + 1];

            if (mz != map_coords[closest_z_map_idx*3 + 2]) continue;

            if (mx + width < cx - half_view || mx > cx + half_view ||
                my + height < cy - half_view || my > cy + half_view) {
                continue;
            }

            snprintf(log_msg, sizeof(log_msg), "Mini-map: Rendering map %d: x=[%d,%d), y=[%d,%d), z=%d", i, mx, mx + width, my, my + height, mz);
            log_to_file(log_msg);

            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    float world_x = mx + x;
                    float world_y = my + y;
                    if (world_x < cx - half_view || world_x >= cx + half_view ||
                        world_y < cy - half_view || world_y >= cy + half_view) {
                        continue;
                    }
                    if (maps[i][y][x] == '#') {
                        glColor3f(0.5f, 0.5f, 0.5f);
                        glBegin(GL_QUADS);
                        glVertex2f(world_x + 0.1, world_y + 0.1);
                        glVertex2f(world_x + 0.9, world_y + 0.1);
                        glVertex2f(world_x + 0.9, world_y + 0.9);
                        glVertex2f(world_x + 0.1, world_y + 0.9);
                        glEnd();
                    } else if (maps[i][y][x] == piece_symbol[0]) {
                        int found = 0;
                        for (int j = 0; j < num_super_state_pieces; j++) {
                            if (super_state_x[j] == mx + x &&
                                super_state_y[j] == my + y &&
                                super_state_z[j] == mz &&
                                strcmp(super_state_symbol[j], piece_symbol) == 0) {
                                if (super_state_has_color[j]) {
                                    glColor3f(super_state_r[j], super_state_g[j], super_state_b[j]);
                                } else {
                                    glColor3f(1.0f, 0.8f, 0.8f);
                                }
                                float quad_size = super_state_size[j] * 2.0f;
                                float half_size = quad_size / 2.0f;
                                glBegin(GL_QUADS);
                                glVertex2f(world_x + 0.5f - half_size, world_y + 0.5f - half_size);
                                glVertex2f(world_x + 0.5f + half_size, world_y + 0.5f - half_size);
                                glVertex2f(world_x + 0.5f + half_size, world_y + 0.5f + half_size);
                                glVertex2f(world_x + 0.5f - half_size, world_y + 0.5f + half_size);
                                glEnd();
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            glColor3f(1.0f, 0.8f, 0.8f);
                            glBegin(GL_QUADS);
                            glVertex2f(world_x + 0.2, world_y + 0.2);
                            glVertex2f(world_x + 0.8, world_y + 0.2);
                            glVertex2f(world_x + 0.8, world_y + 0.8);
                            glVertex2f(world_x + 0.2, world_y + 0.8);
                            glEnd();
                        }
                    }
                }
            }
        }
    } else {
        log_to_file("Mini-map: No maps loaded");
    }

    for (int i = 0; i < num_super_state_pieces; i++) {
        float world_x = super_state_x[i];
        float world_y = super_state_y[i];
        if (world_x < cx - half_view || world_x >= cx + half_view ||
            world_y < cy - half_view || world_y >= cy + half_view) {
            continue;
        }
        if (super_state_has_color[i]) {
            glColor3f(super_state_r[i], super_state_g[i], super_state_b[i]);
        } else {
            glColor3f(0.2f, 0.8f, 0.2f);
        }
        float quad_size = super_state_size[i] * 2.0f;
        float half_size = quad_size / 2.0f;
        glBegin(GL_QUADS);
        glVertex2f(world_x + 0.5f - half_size, world_y + 0.5f - half_size);
        glVertex2f(world_x + 0.5f + half_size, world_y + 0.5f - half_size);
        glVertex2f(world_x + 0.5f + half_size, world_y + 0.5f + half_size);
        glVertex2f(world_x + 0.5f - half_size, world_y + 0.5f + half_size);
        glEnd();
    }

       if (player_coords_valid) {
        glColor3f(player_r, player_g, player_b);
        glBegin(GL_QUADS);
        glVertex2f(player_x + 0.2, player_y + 0.2);
        glVertex2f(player_x + 0.8, player_y + 0.2);
        glVertex2f(player_x + 0.8, player_y + 0.8);
        glVertex2f(player_x + 0.2, player_y + 0.8);
        glEnd();
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 0.0f, 0.3f);
    glBegin(GL_QUADS);
    glVertex2f(cx + 0.1, cy + 0.1);
    glVertex2f(cx + 0.9, cy + 0.1);
    glVertex2f(cx + 0.9, cy + 0.9);
    glVertex2f(cx + 0.1, cy + 0.9);
    glEnd();
    glDisable(GL_BLEND);

    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx - half_view + 0.05, cy - half_view + 0.05);
    glVertex2f(cx + half_view - 0.05, cy - half_view + 0.05);
    glVertex2f(cx + half_view - 0.05, cy + half_view - 0.05);
    glVertex2f(cx - half_view + 0.05, cy + half_view - 0.05);
    glEnd();

    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}

int check_memory_usage() {
    FILE *fp = fopen("/proc/self/statm", "r");
    if (!fp) {
        log_to_file("Warning: Failed to read /proc/self/statm");
        return 0;
    }
    unsigned long resident;
    fscanf(fp, "%*ld %ld", &resident);
    fclose(fp);
    unsigned long memory_mb = resident * 4 / 1024;
    if (memory_mb > 4000) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Error: Memory usage exceeds 4GB (%ld MB), exiting", memory_mb);
        log_to_file(msg);
        exit(1);
    }
    return 1;
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    check_memory_usage();
    read_state();
    read_super_state();
    check_maps_update();

#if ENABLE_AR
    update_camera(ar_camera_fd, ar_camera_buffers, ar_num_buffers, ar_texture_id, ar_frame_width, ar_frame_height);
    if (ar_camera_enabled) {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, 1, 0, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glEnable(GL_DEPTH_TEST); // Keep depth test enabled
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindTexture(GL_TEXTURE_2D, ar_texture_id);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex3f(0, 0, -0.1); // z=-0.1 to place behind terminal
        glTexCoord2f(1, 1); glVertex3f(1, 0, -0.1);
        glTexCoord2f(1, 0); glVertex3f(1, 1, -0.1);
        glTexCoord2f(0, 0); glVertex3f(0, 1, -0.1);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
#endif

    if (terminal_enabled) {
        if (check_commands_update()) {
            read_commands();
        }
        draw_terminal();
    }

    // Reset depth buffer and OpenGL state for 3D rendering
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)window_width / window_height, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

/* previous camera
    gluLookAt(cam_x, cam_y, cam_z, player_x, player_y, player_z, 0, 0, 1);
    */
    // Define camera offset (e.g., 5 units behind, 3 units above, 2 units to the side)
float camera_offset_x = -5.0f; // Behind the player
float camera_offset_y = 0.0f;  // No lateral offset
float camera_offset_z = 3.0f;  // Above the player

// Compute camera position based on player position
float camera_x = player_x + camera_offset_x;
float camera_y = player_y + camera_offset_y;
float camera_z = player_z + camera_offset_z;

// Set camera to look at the player
gluLookAt(camera_x, camera_y, camera_z, 
          player_x + 0.5f, player_y + 0.5f, player_z + 0.5f, 
          0, 0, 1);
          ////////////////////////

    for (int i = 0; i < num_maps; i++) {
        glCallList(map_display_lists[i]);
    }

  if (player_coords_valid) {
        glColor3f(player_r, player_g, player_b);
        draw_sphere(player_x + 0.5f, player_y + 0.5f, player_z + 0.5f, DEFAULT_PIECE_SIZE);
    }

    for (int i = 0; i < num_super_state_pieces; i++) {
        if (super_state_has_color[i]) {
            glColor3f(super_state_r[i], super_state_g[i], super_state_b[i]);
        } else {
            glColor3f(0.2f, 0.8f, 0.2f);
        }
        if (super_state_shape[i] == SHAPE_CUBE) {
            draw_cube(super_state_x[i] + 0.5f, super_state_y[i] + 0.5f, super_state_z[i] + 0.5f, super_state_size[i] * 2.0f);
        } else {
            draw_sphere(super_state_x[i] + 0.5f, super_state_y[i] + 0.5f, super_state_z[i] + 0.5f, super_state_size[i]);
        }
    }

    if (cursor_pos[0] >= -1000 && cursor_pos[0] < 1000 && cursor_pos[1] >= -1000 && cursor_pos[1] < 1000 && cursor_pos[2] >= -1000 && cursor_pos[2] < 1000) {
        draw_cursor((float)cursor_pos[0], (float)cursor_pos[1], (float)cursor_pos[2]);
    }

    draw_mini_map();

    char coord_text[256];
    snprintf(coord_text, sizeof(coord_text), "Player: (%d, %d, %d)", player_x, player_y, player_z);
    draw_text(10, window_height - 20, coord_text);
    snprintf(coord_text, sizeof(coord_text), "Camera: (%d, %d, %d)", cam_x, cam_y, cam_z);
    draw_text(10, window_height - 40, coord_text);
    snprintf(coord_text, sizeof(coord_text), "Cursor: (%d, %d, %d)", cursor_pos[0], cursor_pos[1], cursor_pos[2]);
    draw_text(10, window_height - 60, coord_text);

    glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {
    FILE *fp = fopen("commands_gl.txt", "a");
    if (fp) {
        time_t now = time(NULL);
        char *timestamp = ctime(&now);
        timestamp[strcspn(timestamp, "\n")] = '\0'; // Remove newline from timestamp
        if (isprint(key)) {
            fprintf(fp, "[%s] Keyboard: key=%c, x=%d, y=%d\n", timestamp, key, x, y);
        } else {
            fprintf(fp, "[%s] Keyboard: key=%d, x=%d, y=%d\n", timestamp, (int)key, x, y);
        }
        fclose(fp);
    }
    // Rest of your keyboard handling code here
}

void special(int key, int x, int y) {
    char *key_str;
    switch (key) {
        case GLUT_KEY_LEFT: key_str = "LeftArrow"; break;
        case GLUT_KEY_RIGHT: key_str = "RightArrow"; break;
        case GLUT_KEY_UP: key_str = "UpArrow"; break;
        case GLUT_KEY_DOWN: key_str = "DownArrow"; break;
        case GLUT_KEY_PAGE_UP:
            key_str = "PageUp";
            if (scroll_offset < num_command_lines - 10) { // max_display_lines = 10
                scroll_offset++;
                glutPostRedisplay();
            }
            break;
        case GLUT_KEY_PAGE_DOWN:
            key_str = "PageDown";
            if (scroll_offset > 0) {
                scroll_offset--;
                glutPostRedisplay();
            }
            break;
        default: key_str = "UnknownSpecial"; break;
    }
    char details[64];
    snprintf(details, sizeof(details), "key=%s, x=%d, y=%d", key_str, x, y);
    log_input_to_file("SpecialKeyboard", details);
}


// Modified mouse function
void mouse(int button, int state, int x, int y) {
    static int last_y = 0;
    char *button_str;
    switch (button) {
        case GLUT_LEFT_BUTTON: button_str = "Left"; break;
        case GLUT_MIDDLE_BUTTON: button_str = "Middle"; break;
        case GLUT_RIGHT_BUTTON: button_str = "Right"; break;
        default: button_str = "Unknown"; break;
    }
    char *state_str = state == GLUT_DOWN ? "Down" : "Up";
    char details[64];
    snprintf(details, sizeof(details), "button=%s, state=%s, x=%d, y=%d", button_str, state_str, x, y);
    log_input_to_file("Mouse", details);

    // Scrollbar interaction
    float term_width = 0.75f;
    float term_height = 0.25f / 12.0f * (num_command_lines < 10 ? num_command_lines : 10);
    float scroll_x = 0.0125f + term_width;
    float scroll_width = 0.02f;
    float term_y = 0.0167f;
    float x_normalized = (float)x / window_width;
    float y_normalized = 1.0f - (float)y / window_height;

    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            if (x_normalized >= scroll_x && x_normalized <= scroll_x + scroll_width &&
                y_normalized >= term_y && y_normalized <= term_y + term_height) {
                dragging = 1;
                last_y = y;
            }
        } else if (state == GLUT_UP) {
            dragging = 0;
        }
    }
}

// New motion function
void motion(int x, int y) {
    static int last_y = 0;
    if (dragging) {
        int max_display_lines = 10;
        int max_scroll = num_command_lines - max_display_lines;
        if (max_scroll > 0) {
            float term_height = 0.25f / 12.0f * (num_command_lines < max_display_lines ? num_command_lines : max_display_lines);
            float thumb_height = term_height * ((float)max_display_lines / num_command_lines);
            int delta_y = last_y - y;
            float scroll_sensitivity = (float)max_scroll / (term_height - thumb_height);
            scroll_offset += (int)(delta_y * scroll_sensitivity);
            if (scroll_offset < 0) scroll_offset = 0;
            if (scroll_offset > max_scroll) scroll_offset = max_scroll;
            glutPostRedisplay();
        }
    }
    last_y = y;
}

void animate() {
    static struct timeval last_time = {0};
    struct timeval curr_time;
    gettimeofday(&curr_time, NULL);
    double elapsed = (curr_time.tv_sec - last_time.tv_sec) * 1000.0 +
                     (curr_time.tv_usec - last_time.tv_usec) / 1000.0;
    if (elapsed >= 1000.0 / TARGET_FPS) {
        glutPostRedisplay();
        last_time = curr_time;
    }
}

void reshape(int w, int h) {
    window_width = w;
    window_height = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)w / h, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void init() {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
#if ENABLE_AR
    glGenTextures(1, &ar_texture_id);
    if (!init_camera(&ar_camera_fd, ar_camera_buffers, &ar_num_buffers, "/dev/video4", ar_frame_width, ar_frame_height)) {
        log_to_file("Using default black background for AR");
    } else {
        ar_camera_enabled = 1;
    }
#endif
    // Initialize command lines array
    command_capacity = 0;
    resize_command_lines();
    initFreeType();
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        log_to_file("Error: No arguments allowed");
        return 1;
    }

    read_locations();

    if (super_maps_dir[0] != '\0' && is_directory_not_empty(super_maps_dir)) {
        strncpy(maps_dir, super_maps_dir, MAX_PATH - 1);
        maps_dir[MAX_PATH - 1] = '\0';
        log_to_file("Using super_maps directory for map rendering");
    } else {
        log_to_file("super_maps directory empty or not specified, falling back to default maps");
    }

    read_state();
    read_super_state();
    if (terminal_enabled) {
        read_commands();
    }
    glutInit(&argc, argv);
    glut_initialized = 1;
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("9th Person Voxel Camera (GL)");
    glutDisplayFunc(display);
    glutIdleFunc(animate);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
     glutMotionFunc(motion); // Add motion callback
    init();
    read_maps();
    if (num_maps > 0) {
        init_map_display_lists();
    }
    glutMainLoop();

#if ENABLE_AR
    cleanup_camera(&ar_camera_fd, ar_camera_buffers, ar_num_buffers, &ar_camera_enabled, ar_frame_width, ar_frame_height);
#endif
    free_maps();
    for (int i = 0; i < num_super_state_pieces; i++) {
        free(super_state_symbol[i]);
    }
    free(super_state_x);
    free(super_state_y);
    free(super_state_z);
    free(super_state_symbol);
    free(super_state_r);
    free(super_state_g);
    free(super_state_b);
    free(super_state_has_color);
    free(super_state_size);
    free(super_state_shape);
    free(command_lines);
    return 0;
}
