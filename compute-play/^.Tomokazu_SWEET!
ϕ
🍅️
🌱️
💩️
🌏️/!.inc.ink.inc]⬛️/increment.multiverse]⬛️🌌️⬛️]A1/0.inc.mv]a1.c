#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    double energy;      // Energy from Big Bang
    double matter;      // Matter forming universes
    double chemicals;   // Chemicals in cooling universes
    double cells;       // Simple life forms
    double creatures;   // Complex life forms
    double humanoids;   // Intelligent beings
    double technology;  // Technological advancements
    int universes;      // Number of universes in multiverse
    int stage;          // Current stage of evolution
} Multiverse;

void initialize_multiverse(Multiverse *m) {
    m->energy = 1e10; // Starting energy from Big Bang
    m->matter = 0;
    m->chemicals = 0;
    m->cells = 0;
    m->creatures = 0;
    m->humanoids = 0;
    m->technology = 0;
    m->universes = 1;
    m->stage = 0; // 0: Big Bang, 1: Matter, 2: Chemicals, 3: Cells, 4: Creatures, 5: Humanoids, 6: Space Travel
}

void print_status(Multiverse *m) {
    printf("\n=== Multiverse Status ===\n");
    printf("Universes: %d\n", m->universes);
    printf("Energy: %.2e\n", m->energy);
    printf("Matter: %.2e\n", m->matter);
    printf("Chemicals: %.2e\n", m->chemicals);
    printf("Cells: %.2e\n", m->cells);
    printf("Creatures: %.2e\n", m->creatures);
    printf("Humanoids: %.2e\n", m->humanoids);
    printf("Technology: %.2e\n", m->technology);
    switch (m->stage) {
        case 0: printf("Stage: Big Bang\n"); break;
        case 1: printf("Stage: Matter Formation\n"); break;
        case 2: printf("Stage: Chemical Evolution\n"); break;
        case 3: printf("Stage: Cellular Life\n"); break;
        case 4: printf("Stage: Complex Creatures\n"); break;
        case 5: printf("Stage: Humanoid Civilizations\n"); break;
        case 6: printf("Stage: Space Travel\n"); break;
    }
}

void update_multiverse(Multiverse *m) {
    // Big Bang: Convert energy to matter
    if (m->stage == 0) {
        if (m->energy >= 1e9) {
            m->energy -= 1e9;
            m->matter += 1e6;
            if (m->matter >= 1e6) {
                m->stage = 1;
                printf("Universes cooling, matter forming...\n");
            }
        }
    }
    // Matter: Form chemicals
    else if (m->stage == 1) {
        if (m->matter >= 1e5) {
            m->matter -= 1e5;
            m->chemicals += 1e4;
            if (m->chemicals >= 1e4) {
                m->stage = 2;
                printf("Chemicals forming in cooling universes...\n");
            }
        }
    }
    // Chemicals: Form cells
    else if (m->stage == 2) {
        if (m->chemicals >= 1e3) {
            m->chemicals -= 1e3;
            m->cells += 1e2;
            if (m->cells >= 1e2) {
                m->stage = 3;
                printf("Simple cells emerging...\n");
            }
        }
    }
    // Cells: Form creatures
    else if (m->stage == 3) {
        if (m->cells >= 1e2) {
            m->cells -= 1e2;
            m->creatures += 10;
            if (m->creatures >= 10) {
                m->stage = 4;
                printf("Complex creatures evolving...\n");
            }
        }
    }
    // Creatures: Form humanoids
    else if (m->stage == 4) {
        if (m->creatures >= 10) {
            m->creatures -= 10;
            m->humanoids += 1;
            if (m->humanoids >= 1) {
                m->stage = 5;
                printf("Humanoids developing civilizations...\n");
            }
        }
    }
    // Humanoids: Develop technology
    else if (m->stage == 5) {
        if (m->humanoids > 0) { // Allow updates if humanoids are positive
            m->humanoids -= 0.1;
            m->technology += 0.1;
            if (m->technology >= 1) {
                m->humanoids = 0; // Reset to avoid floating-point issues
                m->stage = 6;
                printf("Space travel achieved!\n");
            }
        }
    }
    // Space Travel: Expand multiverse
    else if (m->stage == 6) {
        if (m->technology >= 1) {
            m->technology -= 1;
            m->universes += 1;
            m->energy += 1e10; // New universe adds energy
            m->stage = 0; // Reset to Big Bang for new universe
            printf("New universe created in the multiverse!\n");
        }
    }
}

int main() {
    Multiverse m;
    initialize_multiverse(&m);
    srand(time(NULL));

    printf("Starting Multiverse Evolution...\n");
    while (1) {
        print_status(&m);
        update_multiverse(&m);
        sleep(1); // Update every second
    }
    return 0;
}
