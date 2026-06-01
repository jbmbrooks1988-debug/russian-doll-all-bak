# 🛠️ Chapter 4: Development & The Standardized Ops Flow
Developing for TPMOS v6.00 requires a shift in mindset. You aren't building an "app"; you are building a **Collection of Ops** managed by a **Thin Brain**. 🏗️

---

## 🚀 The 5-Step Project Flow
Follow this sequence to build a project that is 100% compliant with the Standardized Ops Edition.

### 1. Define the DNA (`project.pdl`)
Tell the OS what your project is and where its heart lies.
```pdl
SECTION      | KEY                | VALUE
----------------------------------------
META         | project_id         | my_game
META         | entry_layout       | projects/my_game/layouts/main.chtpm
```

### 2. Create the View (`main.chtpm`)
Design your UI using CHTPM markup. Use `${vars}` to bind to future state.
```html
<layout id="my_game">
    <text label="Score: ${player_score}" />
    <button label="Jump" onClick="OP:my_game::jump" />
</layout>
```

### 3. Build the Muscles (`ops/*.c`)
Each capability (Jump, Shoot, Save) should be a standalone C file.
```c
// jump.c
int main() {
    int current_y = read_state("pieces/player/state.txt", "y");
    write_state("pieces/player/state.txt", "y", current_y + 1);
    trigger_pulse();
    return 0;
}
```

### 4. Register the Ops (`ops_manifest.txt`)
List your muscles so **Fondu** can register them.
```text
my_game::jump=projects/my_game/ops/+x/jump.+x
```

### 5. Install via Fondu 🫕
Run the installer to wire everything into the OS.
```bash
./fondu --install my_game
```

---

## 🛠️ The Developer's Toolkit
TPMOS provides several built-in tools to speed up this flow:

*   **`op-ed`**: A visual editor for creating Pieces and binding Ops to methods.
*   **`fuzz-op`**: A testing suite that hammers your Ops with random data to ensure they don't crash.
*   **`man-ops`**: A manual entry generator for your project.

---

## 💻 Code Example: A Standardized Makefile
In v6.00, your Makefile should always output to the `+x/` directory within your project folder.

```makefile
CC=gcc
CFLAGS=-O2 -Wall -Wextra
TARGETS=ops/+x/jump.+x ops/+x/shoot.+x

all: $(TARGETS)

ops/+x/%.+x: ops/%.c
	@mkdir -p ops/+x
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf ops/+x
```

---

## 🏛️ Scholar's Corner: The "Standardization Riot"
When Fondu and the Standardized Ops mandate were first introduced, there was a minor "riot" among developers who preferred the freedom of the legacy v5.01 system. They argued that creating separate binaries for every action was "too much work." However, the riot ended when they realized that Standardized Ops allowed them to **share** code instantly. A "Jump" muscle written for a platformer could be reused in an RPG with zero changes. Efficiency won the day! 🏛️📈

---

## 📝 Study Questions
1.  What are the 5 steps to building a Standardized Ops project?
2.  Why is it important to use `trigger_pulse()` at the end of an Op?
3.  What is the benefit of using `fondu --install` over manually copying files?
4.  **Exercise:** Design a `project.pdl` for a simple "Calculator" app.

---
[Return to Index](INDEX.md)
