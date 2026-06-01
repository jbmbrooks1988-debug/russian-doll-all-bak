# 🧊 Chapter 20: THE OPENGL SHELL: CHTMGL & High-Fidelity
GL-OS is the high-fidelity evolution of the TPMOS Theater. It moves from 2D ASCII characters to 3D GPU-accelerated **Artifacts**. 🖥️🌌

---

## 🏗️ CHTMGL: OpenGL Markup
CHTMGL is an extension of the CHTPM language designed for 3D scene composition. It is parsed by `pieces/apps/gl_os/plugins/gltpm_parser.c`.

### New Tags
*   `<window title="..." camera="...">`: Defines the 3D viewport.
*   `<scene bg_color="R,G,B">`: The container for 3D artifacts.
*   `<artifact id="..." type="voxel" asset_id="..." />`: Renders a 3D object.
*   `<light type="point" pos="x,y,z" color="..." />`: Illuminates the scene.

---

## 🧊 Artifacts & Voxel Masks
In CHTMGL, we don't use complex 3D meshes (like .FBX or .OBJ) by default. We use **Voxel Masks**.
*   **8x8x8 Matrix:** Each Artifact is defined by a 512-bit mask.
*   **Procedural Generation:** The OS can "extrude" any ASCII character into a 3D voxel shape.
*   **High Performance:** Rendering 1,000 voxels is faster than rendering a single high-poly mesh, maintaining the "KISS" principle of TPMOS.

---

## 🎥 The Camera System
GL-OS supports multiple camera modes, switchable via Standardized Ops:
1.  **Ortho (Mode 0):** Pure 2D layout, matching the ASCII theater.
2.  **Isometric (Mode 1):** The classic "SimCity" or "Final Fantasy Tactics" view.
3.  **First-Person (Mode 2):** Full 3D immersion.
4.  **Follow (Mode 3):** Camera stays locked on a specific Piece.

---

## 💻 Code Example: Voxel Mask Parsing
From `gltpm_parser.c`:
```c
static void gltpm_load_artifact(const char* path, unsigned char mask[8][8]) {
    FILE *f = fopen(path, "r");
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (key[0] == 'z') {
            int z = atoi(key + 1);
            // Parse 8 hex bytes for this Z-layer
            for (int i = 0; i < 8; i++) {
                mask[z][i] = (unsigned char)strtol(token, NULL, 16);
            }
        }
    }
}
```

---

## 🏛️ Scholar's Corner: The "Ascii-to-Artifact" Bridge
The design of GL-OS was driven by the **"Bridge Principle."** We didn't want two different OSs; we wanted one OS with two views. A developer can write their game in ASCII (v5.01 style) and, by adding a single `<artifact>` tag to their Piece, see it instantly appear in 3D in GL-OS. This bridge ensures that 100% of legacy projects are "Upwardly Compatible." 🌉🧊

---

## 📝 Study Questions
1.  What is a Voxel Mask and how does it define a 3D Artifact?
2.  Name the four camera modes supported by GL-OS.
3.  How does CHTMGL differ from standard CHTPM?
4.  **Critical Thinking:** Why would a developer choose the OpenGL Shell over the ASCII Theater?

---
[Return to Index](INDEX.md)
