#include <GL/glut.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>

#define MAX_LINE 256
#define TRANSLUCENT_BLOCKS 1
#define DRAW_GRID 1
#define ENABLE_AR 1
#define ENABLE_WEBCAM_STREAM 1
#define CAMERA_WIDTH 640
#define CAMERA_HEIGHT 480
#define MINI_MAP_VIEW_SIZE 10
#define TARGET_FPS 24
#define MAX_MAPS 100
#define MAX_MAP_SIZE 1000

int window_width = 800, window_height = 600;
int state_data[9] = {0}; // [player_x, y, z, cam_x, y, z, cursor_x, y, z]
char piece_symbol[16] = "P";
int piece_data[1024] = {0}; // [x0, y0, z0, x1, y1, z1, ...]
int piece_count = 0;
int maps_data[1024*1024] = {0};
int num_maps = 0;
GLuint map_display_lists[MAX_MAPS] = {0};
int ar_camera_fd = -1, webcam_fd = -1;
void* ar_camera_buffers[4] = {0};
void* webcam_buffers[4] = {0};
unsigned int ar_num_buffers = 0, webcam_num_buffers = 0;
GLuint ar_texture_id = 0, webcam_texture_id = 0;
int ar_frame_width = CAMERA_WIDTH, ar_frame_height = CAMERA_HEIGHT;
int webcam_frame_width = CAMERA_WIDTH / 2, webcam_frame_height = CAMERA_HEIGHT / 2;
int ar_camera_enabled = 0, webcam_enabled = 0;

void log_to_file(const char *message) {
    FILE *fp = fopen("log.txt", "a");
    if (!fp) return;
    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = '\0';
    fprintf(fp, "[%s] %s\n", timestamp, message);
    fclose(fp);
}

int file_exists(const char *filename) {
    struct stat buffer;
    return stat(filename, &buffer) == 0;
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
#if ENABLE_AR || ENABLE_WEBCAM_STREAM
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

void update_camera(int fd, void* buffers[], unsigned int num_buffers, GLuint texture_id, int width, int height, const char *filename) {
#if ENABLE_AR || ENABLE_WEBCAM_STREAM
    if (fd != -1) {
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
    } else if (file_exists(filename)) {
        FILE *fp = fopen(filename, "rb");
        if (!fp) {
            log_to_file("Warning: Failed to read camera data file");
            return;
        }
        unsigned char* rgb = malloc(width * height * 3);
        if (!rgb) {
            log_to_file("Error: Memory allocation failed for camera RGB buffer");
            fclose(fp);
            exit(1);
        }
        size_t read_bytes = fread(rgb, 1, width * height * 3, fp);
        fclose(fp);
        if (read_bytes != width * height * 3) {
            log_to_file("Warning: Incomplete camera data read");
            free(rgb);
            return;
        }
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        free(rgb);
    }
#endif
}

void cleanup_camera(int *fd, void* buffers[], unsigned int num_buffers, int *enabled, int width, int height) {
#if ENABLE_AR || ENABLE_WEBCAM_STREAM
    if (!*enabled || *fd == -1) return;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(*fd, VIDIOC_STREAMOFF, &type) == -1) {
        log_to_file("Warning: Failed to stop streaming");
    }
    for (unsigned int i = 0; i < num_buffers; i++) {
        if (buffers[i]) munmap(buffers[i], width * height * 2);
        buffers[i] = NULL;
    }
    close(*fd);
    *fd = -1;
    *enabled = 0;
#endif
}

void init_map_display_lists() {
    if (num_maps == 0) {
        log_to_file("Warning: No maps loaded, skipping display list creation");
        return;
    }
    for (int i = 0; i < num_maps; i++) {
        if (map_display_lists[i]) glDeleteLists(map_display_lists[i], 1);
        map_display_lists[i] = glGenLists(1);
        if (!map_display_lists[i]) {
            log_to_file("Error: Failed to generate display list");
            exit(1);
        }
        glNewList(map_display_lists[i], GL_COMPILE);
        int mx = maps_data[i*4 + 0];
        int my = maps_data[i*4 + 1];
        int mz = maps_data[i*4 + 2];
        int offset = maps_data[i*4 + 3];
        int width = maps_data[i*4 + 3 + 1];
        int height = maps_data[i*4 + 3 + 2];
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                char tile = maps_data[offset + y*MAX_MAP_SIZE + x];
                if (tile == '#') {
                    glColor4f(0.5f, 0.5f, 0.5f, TRANSLUCENT_BLOCKS ? 0.3f : 1.0f);
                    glPushMatrix();
                    glTranslatef(mx + x + 0.5f, my + y + 0.5f, mz + 0.5f);
                    glutSolidCube(1.0);
                    glPopMatrix();
                } else if (tile == piece_symbol[0]) {
                    glColor3f(1.0f, 0.8f, 0.8f);
                    glPushMatrix();
                    glTranslatef(mx + x + 0.5f, my + y + 0.5f, mz + 0.5f);
                    glutSolidSphere(0.25, 16, 16);
                    glPopMatrix();
                }
            }
        }
        glDisable(GL_BLEND);
        glEndList();
    }
}

