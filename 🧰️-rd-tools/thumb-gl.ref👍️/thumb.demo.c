#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_WIDTH  400
#define WINDOW_HEIGHT 500
#define NUMBERS_TOTAL 100
#define VISIBLE_COUNT 10

// Thumb scroller
float thumbY = 0;          // position from top (0 = top, max = WINDOW_HEIGHT - thumbHeight)
float thumbHeight = 50.0f; // fixed thumb size
int dragging = 0;

// Scroll state
int startIndex = 0;

// Debug info
char debugText[128] = "Click anywhere...";
int clickedNumber = -1;

// Mouse click position (actual screen space, Y=0 at top for logic!)
int lastClickX = 0, lastClickY = 0;

// Convert screen Y (mouse Y, top=0) to OpenGL Y (bottom=0)
float screenToOpenGLY(int y) {
    return WINDOW_HEIGHT - y;
}

// Convert mouse Y to thumb position (track space, top=0)
float mouseYToThumbY(int y) {
    return y - thumbHeight/2.0f; // center grab
}

// Update startIndex from thumb position
void updateIndexFromThumb() {
    float trackHeight = WINDOW_HEIGHT - thumbHeight;
    if (trackHeight <= 0) return;

    float ratio = thumbY / trackHeight;
    startIndex = (int)(ratio * (NUMBERS_TOTAL - VISIBLE_COUNT));

    if (startIndex < 0) startIndex = 0;
    if (startIndex > NUMBERS_TOTAL - VISIBLE_COUNT)
        startIndex = NUMBERS_TOTAL - VISIBLE_COUNT;
}

// Update thumb from startIndex (for initialization or external scroll)
void updateThumbFromIndex() {
    float trackHeight = WINDOW_HEIGHT - thumbHeight;
    if (trackHeight <= 0) return;

    float ratio = (float)startIndex / (NUMBERS_TOTAL - VISIBLE_COUNT);
    thumbY = ratio * trackHeight;
}

// Check if click is on a number
int getClickedNumber(int x, int y) {
    // We treat mouse Y as top=0 for logic (inverted from OpenGL)
    for (int i = 0; i < VISIBLE_COUNT; i++) {
        int numYTop = 30 + i * 40;        // top of number box (Y=0 at top)
        int numYBottom = numYTop + 35;    // bottom of number box
        int numXLeft = 50;
        int numXRight = 150;

        if (x >= numXLeft && x <= numXRight &&
            y >= numYTop && y <= numYBottom) {
            return startIndex + i + 1; // 1-based index
        }
    }
    return -1;
}

// Render everything
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // Render numbers
    glColor3f(0.2f, 0.2f, 0.8f);
    for (int i = 0; i < VISIBLE_COUNT; i++) {
        int num = startIndex + i + 1;
        char buffer[16];
        sprintf(buffer, "%d", num);

        float x = 50.0f;
        // OpenGL Y: 0 at bottom → we invert by subtracting from height
        float y = WINDOW_HEIGHT - (40.0f + i * 40.0f); // 40px down from top

        glRasterPos2f(x, y);
        for (char* c = buffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
        }
    }

    // Draw thumb (right edge, position based on thumbY from top)
    glColor3f(0.8f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
        glVertex2f(WINDOW_WIDTH - 20, WINDOW_HEIGHT - thumbY);               // top-left
        glVertex2f(WINDOW_WIDTH - 10, WINDOW_HEIGHT - thumbY);               // top-right
        glVertex2f(WINDOW_WIDTH - 10, WINDOW_HEIGHT - thumbY - thumbHeight); // bottom-right
        glVertex2f(WINDOW_WIDTH - 20, WINDOW_HEIGHT - thumbY - thumbHeight); // bottom-left
    glEnd();

    // Render debug text (clicked position + clicked number)
    glColor3f(0.0f, 0.0f, 0.0f);
    glRasterPos2f(10, WINDOW_HEIGHT - 15); // top-left corner
    char* c = debugText;
    while (*c) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *c++);

    if (clickedNumber > 0) {
        char numMsg[64];
        sprintf(numMsg, "Clicked Number: %d", clickedNumber);
        glRasterPos2f(10, WINDOW_HEIGHT - 30);
        c = numMsg;
        while (*c) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *c++);
    }

    glutSwapBuffers();
}

// Mouse click
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        lastClickX = x;
        lastClickY = y; // this is screen Y (0=top, WINDOW_HEIGHT=bottom)

        // Check if clicked on thumb
        float thumbTopScreenY = thumbY; // because thumbY is measured from top
        float thumbBottomScreenY = thumbY + thumbHeight;

        if (x >= WINDOW_WIDTH - 20 && x <= WINDOW_WIDTH - 10 &&
            y >= thumbTopScreenY && y <= thumbBottomScreenY) {
            dragging = 1;
        } else {
            dragging = 0;
            // Check if clicked on a number
            clickedNumber = getClickedNumber(x, y);
        }

        // Update debug text
        sprintf(debugText, "Clicked at X:%d Y:%d (screen top=0)", x, y);
        glutPostRedisplay();
    } else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        dragging = 0;
    }
}

// Mouse drag
void motion(int x, int y) {
    if (dragging) {
        // Update thumbY based on mouse Y (from top)
        thumbY = mouseYToThumbY(y);

        // Constrain within track
        float maxY = WINDOW_HEIGHT - thumbHeight;
        if (thumbY < 0) thumbY = 0;
        if (thumbY > maxY) thumbY = maxY;

        updateIndexFromThumb();
        glutPostRedisplay();
    }
}

// Initialize OpenGL and state
void init() {
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT);
    glMatrixMode(GL_MODELVIEW);

    // 🔥 CRITICAL FOR THAT VERY FX: Initialize thumb at TOP-RIGHT
    thumbY = 0; // top of track
    updateIndexFromThumb(); // startIndex = 0 → shows 1-10
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("THAT VERY FX DEMO — Thumb Starts Top Right (Now Fixed!)");

    init();

    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);

    glutMainLoop();
    return 0;
}
