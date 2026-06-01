# 🗂️ Chapter 18: Dynamic Trait Menus: UI from DNA
In TPMOS v6.00, the UI is no longer static. It is **Procedurally Generated** based on the traits defined in a Piece's DNA. 🧬🖱️

---

## 🧬 Traits as UI Metadata
When the `chtpm_parser` encounters a Piece, it looks at its `<traits>` section in `piece.pdl`.

**Example DNA:**
```pdl
<traits>
    <trait>movable</trait>
    <trait>auditable</trait>
    <trait>tradeable</trait>
</traits>
```

The OS automatically generates a "Trait Menu":
*   **movable** → Adds "Move" button.
*   **auditable** → Adds "View Ledger" button.
*   **tradeable** → Adds "Open Market" button.

---

## 🎛️ The Dynamic Layout Engine
Using the `<layout>` tag in CHTPM, developers can define "Zones" where these dynamic menus appear.

```html
<layout id="inspector">
    <text label="Piece: ${active_piece_id}" />
    <dynamic_menu source="active_piece::traits" />
</layout>
```

---

## 🧠 Method Binding
Each trait is bound to a Standardized Op.
*   The `movable` trait is hard-coded to look for the `user::move_entity` op.
*   Custom traits can be defined in `pieces/system/traits_catalog.txt`.

---

## 💻 Code Example: Trait-to-UI Mapping
```c
void generate_menu(char* piece_id) {
    char** traits = get_traits(piece_id);
    for (int i = 0; i < trait_count; i++) {
        char* op = lookup_trait_op(traits[i]);
        add_button_to_frame(traits[i], op);
    }
}
```

---

## 🏛️ Scholar's Corner: The "Evolutionary UI"
A developer once created a project where Pieces could "mutate" and gain new traits over time. Because the UI was dynamic, the player saw new buttons appear as the Piece evolved. A simple "Rock" Piece eventually gained the `intelligent` trait, and a "Talk" button suddenly appeared on the screen. This **"Evolutionary UI"** is the pinnacle of the Piece-Method's flexibility. 🧬✨

---

## 📝 Study Questions
1.  How are UI menus generated in TPMOS v6.00?
2.  What is the `traits_catalog.txt`?
3.  Explain the relationship between a trait and a Standardized Op.
4.  **Exercise:** Add a `flammable` trait to a Piece's DNA and describe what the resulting UI button should do.

---
[Return to Index](INDEX.md)