void read_maps_data() {
    system("./+x/map_reader.+x");
    if (!file_exists("maps_data.txt")) {
        log_to_file("Warning: maps_data.txt not found, resetting num_maps to 0");
        num_maps = 0;
        return;
    }
    FILE *fp = fopen("maps_data.txt", "r");
    if (!fp) {
        log_to_file("Warning: Failed to read maps_data.txt");
        num_maps = 0;
        return;
    }
    if (fscanf(fp, "%d", &num_maps) != 1 || num_maps < 0) {
        log_to_file("Warning: Invalid num_maps in maps_data.txt");
        num_maps = 0;
        fclose(fp);
        return;
    }
    if (num_maps > MAX_MAPS) num_maps = MAX_MAPS;
    int map_offset = MAX_MAPS * 4 + 3 * MAX_MAPS;
    for (int i = 0; i < num_maps; i++) {
        int map_x, map_y, map_z, width, height;
        if (fscanf(fp, "%d %d %d %d %d", &map_x, &map_y, &map_z, &width, &height) != 5) {
            log_to_file("Warning: Invalid map data format");
            num_maps = i;
            break;
        }
        if (width > MAX_MAP_SIZE || height > MAX_MAP_SIZE) {
            log_to_file("Warning: Map dimensions exceed maximum");
            num_maps = i;
            break;
        }
        maps_data[i*4 + 0] = map_x;
        maps_data[i*4 + 1] = map_y;
        maps_data[i*4 + 2] = map_z;
        maps_data[i*4 + 3] = map_offset;
        maps_data[i*4 + 3 + 1] = width;
        maps_data[i*4 + 3 + 2] = height;
        char line[MAX_LINE];
        fgets(line, MAX_LINE, fp); // Skip newline
        for (int y = 0; y < height; y++) {
            if (!fgets(line, MAX_LINE, fp)) {
                log_to_file("Warning: Incomplete map data");
                num_maps = i;
                break;
            }
            line[strcspn(line, "\n")] = '\0';
            for (int x = 0; x < width; x++) {
                maps_data[map_offset + y*MAX_MAP_SIZE + x] = line[x] ? line[x] : '.';
            }
        }
        map_offset += width * height;
    }
    fclose(fp);
    log_to_file("Maps data read successfully");
    init_map_display_lists();
}

void read_state_data() {
    system("./+x/state_reader.+x");
    if (!file_exists("state_data.txt")) {
        log_to_file("Warning: state_data.txt not found, using default state");
        return;
    }
    FILE *fp = fopen("state_data.txt", "r");
    if (!fp) {
        log_to_file("Warning: Failed to read state_data.txt");
        return;
    }
    if (fscanf(fp, "%d %d %d %d %d %d %d %d %d %s %d",
               &state_data[0], &state_data[1], &state_data[2],
               &state_data[3], &state_data[4], &state_data[5],
               &state_data[6], &state_data[7], &state_data[8],
               piece_symbol, &piece_count) != 11) {
        log_to_file("Warning: Invalid state data format");
        fclose(fp);
        return;
    }
    if (piece_count > 341) piece_count = 341;
    for (int i = 0; i < piece_count; i++) {
        if (fscanf(fp, "%d %d %d", &piece_data[i*3 + 0], &piece_data[i*3 + 1], &piece_data[i*3 + 2]) != 3) {
            log_to_file("Warning: Incomplete piece data");
            piece_count = i;
            break;
        }
    }
    fclose(fp);

    for (int i = 0; i < piece_count; i++) {
        int x = piece_data[i*3 + 0], y = piece_data[i*3 + 1], z = piece_data[i*3 + 2];
        for (int j = 0; j < num_maps; j++) {
            int mx = maps_data[j*4 + 0];
            int my = maps_data[j*4 + 1];
            int mz = maps_data[j*4 + 2];
            int offset = maps_data[j*4 + 3];
            int width = maps_data[j*4 + 3 + 1];
            int height = maps_data[j*4 + 3 + 2];
            if (z == mz && x >= mx && x < mx + width && y >= my && y < my + height) {
                maps_data[offset + (y - my)*MAX_MAP_SIZE + (x - mx)] = piece_symbol[0];
            }
        }
    }
    init_map_display_lists();
}

void draw_cube(float x, float y, float z) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glutSolidCube(1.0);
    glPopMatrix();
}

