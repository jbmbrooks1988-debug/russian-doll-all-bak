# ✅ TPMOS v6.00 Quiz: Answer Key
Check your answers against the official v6.00 standard. 🎓🏛️

---

### **Section 1: Core Philosophy**
1.  **Piece (Soul), Module (Brain), OS (Theater).**
2.  **The Mirror (state.txt)** is kept in a file to ensure persistence across crashes ("If it's not in a file, it's a lie").
3.  **Thin Brain mandate:** Modules should only route input and delegate logic to Ops; they should not perform heavy computation themselves.
4.  **Data Sovereignty:** The principle that a Piece owns its state exclusively and stores it locally.

### **Section 2: Standardized Ops & Fondu**
5.  `./fondu --install <project_id>`
6.  `pieces/os/ops_catalog.txt`
7.  The `ops_manifest.txt` lists the mapping of Op names to their binary paths so Fondu can register them.
8.  By updating `pieces/display/frame_changed.txt` (The Pulse).

### **Section 3: The Theater (CHTPM & GL)**
9.  `${variable_name}` - replaced at runtime by the CHTPM parser with values from state files.
10. An **Artifact** is a 3D object defined by an 8x8x8 voxel mask.
11. **Extrusion** pushed a 2D ASCII character into the Z-axis to create a 3D shape.
12. **Zero-Waste Polling** ensures the OS only renders when data actually changes, saving battery and CPU.

### **Section 4: Advanced Concepts (P2P & AI)**
13. By reading `known_hosts.pdl` and gossiping with discovered peers.
14. The **Ring Master** resolves state conflicts and maintains the global `chat_ledger.txt`.
15. A module that uses an AI Brain (LLM) to perform reasoning and execute Ops.
16. It reads a Piece's DNA, generates a prompt, and uses an LLM to write a backstory into `lore.txt`.

### **Section 5: Economics & Marketing**
17. By fetching live stock prices from Yahoo Finance and writing them directly into Stock Piece state files.
18. **The Architect, The Dreamer, and The Bot.**
19. The cumulative development time saved by reusing standardized, interoperable muscles.
20. **Exo-Sovereignty:** The ability of a node to function independently without external internet dependencies.

---
**Certification Level: Master Architect** 🏛️✨
