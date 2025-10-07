/*
 * core.c - Core engine functionality
 * Based on RMMV's rpg_core.js functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glut.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <time.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

// Utility function declarations
int clamp(int value, int min, int max);
int mod(int value, int n);
int random_int(int max);
void rgb_to_float(int r, int g, int b, float *out);
char* pad_zero(char* str, int length);
bool array_contains(int* array, int size, int element);

// Core engine function declarations
void init_core(void);
void update_core(void);
void render_core(void);
void cleanup_core(void);

// Getters
FT_Library get_ft_library(void);
Display* get_x_display(void);
Window get_x_window(void);

// Core engine state
static bool core_initialized = false;
static FT_Library ft_library;
static Display *x_display = NULL;
static Window x_window = None;

// Utility functions
int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

int mod(int value, int n) {
    return ((value % n) + n) % n;
}

// Math utilities
int random_int(int max) {
    return rand() % max;
}

// Color utilities
void rgb_to_float(int r, int g, int b, float *out) {
    out[0] = r / 255.0f;
    out[1] = g / 255.0f;
    out[2] = b / 255.0f;
}

// String utilities
char* pad_zero(char* str, int length) {
    int len = strlen(str);
    if (len >= length) return str;
    
    char* padded = malloc(length + 1);
    int zeros = length - len;
    for (int i = 0; i < zeros; i++) {
        padded[i] = '0';
    }
    strcpy(padded + zeros, str);
    padded[length] = '\0';
    return padded;
}

// Array utilities
bool array_contains(int* array, int size, int element) {
    for (int i = 0; i < size; i++) {
        if (array[i] == element) return true;
    }
    return false;
}

// Core initialization
void init_core(void) {
    if (core_initialized) return;
    
    printf("Initializing core...\n");
    
    // Initialize FreeType
    if (FT_Init_FreeType(&ft_library)) {
        fprintf(stderr, "Could not init FreeType library\n");
    }
    
    // Get current display and window
    x_display = glXGetCurrentDisplay();
    x_window = glXGetCurrentDrawable();
    
    // Seed random number generator
    srand(time(NULL));
    
    core_initialized = true;
    printf("Core initialized.\n");
}

// Core update
void update_core(void) {
    // Core update logic (if any)
}

// Core rendering
void render_core(void) {
    // Core rendering logic (if any)
}

// Core cleanup
void cleanup_core(void) {
    if (!core_initialized) return;
    
    printf("Cleaning up core...\n");
    
    // Cleanup FreeType
    if (ft_library) {
        FT_Done_FreeType(ft_library);
        ft_library = NULL;
    }
    
    core_initialized = false;
    printf("Core cleaned up.\n");
}

// Getters
FT_Library get_ft_library(void) {
    return ft_library;
}

Display* get_x_display(void) {
    return x_display;
}

Window get_x_window(void) {
    return x_window;
}