void draw_sphere(float x, float y, float z) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glutSolidSphere(0.25, 16, 16);
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

void draw_grid() {
    if (num_maps == 0) return;
    glColor3f(0.2f, 0.2f, 0.2f);
    for (int i = 0; i < num_maps; i++) {
        int mx = maps_data[i*4 + 0];
        int my = maps_data[i*4 + 1];
        int mz = maps_data[i*4 + 2];
        int width = maps_data[i*4 + 3 + 1];
        int height = maps_data[i*4 + 3 + 2];
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                glPushMatrix();
                glTranslatef(mx + x + 0.5f, my + y + 0.5f, mz + 0.5f);
                glutWireCube(1.0);
                glPopMatrix();
            }
        }
    }
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

    float cx = floor((float)state_data[6]);
    float cy = floor((float)state_data[7]);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Mini-map: Raw cursor pos: (%d, %d, %d), Snapped: (%.0f, %.0f)", 
             state_data[6], state_data[7], state_data[8], cx, cy);
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

    int min_z_diff = 2147483647;
    int closest_z_map_idx = -1;
    for (int i = 0; i < num_maps; i++) {
        int mz = maps_data[i*4 + 2];
        int z_diff = abs(state_data[8] - mz);
        if (z_diff < min_z_diff) {
            min_z_diff = z_diff;
            closest_z_map_idx = i;
        }
    }

    if (closest_z_map_idx >= 0) {
        for (int i = 0; i < num_maps; i++) {
            int mx = maps_data[i*4 + 0];
            int my = maps_data[i*4 + 1];
            int mz = maps_data[i*4 + 2];
            int offset = maps_data[i*4 + 3];
            int width = maps_data[i*4 + 3 + 1];
            int height = maps_data[i*4 + 3 + 2];

            if (mz != maps_data[closest_z_map_idx*4 + 2]) continue;
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
                    char tile = maps_data[offset + y*MAX_MAP_SIZE + x];
                    if (tile == '#') {
                        glColor3f(0.5f, 0.5f, 0.5f);
                        glBegin(GL_QUADS);
                        glVertex2f(world_x + 0.1, world_y + 0.1);
                        glVertex2f(world_x + 0.9, world_y + 0.1);
                        glVertex2f(world_x + 0.9, world_y + 0.9);
                        glVertex2f(world_x + 0.1, world_y + 0.9);
                        glEnd();
                    } else if (tile == piece_symbol[0]) {
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
    } else {
        log_to_file("Mini-map: No maps loaded");
    }

    glColor3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(state_data[0] + 0.2, state_data[1] + 0.2);
    glVertex2f(state_data[0] + 0.8, state_data[1] + 0.2);
    glVertex2f(state_data[0] + 0.8, state_data[1] + 0.8);
    glVertex2f(state_data[0] + 0.2, state_data[1] + 0.8);
    glEnd();

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

void draw_webcam_stream() {
#if ENABLE_WEBCAM_STREAM
    if (webcam_enabled) {
        system("./+x/camera_handler.+x webcam");
        update_camera(webcam_fd, webcam_buffers, webcam_num_buffers, webcam_texture_id, webcam_frame_width, webcam_frame_height, "camera_data_webcam.bin");
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, window_width, 0, window_height, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, webcam_texture_id);
        int stream_size = (window_width < window_height ? window_width : window_height) * 0.2;
        glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(window_width - stream_size, 0);
        glTexCoord2f(1, 1); glVertex2f(window_width, 0);
        glTexCoord2f(1, 0); glVertex2f(window_width, stream_size);
        glTexCoord2f(0, 0); glVertex2f(window_width - stream_size, stream_size);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
#endif
}

int check_memory_usage() {
    FILE *fp = fopen("/proc/self/statm", "r");
    if (!fp) {
        log_to_file("Warning: Failed to read /proc/self/statm");
        return 0;
    }
    unsigned long resident;
    if (fscanf(fp, "%*ld %ld", &resident) != 1) {
        log_to_file("Warning: Failed to parse /proc/self/statm");
        fclose(fp);
        return 0;
    }
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

    if (!check_memory_usage()) {
        log_to_file("Warning: Memory check failed, proceeding with caution");
    }

    read_state_data();
    read_maps_data();

#if ENABLE_AR
    if (ar_camera_enabled || file_exists("camera_data_ar.bin")) {
        system("./+x/camera_handler.+x ar");
        update_camera(ar_camera_fd, ar_camera_buffers, ar_num_buffers, ar_texture_id, ar_frame_width, ar_frame_height, "camera_data_ar.bin");
        if (ar_camera_enabled || file_exists("camera_data_ar.bin")) {
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, 1, 0, 1, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, ar_texture_id);
            glBegin(GL_QUADS);
            glTexCoord2f(0, 1); glVertex2f(0, 0);
            glTexCoord2f(1, 1); glVertex2f(1, 0);
            glTexCoord2f(1, 0); glVertex2f(1, 1);
            glTexCoord2f(0, 0); glVertex2f(0, 1);
            glEnd();
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_DEPTH_TEST);
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
        }
    }
#endif

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)window_width / window_height, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(state_data[3], state_data[4], state_data[5], state_data[0], state_data[1], state_data[2], 0, 0, 1);

