
# C-HTML Framework Blueprint

## 1. Overview

This document outlines a framework for creating UIs in C/OpenGL/GLUT using an HTML-like markup language, dubbed C-HTML. The goal is to separate UI layout and logic from the core application, making projects easier to manage and for AI tools to reason about.

## 2. C-HTML Markup Language

C-HTML will be an XML-based format, similar to HTML. It will define UI elements and their properties using tags and attributes.

### Example:

```xml
<window title="My App" width="800" height="600">
    <header height="50" color="#333333">
        <text value="My Application" x="10" y="10" color="#FFFFFF" />
    </header>
    <canvas id="main_canvas" x="0" y="50" width="800" height="500">
        <!-- 2D/3D content will be rendered here -->
    </canvas>
    <button id="my_button" x="700" y="560" width="80" height="30" label="Click Me" onClick="button_click_handler" />
</window>
```

### Core Concepts:

*   **Elements:** Tags like `<window>`, `<header>`, `<canvas>`, `<button>`, `<text>`.
*   **Attributes:** Properties like `id`, `x`, `y`, `width`, `height`, `color`, `label`, `value`.
*   **Coordinates:** `x` and `y` can be absolute (pixels) or relative (percentages, e.g., "50%").
*   **Events:** Attributes like `onClick` will specify handler functions.

## 3. Architecture

The framework will use a Model-View-Controller (MVC) architecture for the core OpenGL application, with a modular system for extended logic.

*   **`main_prototype_0.c`:** Entry point, OpenGL/GLUT initialization, main loop, and coordination of MVC components.
*   **`view.c`:** The C-HTML interpreter and renderer.
    *   **Parser:** Reads a `.chtml` file and builds a tree data structure of UI elements. A simple XML parser will be implemented.
    *   **Renderer:** Traverses the element tree and draws each element using OpenGL.
*   **`controller.c`:** Handles user input (mouse, keyboard). It will determine which UI element is being interacted with and trigger the appropriate event.
*   **`model.c`:** Manages the application's state. For example, it could hold the value of a text input or the state of a checkbox.

## 4. Event Handling

Events will be handled through function pointers.

1.  The C-HTML parser will read event attributes like `onClick`.
2.  It will use a Look-Up Table (LUT) to map the string value (e.g., "button_click_handler") to a function pointer in the C code.
3.  The `controller.c` will invoke these function pointers when an event occurs.

## 5. Modular System

To keep the core MVC slim, complex logic will be offloaded to separate modules.

*   **Execution:** Modules will be separate executables, launched from the main application using `system()` or `fork()` and `pipes` for communication.
*   **Data Sharing:** Modules will communicate with the core application and each other through `.csv` files. This decouples the modules and avoids complex inter-process communication.

## 6. Canvas and 2D/3D Rendering

The `<canvas>` element is a special element where the 2D/3D game or application will be rendered.

*   The `view.c` renderer will set up the OpenGL viewport and projection for the canvas area.
*   The application-specific rendering logic will be called to draw within the canvas.
*   UI elements can be overlaid on top of the canvas.

## 7. Standard Library of Elements

The framework will provide a set of standard UI elements:

*   `<window>`: The main application window.
*   `<panel>`: A container for other elements.
*   `<button>`: A clickable button.
*   `<text>`: Static text.
*   `<textfield>`: A text input field.
*   `<checkbox>`: A checkbox.
*   `<slider>`: A slider control.
*   `<menu>`: A menu bar or context menu.
*   `<menuitem>`: An item in a menu.
*   `<canvas>`: A drawing area for 2D/3D graphics.

## 8. Data Flow Example: Button Click

1.  User clicks the mouse.
2.  `controller.c` receives the mouse coordinates.
3.  `controller.c` traverses the UI element tree (from `view.c`) to find the element at those coordinates.
4.  It finds a `<button>` element.
5.  `controller.c` looks up the function pointer associated with the button's `onClick` attribute.
6.  `controller.c` calls the function.
7.  The function might update the `model.c`, which in turn could trigger a view update.
