/*
 * camera.c - Camera handling module for AR functionality
 * Based on the example USB camera implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <signal.h>
#include <linux/videodev2.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

// Function declarations for camera module
int init_camera_with_fallback(void);
int update_camera_frame(void);
unsigned char* get_camera_frame(void);
void get_camera_dimensions(int *width, int *height);
void cleanup_camera_resources(void);

// Function prototypes
static void cleanup_camera(void);
static int read_frame(void);

// Camera state
static int WIDTH = 640;
static int HEIGHT = 480;
static int RGB_SIZE; // Initialized in init_camera
static unsigned char *rgb_data = NULL; // Dynamic allocation (RGBA)

// Device names for camera detection
static char *dev_names[] = {"/dev/video4", "/dev/video0"}; // USB camera and fallback
static int current_dev_index = 0; // Track current device
static int fd = -1;
static void *buffer_start = NULL;
static unsigned int buffer_length;
static unsigned int current_pixelformat = V4L2_PIX_FMT_YUYV;

// Helper function to convert pixel format to string
static const char *pixelformat_to_string(unsigned int format) {
    static char str[5];
    str[0] = format & 0xFF;
    str[1] = (format >> 8) & 0xFF;
    str[2] = (format >> 16) & 0xFF;
    str[3] = (format >> 24) & 0xFF;
    str[4] = '\0';
    return str;
}

static int xioctl(int fh, int request, void *arg) {
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

static void errno_exit(const char *s) {
    fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
    cleanup_camera();
}

static void cleanup_camera(void) {
    static int cleaned = 0;
    if (cleaned) return; // Prevent multiple cleanups
    cleaned = 1;

    if (fd != -1) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(fd, VIDIOC_STREAMOFF, &type);
        if (buffer_start) {
            munmap(buffer_start, buffer_length);
            buffer_start = NULL;
        }
        close(fd);
        fd = -1;
    }
    if (rgb_data) {
        free(rgb_data);
        rgb_data = NULL;
    }
}

static void yuyv_to_rgb(unsigned char *yuyv, unsigned char *rgb, int size) {
    int i, j;
    int max_pixels = (RGB_SIZE / 4); // RGBA (4 bytes per pixel)
    for (i = 0, j = 0; i < size - 3 && j < RGB_SIZE - 7; i += 4, j += 8) {
        int y0 = yuyv[i + 0];
        int u  = yuyv[i + 1] - 128;
        int y1 = yuyv[i + 2];
        int v  = yuyv[i + 3] - 128;

        // First pixel
        int r = (298 * y0 + 409 * v + 128) >> 8;
        int g = (298 * y0 - 100 * u - 208 * v + 128) >> 8;
        int b = (298 * y0 + 516 * u + 128) >> 8;
        rgb[j + 0] = (r < 0) ? 0 : (r > 255) ? 255 : r;
        rgb[j + 1] = (g < 0) ? 0 : (g > 255) ? 255 : g;
        rgb[j + 2] = (b < 0) ? 0 : (b > 255) ? 255 : b;
        rgb[j + 3] = 255; // Alpha = 255 (opaque)

        // Second pixel
        r = (298 * y1 + 409 * v + 128) >> 8;
        g = (298 * y1 - 100 * u - 208 * v + 128) >> 8;
        b = (298 * y1 + 516 * u + 128) >> 8;
        rgb[j + 4] = (r < 0) ? 0 : (r > 255) ? 255 : r;
        rgb[j + 5] = (g < 0) ? 0 : (g > 255) ? 255 : g;
        rgb[j + 6] = (b < 0) ? 0 : (b > 255) ? 255 : b;
        rgb[j + 7] = 255; // Alpha = 255 (opaque)
    }
}

static void yuv420_to_rgb(unsigned char *yuv, unsigned char *rgb, int width, int height) {
    int y_size = width * height;
    int uv_size = y_size / 4;
    unsigned char *y = yuv;
    unsigned char *u = yuv + y_size;
    unsigned char *v = u + uv_size;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            int y_idx = j * width + i;
            int uv_idx = (j / 2) * (width / 2) + (i / 2);
            int y_val = y[y_idx];
            int u_val = u[uv_idx] - 128;
            int v_val = v[uv_idx] - 128;

            int r = (298 * y_val + 409 * v_val + 128) >> 8;
            int g = (298 * y_val - 100 * u_val - 208 * v_val + 128) >> 8;
            int b = (298 * y_val + 516 * u_val + 128) >> 8;

            r = r < 0 ? 0 : (r > 255 ? 255 : r);
            g = g < 0 ? 0 : (g > 255 ? 255 : g);
            b = b < 0 ? 0 : (b > 255 ? 255 : b);

            int rgb_idx = (j * width + i) * 4; // RGBA
            rgb[rgb_idx + 0] = r;
            rgb[rgb_idx + 1] = g;
            rgb[rgb_idx + 2] = b;
            rgb[rgb_idx + 3] = 255; // Alpha = 255 (opaque)
        }
    }
}

static void init_mmap(void) {
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
        errno_exit("VIDIOC_REQBUFS");
    }

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)) {
        errno_exit("VIDIOC_QUERYBUF");
    }

    buffer_length = buf.length;
    buffer_start = mmap(NULL, buffer_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (MAP_FAILED == buffer_start) errno_exit("mmap");
}

static int try_format(int width, int height, unsigned int pixelformat) {
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = pixelformat;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)) {
        fprintf(stderr, "Failed to set format %s at %dx%d on %s\n", 
                pixelformat_to_string(pixelformat), width, height, dev_names[current_dev_index]);
        return 0;
    }

    if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt)) {
        fprintf(stderr, "VIDIOC_G_FMT error on %s\n", dev_names[current_dev_index]);
        return 0;
    }

    // Update WIDTH, HEIGHT, and RGB_SIZE if device adjusts resolution
    if (fmt.fmt.pix.width != width || fmt.fmt.pix.height != height) {
        fprintf(stderr, "Format adjusted to %ux%u on %s\n", 
                fmt.fmt.pix.width, fmt.fmt.pix.height, dev_names[current_dev_index]);
        WIDTH = fmt.fmt.pix.width;
        HEIGHT = fmt.fmt.pix.height;
        RGB_SIZE = WIDTH * HEIGHT * 4; // RGBA
        // Reallocate rgb_data
        unsigned char *new_rgb_data = realloc(rgb_data, RGB_SIZE);
        if (!new_rgb_data) {
            fprintf(stderr, "Failed to reallocate rgb_data\n");
            return 0;
        }
        rgb_data = new_rgb_data;
        memset(rgb_data, 0, RGB_SIZE); // Clear buffer to prevent residual data
    }

    printf("Using format: %s, size: %ux%u, bytes: %u on %s\n", 
           pixelformat_to_string(fmt.fmt.pix.pixelformat), 
           fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.sizeimage, dev_names[current_dev_index]);
    buffer_length = fmt.fmt.pix.sizeimage;
    current_pixelformat = fmt.fmt.pix.pixelformat;
    return 1;
}

static int init_device(void) {
    struct v4l2_capability cap;
    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
        fprintf(stderr, "VIDIOC_QUERYCAP error on %s\n", dev_names[current_dev_index]);
        return 0;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "%s is no video capture device\n", dev_names[current_dev_index]);
        return 0;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "%s does not support streaming i/o\n", dev_names[current_dev_index]);
        return 0;
    }

    // Try supported formats based on typical USB camera capabilities
    if (try_format(640, 480, V4L2_PIX_FMT_YUYV)) {
        init_mmap();
        return 1;
    }
    if (try_format(320, 240, V4L2_PIX_FMT_YUYV)) {
        init_mmap();
        return 1;
    }
    if (try_format(1280, 720, V4L2_PIX_FMT_YUV420)) {
        init_mmap();
        return 1;
    }
    if (try_format(320, 240, V4L2_PIX_FMT_YUV420)) {
        init_mmap();
        return 1;
    }

    fprintf(stderr, "No supported format/resolution found for %s\n", dev_names[current_dev_index]);
    return 0;
}

static int start_capturing(void) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
        errno_exit("VIDIOC_QBUF");
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(fd, VIDIOC_STREAMON, &type)) {
        errno_exit("VIDIOC_STREAMON");
    }
    
    return 1;
}

// Try to open a camera device
static int try_open_device(int dev_index) {
    int device_fd = open(dev_names[dev_index], O_RDWR | O_NONBLOCK, 0);
    if (-1 == device_fd) {
        fprintf(stderr, "Cannot open '%s': %d, %s\n", dev_names[dev_index], errno, strerror(errno));
        return -1;
    }
    
    return device_fd;
}

// Switch to the next available camera device
static int switch_device(void) {
    if (fd != -1) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(fd, VIDIOC_STREAMOFF, &type);
        if (buffer_start) {
            munmap(buffer_start, buffer_length);
            buffer_start = NULL;
        }
        close(fd);
        fd = -1;
    }

    // Try the next device
    current_dev_index = (current_dev_index + 1) % (sizeof(dev_names) / sizeof(dev_names[0]));
    printf("Switching to %s\n", dev_names[current_dev_index]);

    fd = try_open_device(current_dev_index);
    if (fd == -1) {
        return 0;
    }

    if (!init_device()) {
        close(fd);
        fd = -1;
        return 0;
    }

    if (!start_capturing()) {
        close(fd);
        fd = -1;
        return 0;
    }
    
    return 1;
}

static int read_frame(void) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
        if (errno == EAGAIN) return 0;
        errno_exit("VIDIOC_DQBUF");
    }

    // Clear rgb_data before processing to ensure no residual data
    memset(rgb_data, 0, RGB_SIZE);

    // Convert frame to RGBA
    if (current_pixelformat == V4L2_PIX_FMT_YUYV) {
        yuyv_to_rgb(buffer_start, rgb_data, buf.bytesused);
    } else if (current_pixelformat == V4L2_PIX_FMT_YUV420) {
        yuv420_to_rgb(buffer_start, rgb_data, WIDTH, HEIGHT);
    } else {
        fprintf(stderr, "Unsupported pixel format: %s\n", pixelformat_to_string(current_pixelformat));
        return 0;
    }

    if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
        errno_exit("VIDIOC_QBUF");
    }

    return 1;
}

// Initialize camera with fallback mechanism
int init_camera_with_fallback(void) {
    // Initialize RGB_SIZE (RGBA)
    RGB_SIZE = WIDTH * HEIGHT * 4;
    
    // Allocate rgb_data
    rgb_data = malloc(RGB_SIZE);
    if (!rgb_data) {
        fprintf(stderr, "Failed to allocate rgb_data\n");
        return 0;
    }
    memset(rgb_data, 0, RGB_SIZE); // Initialize to zero

    // Try to open the first device
    fd = try_open_device(current_dev_index);
    if (fd == -1) {
        // If the first device fails, try the next one
        current_dev_index = (current_dev_index + 1) % (sizeof(dev_names) / sizeof(dev_names[0]));
        fd = try_open_device(current_dev_index);
        if (fd == -1) {
            fprintf(stderr, "Failed to open any camera device\n");
            free(rgb_data);
            rgb_data = NULL;
            return 0;
        }
    }

    if (!init_device()) {
        close(fd);
        fd = -1;
        // Try switching to the next device
        if (!switch_device()) {
            fprintf(stderr, "Failed to initialize any camera device\n");
            free(rgb_data);
            rgb_data = NULL;
            return 0;
        }
    }

    if (!start_capturing()) {
        close(fd);
        fd = -1;
        free(rgb_data);
        rgb_data = NULL;
        return 0;
    }
    
    printf("Camera initialized successfully with %s\n", dev_names[current_dev_index]);
    return 1;
}

// Update camera frame
int update_camera_frame(void) {
    if (fd == -1) return 0;
    
    fd_set fds;
    struct timeval tv = {0, 10000}; // 10ms timeout
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    int r = select(fd + 1, &fds, NULL, NULL, &tv);
    if (-1 == r && EINTR != errno) {
        fprintf(stderr, "select error %d, %s\n", errno, strerror(errno));
        return 0;
    }

    if (FD_ISSET(fd, &fds)) {
        return read_frame();
    }
    
    return 0;
}

// Get camera frame data
unsigned char* get_camera_frame(void) {
    return rgb_data;
}

// Get camera dimensions
void get_camera_dimensions(int *width, int *height) {
    *width = WIDTH;
    *height = HEIGHT;
}

// Cleanup camera resources
void cleanup_camera_resources(void) {
    cleanup_camera();
}
