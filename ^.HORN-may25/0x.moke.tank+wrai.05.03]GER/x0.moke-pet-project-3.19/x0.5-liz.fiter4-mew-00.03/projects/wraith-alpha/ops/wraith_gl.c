#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define WIDTH 1024
#define HEIGHT 640

GLuint texture_id;
unsigned char *frame_buffer = NULL;
off_t last_pulse_size = 0;

void load_texture() {
    FILE *f = fopen("projects/wraith-alpha/session/rgb/current_frame.rgba32", "rb");
    if (!f) return;

    if (!frame_buffer) frame_buffer = malloc(WIDTH * HEIGHT * 4);
    size_t read = fread(frame_buffer, 1, WIDTH * HEIGHT * 4, f);
    (void)read;
    fclose(f);

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame_buffer);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(-1, -1);
    glTexCoord2f(1, 1); glVertex2f(1, -1);
    glTexCoord2f(1, 0); glVertex2f(1, 1);
    glTexCoord2f(0, 0); glVertex2f(-1, 1);
    glEnd();

    glutSwapBuffers();
}

void timer(int value) {
    struct stat st;
    if (stat("projects/wraith-alpha/session/rgb/rgb_frame_changed.txt", &st) == 0) {
        if (st.st_size != last_pulse_size) {
            last_pulse_size = st.st_size;
            load_texture();
            glutPostRedisplay();
        }
    }
    glutTimerFunc(16, timer, 0);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Wraith Alpha RGB Mirror");

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glutDisplayFunc(display);
    glutTimerFunc(16, timer, 0);

    struct stat st;
    if (stat("projects/wraith-alpha/session/rgb/rgb_frame_changed.txt", &st) == 0) 
        last_pulse_size = st.st_size;

    load_texture();
    glutMainLoop();
    return 0;
}
