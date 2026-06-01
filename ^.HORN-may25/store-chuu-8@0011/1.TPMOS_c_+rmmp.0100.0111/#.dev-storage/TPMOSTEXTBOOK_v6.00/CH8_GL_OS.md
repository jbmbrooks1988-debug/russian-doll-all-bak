# 🧊 Chapter 8: GL-OS: The Transition to High-Fidelity
While TPMOS was born in the world of ASCII, version 6.00 marks the beginning of the **High-Fidelity Era**. GL-OS is the bridge between the text terminal and the 3D GPU. 🖥️🌌

---

## 🎭 The Dual-Mode Theater
The CHTPM Parser (Chapter 19) is now designed to output to two different targets:
1.  **ASCII Target:** Renders to the terminal using standard characters.
2.  **OpenGL Target (CHTMGL):** Renders to a 3D window using **Artifacts** and **Textures**.

---

## 🏗️ What are Artifacts?
In the OpenGL Shell, we don't have "characters"; we have **Artifacts**.
*   **Voxel Masks:** 8x8x8 bitmasks that define a 3D shape (e.g., a "Sword" or a "Tree").
*   **Extrusion:** A technique where a 2D ASCII character is "pushed" into the 3D Z-axis to create depth.

---

## 🧠 The GL-OS Module Design
A GL-OS module works identically to a standard module, but it listens for different input events:
*   **Mouse Clicks (X,Y):** Converted from screen coordinates to Piece-relative coordinates.
*   **Camera Swipes:** Used to rotate the 3D viewport.

### Standardized Ops in GL
Ops remain identical! If you have a `move_entity` op, it doesn't care if it's being rendered in ASCII or 3D. It simply updates `state.txt`. The Theater (GL-OS) handles the visual translation.

---

## 💻 Code Example: Artifact Loading (Simplified)
```c
void load_artifact(const char* name) {
    char path[256];
    snprintf(path, sizeof(path), "assets/artifacts/%s.txt", name);
    // Read 8x8x8 bitmask from file
    unsigned char mask[8][8];
    read_bitmask_file(path, mask);
    // Upload to GPU as a 3D Texture or Voxel Mesh
    upload_to_gpu(mask);
}
```

---

## 🏛️ Scholar's Corner: The "First Dimension Shift"
The first time a developer toggled a TPMOS project from ASCII to GL, they saw their 2D character suddenly "stand up" into 3D. It was a simple "extrude" effect, but it changed the project forever. This moment, known as the **"First Dimension Shift,"** proved that the Piece Method was truly scale-free—logic doesn't care about the number of dimensions it exists in. 🧊✨

---

## 📝 Study Questions
1.  What is an "Artifact" and how does it differ from an ASCII character?
2.  How does GL-OS maintain compatibility with existing Standardized Ops?
3.  Explain the concept of "Extrusion" in the context of CHTMGL.
4.  **Critical Thinking:** Why is it important to keep the "Brain" (Module) separate from the "Theater" (GL-OS)?

---
[Return to Index](INDEX.md)
