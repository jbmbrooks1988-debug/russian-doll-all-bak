post: im 100% ok with putting this in header
cuz its safe contained and never used again. tho its only 20 loc. w/e

# How the UIElement Struct Works Across view.c and controller.c

## Introduction

In C programming, when you define a struct with the same name in multiple source files that need to share data, you need to carefully manage how these definitions are used. This document explains how the `UIElement` structure is defined and shared between `view.c` and `controller.c` in this project.

## The UIElement Struct Definition

In both `view.c` and `controller.c`, you'll find an identical definition of the `UIElement` struct:

```c
typedef struct {
    char type[20];
    int x, y, width, height;
    char id[50];
    char label[50];
    char text_content[256];
    int cursor_pos;
    int is_active;
    int is_checked;
    int slider_value;
    int slider_min, slider_max, slider_step;
    float color[3];
    int parent;
    // ... additional fields
} UIElement;
```

## How the Struct is Shared

### 1. Why the Struct Definition is Needed in Both Files

The struct definition is needed in `controller.c` because of how C compilation works. The `extern` declaration alone is NOT enough. Here's why:

- When the compiler processes `controller.c`, it needs to know the size and internal layout of the `UIElement` struct to correctly access its members
- Without the struct definition, the compiler couldn't determine how to access fields like `elements[i].x`, `elements[i].width`, `elements[i].label`, etc.
- The `extern` declaration only tells the compiler that `elements[]` exists somewhere else, but the struct definition tells it what that array contains and how to access its members

### 2. External Declarations

In `controller.c`, you'll find external declarations that reference the actual variables defined in `view.c`:

```c
extern UIElement elements[];
extern int num_elements;
```

These declarations tell the compiler that:
- `elements[]` is an array of `UIElement` structs that exists somewhere else (in `view.c`)
- `num_elements` is an integer variable that exists somewhere else (in `view.c`)

### 3. The Complete Picture

This means that when `controller.c` accesses the struct:

```c
// Compiler needs to know the offset of .x within the UIElement struct
if (x >= abs_x && x <= abs_x + elements[i].width && ...)
```

The compiler calculates the memory offset for `elements[i].width` based on the struct definition. If the struct definition weren't present in `controller.c`, the compiler wouldn't know how many bytes to skip from the start of the struct to reach the `width` field.

### 3. Actual Definitions in view.c

In `view.c`, these variables are actually defined:
```c
UIElement elements[MAX_ELEMENTS];
int num_elements = 0;
```

## Memory and Data Sharing

### Single Memory Location

Even though the struct is defined in both files, there's only one copy of the actual data (`elements` array and `num_elements`). This data is defined in `view.c` but can be accessed from `controller.c` using the external declarations.

### How It Works Together

1. **Parsing (in view.c)**: The `parse_chtml()` function reads the CHTML file and populates the `elements` array.
2. **Event Handling (in controller.c)**: Functions like `mouse()` and `keyboard()` access and modify the same `elements` array to respond to user interactions.
3. **Rendering (in view.c)**: The `display()` function reads from the `elements` array to draw the UI elements on screen.

## Compilation Process

When compiling this project:
1. `view.c` is compiled separately, creating the actual `elements` array
2. `controller.c` is compiled separately, accessing the `elements` array through external references
3. During the linking phase, the linker connects the external references in `controller.c` to the actual definitions in `view.c`

## Benefits of This Approach

1. **Separation of Concerns**: The view handles rendering and parsing, while the controller handles user input and events.
2. **Shared Data**: Both modules can access the same UI elements without duplicating data.
3. **Maintainability**: Changes to the UIElement structure need to be made in both files simultaneously, ensuring consistency.

## Better Practices (Advanced)

In larger projects, you'd typically put the struct definition in a header file (like `ui_element.h`) and include it in both source files to avoid duplicating the definition:

```c
// ui_element.h
typedef struct {
    char type[20];
    int x, y, width, height;
    // ... other fields
} UIElement;

extern UIElement elements[];
extern int num_elements;
```

This way, you only define the struct once and include it where needed, reducing the risk of inconsistencies between files.

## Summary

The struct with the same name works across both files by:
1. Having identical definitions in both files
2. Using `extern` declarations in controller.c to reference variables defined in view.c
3. Sharing the same memory space for the actual data
4. Allowing both modules to work on the same data while maintaining separation of functionality