#if DRAW_GRID
    draw_grid();
#endif

    for (int i = 0; i < num_maps; i++) {
        if (map_display_lists[i]) glCallList(map_display_lists[i]);
    }

    glColor3f(0.0f, 0.0f, 1.0f);
    draw_sphere(state_data[0] + 0.5f, state_data[1] + 0.5f, state_data[2] + 0.5f);

    if (state_data[6] >= -1000 && state_data[6] < 1000 && state_data[7] >= -1000 && state_data[7] < 1000 && state_data[8] >= -1000 && state_data[8] < 1000) {
        draw_cursor((float)state_data[6], (float)state_data[7], (float)state_data[8]);
    }

    draw_mini_map();

#if ENABLE_WEBCAM_STREAM
    draw_webcam_stream();
#endif

    char coord_text[256];
    snprintf(coord_text, sizeof(coord_text), "Player: (%d, %d, %d)", state_data[0], state_data[1], state_data[2]);
    draw_text(10, window_height - 20, coord_text);
    snprintf(coord_text, sizeof(coord_text), "Camera: (%d, %d, %d)", state_data[3], state_data[4], state_data[5]);
    draw_text(10, window_height - 40, coord_text);
    snprintf(coord_text, sizeof(coord_text), "Cursor: (%d, %d, %d)", state_data[6], state_data[7], state_data[8]);
    draw_text(10, window_height - 60, coord_text);

    glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {
    char details[64];
    snprintf(details, sizeof(details), "key=%c, x=%d, y=%d", key, x, y);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "./+x/input_logger.+x keyboard \"%s\"", details);
    system(cmd);
}

void special_keyboard(int key, int x, int y) {
    char *key_str;
    switch (key) {
        case GLUT_KEY_LEFT: key_str = "LeftArrow"; break;
        case GLUT_KEY_RIGHT: key_str = "RightArrow"; break;
        case GLUT_KEY_UP: key_str = "UpArrow"; break;
        case GLUT_KEY_DOWN: key_str = "DownArrow"; break;
        default: key_str = "UnknownSpecial"; break;
    }
    char details[64];
    snprintf(details, sizeof(details), "key=%s, x=%d, y=%d", key_str, x, y);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "./+x/input_logger.+x special_keyboard \"%s\"", details);
    system(cmd);
}

void mouse(int button, int state, int x, int y) {
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
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "./+x/input_logger.+x mouse \"%s\"", details);
    system(cmd);
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
    if (h == 0) h = 1; // Prevent division by zero
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
    if (!ar_texture_id) {
        log_to_file("Error: Failed to generate AR texture ID");
        exit(1);
    }
    if (!init_camera(&ar_camera_fd, ar_camera_buffers, &ar_num_buffers, "/dev/video4", ar_frame_width, ar_frame_height)) {
        log_to_file("Using default black background for AR");
    } else {
        ar_camera_enabled = 1;
    }
#endif
#if ENABLE_WEBCAM_STREAM
    glGenTextures(1, &webcam_texture_id);
    if (!webcam_texture_id) {
        log_to_file("Error: Failed to generate webcam texture ID");
        exit(1);
    }
    if (!init_camera(&webcam_fd, webcam_buffers, &webcam_num_buffers, "/dev/video1", webcam_frame_width, webcam_frame_height)) {
        log_to_file("Warning: Failed to initialize webcam at /dev/video1");
    } else {
        webcam_enabled = 1;
    }
#endif
}

int main(int argc, char *argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("7th Person Voxel Camera (GL)");
    glutDisplayFunc(display);
    glutIdleFunc(animate);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special_keyboard);
    glutMouseFunc(mouse);
    init();
    read_maps_data();
    read_state_data();
    glutMainLoop();

#if ENABLE_AR
    cleanup_camera(&ar_camera_fd, ar_camera_buffers, ar_num_buffers, &ar_camera_enabled, ar_frame_width, ar_frame_height);
#endif
#if ENABLE_WEBCAM_STREAM
    cleanup_camera(&webcam_fd, webcam_buffers, webcam_num_buffers, &webcam_enabled, webcam_frame_width, webcam_frame_height);
#endif
    return 0;
}
