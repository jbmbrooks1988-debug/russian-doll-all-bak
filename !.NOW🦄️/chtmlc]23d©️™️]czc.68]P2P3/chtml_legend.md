
# C-HTML Legend

This document describes the available elements and attributes in the C-HTML framework.

## Elements

### `<window>`

The root element of a C-HTML document.

**Attributes:**

*   `title` (string): The title of the application window.
*   `width` (int): The width of the window in pixels.
*   `height` (int): The height of the window in pixels.

### `<header>`

A container for header content.

**Attributes:**

*   `y` (int): The y-coordinate of the header.
*   `width` (int): The width of the header.
*   `height` (int): The height of the header.
*   `color` (hex): The background color of the header (e.g., "#333333").

### `<text>`

A static text element.

**Attributes:**

*   `id` (string): A unique identifier for the text element.
*   `value` (string): The text to display.
*   `x` (int): The x-coordinate of the text.
*   `y` (int): The y-coordinate of the text.
*   `color` (hex): The color of the text (e.g., "#FFFFFF").

### `<button>`

A clickable button.

**Attributes:**

*   `id` (string): A unique identifier for the button.
*   `x` (int): The x-coordinate of the button.
*   `y` (int): The y-coordinate of the button.
*   `width` (int): The width of the button.
*   `height` (int): The height of the button.
*   `label` (string): The text displayed on the button.
*   `onClick` (string): The name of the C function to call when the button is clicked.

### `<panel>`

A container for other elements.

**Attributes:**

*   `x` (int): The x-coordinate of the panel.
*   `y` (int): The y-coordinate of the panel.
*   `width` (int): The width of the panel.
*   `height` (int): The height of the panel.
*   `color` (hex): The background color of the panel (e.g., "#444444").

### `<textfield>`

A text input field.

**Attributes:**

*   `id` (string): A unique identifier for the text field.
*   `x` (int): The x-coordinate of the text field.
*   `y` (int): The y-coordinate of the text field.
*   `width` (int): The width of the text field.
*   `height` (int): The height of the text field.
*   `value` (string): The initial text content of the text field.

### `<textarea>`

A multi-line text input field.

**Attributes:**

*   `id` (string): A unique identifier for the text area.
*   `x` (int): The x-coordinate of the text area.
*   `y` (int): The y-coordinate of the text area.
*   `width` (int): The width of the text area.
*   `height` (int): The height of the text area.
*   `value` (string): The initial text content of the text area.

### `<checkbox>`

A checkbox for boolean input.

**Attributes:**

*   `id` (string): A unique identifier for the checkbox.
*   `x` (int): The x-coordinate of the checkbox.
*   `y` (int): The y-coordinate of the checkbox.
*   `width` (int): The width of the checkbox (for the square).
*   `height` (int): The height of the checkbox (for the square).
*   `label` (string): The text label displayed next to the checkbox.
*   `checked` (boolean): "true" if the checkbox is initially checked, "false" otherwise.

### `<slider>`

A slider for selecting a value from a range. Supports both horizontal (width > height) and vertical (height > width) orientations automatically.

**Attributes:**

*   `id` (string): A unique identifier for the slider.
*   `x` (int): The x-coordinate of the slider.
*   `y` (int): The y-coordinate of the slider.
*   `width` (int): The width of the slider track.
*   `height` (int): The height of the slider track.
*   `min` (int): The minimum value of the slider.
*   `max` (int): The maximum value of the slider.
*   `value` (int): The current value of the slider.
*   `step` (int): The increment/decrement step for the slider value.

**Behavior:**
*   Orientation is automatically determined by aspect ratio (height > width = vertical slider)
*   Sends `SLIDER:<id>:<value>` to external modules when moved
*   Supports both mouse click and drag interaction

### `<button id="run_module_button">`

A button to trigger the execution of an external module.

**Attributes:**

*   `id` (string): A unique identifier for the button.
*   `x` (int): The x-coordinate of the button.
*   `y` (int): The y-coordinate of the button.
*   `width` (int): The width of the button.
*   `height` (int): The height of the button.
*   `label` (string): The text displayed on the button (e.g., "Run Module").
*   `onClick` (string): The name of the C function to call when the button is clicked (e.g., "run_module_handler").

