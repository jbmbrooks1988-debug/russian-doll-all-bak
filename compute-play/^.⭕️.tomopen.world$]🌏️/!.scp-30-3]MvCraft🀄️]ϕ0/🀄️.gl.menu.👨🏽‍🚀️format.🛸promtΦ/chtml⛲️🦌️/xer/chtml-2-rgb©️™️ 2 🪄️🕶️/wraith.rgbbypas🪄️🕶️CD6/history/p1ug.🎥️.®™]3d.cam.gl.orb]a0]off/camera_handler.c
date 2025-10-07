#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>

#define CAMERA_WIDTH 640
#define CAMERA_HEIGHT 480

void log_to_file(const char *message) {
    FILE *fp = fopen("log.txt", "a");
    if (!fp) return;
    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strcspn(timestamp, "\n")] = '\0';
    fprintf(fp, "[%s] %s\n", timestamp, message);
    fclose(fp);
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

void handle_camera(const char *device, int width, int height, const char *output) {
    int fd = open(device, O_RDWR);
    if (fd == -1) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Warning: Failed to open camera at %s", device);
        log_to_file(msg);
        return;
    }

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Warning: Failed to set camera format for %s", device);
        log_to_file(msg);
        close(fd);
        return;
    }

    struct v4l2_requestbuffers req = {0};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Warning: Failed to request buffers for %s", device);
        log_to_file(msg);
        close(fd);
        return;
    }
    unsigned int num_buffers = req.count;

    void* buffers[4];
    for (unsigned int i = 0; i < num_buffers; i++) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Warning: Failed to query buffer for %s", device);
            log_to_file(msg);
            close(fd);
            return;
        }
        buffers[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (buffers[i] == MAP_FAILED) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Warning: Failed to map buffer for %s", device);
            log_to_file(msg);
            close(fd);
            return;
        }
    }

    for (unsigned int i = 0; i < num_buffers; i++) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Warning: Failed to queue buffer for %s", device);
            log_to_file(msg);
            for (unsigned int j = 0; j < i; j++) munmap(buffers[j], width * height * 2);
            close(fd);
            return;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Warning: Failed to start streaming for %s", device);
        log_to_file(msg);
        for (unsigned int i = 0; i < num_buffers; i++) munmap(buffers[i], width * height * 2);
        close(fd);
        return;
    }

    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
        log_to_file("Warning: Failed to dequeue buffer");
        for (unsigned int i = 0; i < num_buffers; i++) munmap(buffers[i], width * height * 2);
        close(fd);
        return;
    }

    unsigned char* rgb = malloc(width * height * 3);
    if (!rgb) {
        log_to_file("Error: Memory allocation failed for camera RGB buffer");
        for (unsigned int i = 0; i < num_buffers; i++) munmap(buffers[i], width * height * 2);
        close(fd);
        return;
    }
    yuyv_to_rgb(buffers[buf.index], rgb, width, height);

    FILE *fp = fopen(output, "wb");
    if (!fp) {
        log_to_file("Error: Failed to write camera data file");
        free(rgb);
        for (unsigned int i = 0; i < num_buffers; i++) munmap(buffers[i], width * height * 2);
        close(fd);
        return;
    }
    fwrite(rgb, 1, width * height * 3, fp);
    fclose(fp);
    free(rgb);

    if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
        log_to_file("Warning: Failed to requeue buffer");
    }

    ioctl(fd, VIDIOC_STREAMOFF, &type);
    for (unsigned int i = 0; i < num_buffers; i++) {
        munmap(buffers[i], width * height * 2);
    }
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        log_to_file("Error: camera_handler requires one argument (ar or webcam)");
        return 1;
    }
    if (strcmp(argv[1], "ar") == 0) {
        handle_camera("/dev/video4", CAMERA_WIDTH, CAMERA_HEIGHT, "camera_data_ar.bin");
    } else if (strcmp(argv[1], "webcam") == 0) {
        handle_camera("/dev/video1", CAMERA_WIDTH / 2, CAMERA_HEIGHT / 2, "camera_data_webcam.bin");
    } else {
        log_to_file("Error: Invalid camera type (use ar or webcam)");
        return 1;
    }
    return 0;
}