### `onClick` handlers

Specialized event handlers for various UI interactions.

**Available handlers:**

*   `switch_to_2d_handler` (string): Switches canvas elements to 2D rendering mode.
*   `switch_to_3d_handler` (string): Switches canvas elements to 3D rendering mode.

### `<menu>`

A menu bar that can contain menu items.

**Attributes:**

*   `id` (string): A unique identifier for the menu.
*   `x` (int): The x-coordinate of the menu.
*   `y` (int): The y-coordinate of the menu.
*   `width` (int): The width of the menu.
*   `height` (int): The height of the menu.
*   `label` (string): The text label displayed on the menu (e.g., "Main Menu").
*   `color` (hex): The background color of the menu (e.g., "#555555").
*   `onClick` (string): The name of the C function to call when the menu is clicked.

### `<menuitem>`

An item within a menu that can be selected.

**Attributes:**

*   `x` (int): The x-coordinate of the menu item (relative to parent).
*   `y` (int): The y-coordinate of the menu item (relative to parent).
*   `width` (int): The width of the menu item.
*   `height` (int): The height of the menu item.
*   `label` (string): The text label displayed on the menu item (e.g., "File", "Edit").
*   `color` (hex): The background color of the menu item (e.g., "#CCCCCC").
*   `onClick` (string): The name of the C function to call when the menu item is clicked.

### `<canvas>`

A drawable area for custom graphics rendering and interaction with external modules.

**Attributes:**

*   `id` (string): A unique identifier for the canvas.
*   `x` (int): The x-coordinate of the canvas.
*   `y` (int): The y-coordinate of the canvas.
*   `width` (int): The width of the canvas.
*   `height` (int): The height of the canvas.
*   `label` (string): An optional label for the canvas.
*   `view_mode` (string): The rendering mode for the canvas - "2d" for 2D rendering or "3d" for 3D rendering (default: "2d").
*   `onClick` (string): The name of the C function to call when the canvas is clicked.

**Behavior:**
*   Can receive click events and sends `CANVAS:<x>,<y>` to external modules (coordinates relative to canvas origin)
*   Supports custom shape rendering from external modules using the enhanced protocol
*   Updates dynamically based on messages from connected modules

### `<dirlist>`

A directory listing element that displays files and directories in a specified path.

**Attributes:**

*   `id` (string): A unique identifier for the directory list element.
*   `x` (int): The x-coordinate of the directory list.
*   `y` (int): The y-coordinate of the directory list.
*   `width` (int): The width of the directory list.
*   `height` (int): The height of the directory list.
*   `label` (string): An optional label for the directory list.
*   `path` (string): The directory path to list contents from (default: current directory ".").

## Module Communication Protocol

The C-HTML framework supports communication with external modules via standard input/output pipes. This enables dynamic UI updates and custom rendering in canvas elements.

### Messages from UI to Module:

*   `SLIDER:<id>:<value>` - Sent when a slider value changes
*   `CANVAS:<x>,<y>` - Sent when a canvas is clicked (coordinates relative to canvas origin)

### Messages from Module to UI:

*   `CLEAR_SHAPES` - Clear all previously rendered shapes
*   `SHAPE;type;label;x;y;z;width;height;depth;r;g;b;a` - Render a shape in the canvas
    *   `type`: Shape type (e.g., "SQUARE", "RECT", "CIRCLE", "TRIANGLE")
    *   `label`: Unique identifier for the shape
    *   `x,y,z`: 3D coordinates
    *   `width,height,depth`: Dimensions
    *   `r,g,b,a`: RGBA color values (0.0-1.0)
*   `VAR;label;type;value` - Update a UI variable
    *   `label`: Variable name (should match element ID in UI)
    *   `type`: Variable type ("int", "float", "string")
    *   `value`: The variable value
*   `ARRAY;label;type;count;val1,val2,...` - Send an array of values
    *   `label`: Array name
    *   `type`: Array type ("int", "float")
    *   `count`: Number of elements
    *   `val1,val2,...`: Comma-separated values

### UI Variable Updates:

Variables sent from modules with matching labels to UI element IDs will automatically update those elements. This is particularly useful for updating text elements to display information based on canvas clicks or slider changes.
