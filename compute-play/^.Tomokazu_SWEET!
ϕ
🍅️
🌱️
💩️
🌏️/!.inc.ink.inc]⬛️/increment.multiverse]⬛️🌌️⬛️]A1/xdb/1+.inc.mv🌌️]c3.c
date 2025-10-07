#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_ELEMENTS 128
#define MAX_EVENTS 100
#define ELEMENTS_FILE "elements]a1.txt"

typedef struct {
    int year, day, hour; // In-game time
} Time;

typedef struct {
    char name[32];
    int protons, neutrons, electrons, col4, col5; // From elements]a1.txt
    double quantity; // Amount in universe
} Element;

typedef struct {
    char name[32];
    double probability; // Chance per update (0 to 1)
    int stage; // Stage when event can occur (-1 for any)
    void (*effect)(void*); // Function to apply effect
} Event;

typedef struct {
    char name[32];
    int year, day; // Scheduled time
} ScheduledEvent;

typedef struct {
    double energy;      // ⚡️ 能量
    double matter;      // 🪨 物质
    Element* elements;  // 🧪 化学物质
    int element_count;
    double cells;       // 🦠 细胞
    double creatures;   // 🐾 生物
    double humanoids;   // 👤 人形生物
    double technology;  // 💻 技术
    int universes;      // 🌌 宇宙
    int stage;          // 🕰️ 阶段
    Time time;          // In-game time
    int control_mode;   // 0: Computer, 1: Human
    ScheduledEvent events[MAX_EVENTS];
    int event_count;
} Multiverse;

// Event effect functions
void supernova_effect(void* m_ptr) {
    Multiverse* m = (Multiverse*)m_ptr;
    m->matter += 1e6;
    printf("Supernova 🌟 超新星: Matter increased by 1e6!\n");
}

void quantum_breakthrough_effect(void* m_ptr) {
    Multiverse* m = (Multiverse*)m_ptr;
    for (int i = 0; i < m->element_count; i++) {
        if (strcmp(m->elements[i].name, "H2") == 0 || strcmp(m->elements[i].name, "O2") == 0) {
            m->elements[i].quantity += 1e3;
        }
    }
    printf("Quantum Breakthrough ⚛️ 量子突破: H2 and O2 increased by 1e3!\n");
}

void alien_contact_effect(void* m_ptr) {
    Multiverse* m = (Multiverse*)m_ptr;
    m->technology += 0.5;
    printf("Alien Contact 👽 外星接触: Technology increased by 0.5!\n");
}

// Event definitions
Event event_list[] = {
    {"Supernova", 0.05, 1, supernova_effect},
    {"Quantum Breakthrough", 0.03, 2, quantum_breakthrough_effect},
    {"Alien Contact", 0.02, 5, alien_contact_effect}
};
int event_list_size = sizeof(event_list) / sizeof(event_list[0]);

void initialize_multiverse(Multiverse* m) {
    m->energy = 1e10;
    m->matter = 0;
    m->cells = 0;
    m->creatures = 0;
    m->humanoids = 0;
    m->technology = 0;
    m->universes = 1;
    m->stage = 0;
    m->time.year = 1;
    m->time.day = 1;
    m->time.hour = 0;
    m->event_count = 0;

    // Parse elements]a1.txt
    FILE* file = fopen(ELEMENTS_FILE, "r");
    if (!file) {
        printf("Error: Cannot open %s\n", ELEMENTS_FILE);
        exit(1);
    }
    m->elements = malloc(MAX_ELEMENTS * sizeof(Element));
    m->element_count = 0;
    char line[256];
    while (fgets(line, sizeof(line), file) && m->element_count < MAX_ELEMENTS) {
        Element* e = &m->elements[m->element_count];
        sscanf(line, "%s %d %d %d %d %d", e->name, &e->protons, &e->neutrons, &e->electrons, &e->col4, &e->col5);
        e->quantity = (strcmp(e->name, "H2") == 0 || strcmp(e->name, "O2") == 0) ? 1e3 : 0; // Initialize H2, O2
        m->element_count++;
    }
    fclose(file);
}

void advance_time(Time* t) {
    t->hour++;
    if (t->hour >= 24) {
        t->hour = 0;
        t->day++;
        if (t->day > 365) {
            t->day = 1;
            t->year++;
        }
    }
}

void print_time(Time* t) {
    printf("Time 🕰️ 时间: Year %d, Day %d, %02d:00\n", t->year, t->day, t->hour);
}

void print_status(Multiverse* m) {
    printf("\n=== Multiverse Status 🌌 ===\n");
    print_time(&m->time);
    printf("Universes 🌌 宇宙: %d\n", m->universes);
    printf("Energy ⚡️ 能量: %.2e\n", m->energy);
    printf("Matter 🪨 物质: %.2e\n", m->matter);
    printf("Elements 🧪 化学物质:\n");
    for (int i = 0; i < m->element_count; i++) {
        if (m->elements[i].quantity > 0) {
            printf("  %s: %.2e\n", m->elements[i].name, m->elements[i].quantity);
        }
    }
    printf("Cells 🦠 细胞: %.2e\n", m->cells);
    printf("Creatures 🐾 生物: %.2e\n", m->creatures);
    printf("Humanoids 👤 人形生物: %.2e\n", m->humanoids);
    printf("Technology 💻 技术: %.2e\n", m->technology);
    switch (m->stage) {
        case 0: printf("Stage 🕰️ 阶段: Big Bang 大爆炸\n"); break;
        case 1: printf("Stage 🕰️ 阶段: Matter Formation 物质形成\n"); break;
        case 2: printf("Stage 🕰️ 阶段: Chemical Evolution 化学进化\n"); break;
        case 3: printf("Stage 🕰️ 阶段: Cellular Life 细胞生命\n"); break;
        case 4: printf("Stage 🕰️ 阶段: Complex Creatures 复杂生物\n"); break;
        case 5: printf("Stage 🕰️ 阶段: Humanoid Civilizations 人形文明\n"); break;
        case 6: printf("Stage 🕰️ 阶段: Space Travel 太空旅行\n"); break;
    }
}

void form_water(Multiverse* m) {
    Element* h2 = NULL;
    Element* o2 = NULL;
    Element* water = NULL;
    for (int i = 0; i < m->element_count; i++) {
        if (strcmp(m->elements[i].name, "H2") == 0) h2 = &m->elements[i];
        if (strcmp(m->elements[i].name, "O2") == 0) o2 = &m->elements[i];
        if (strcmp(m->elements[i].name, "Water") == 0) water = &m->elements[i];
    }
    if (h2 && o2 && water && h2->quantity >= 2 && o2->quantity >= 1) {
        h2->quantity -= 2;
        o2->quantity -= 1;
        water->quantity += 2;
        printf("Reaction ⚗️ 反应: 2H2 + O2 -> 2H2O, Water increased by 2!\n");
    }
}

void check_random_events(Multiverse* m) {
    for (int i = 0; i < event_list_size; i++) {
        if (m->stage == event_list[i].stage || event_list[i].stage == -1) {
            if ((rand() % 1000) / 1000.0 < event_list[i].probability) {
                event_list[i].effect(m);
            }
        }
    }
}

void check_scheduled_events(Multiverse* m) {
    for (int i = 0; i < m->event_count; i++) {
        if (m->time.year == m->events[i].year && m->time.day == m->events[i].day) {
            for (int j = 0; j < event_list_size; j++) {
                if (strcmp(m->events[i].name, event_list[j].name) == 0) {
                    event_list[j].effect(m);
                    // Remove event
                    for (int k = i; k < m->event_count - 1; k++) {
                        m->events[k] = m->events[k + 1];
                    }
                    m->event_count--;
                    break;
                }
            }
        }
    }
}

void schedule_event(Multiverse* m, char* event_name, int year, int day) {
    if (m->event_count >= MAX_EVENTS) {
        printf("Error: Event queue full!\n");
        return;
    }
    strcpy(m->events[m->event_count].name, event_name);
    m->events[m->event_count].year = year;
    m->events[m->event_count].day = day;
    m->event_count++;
    printf("Scheduled %s at Year %d, Day %d\n", event_name, year, day);
}

void handle_human_input(Multiverse* m) {
    char input[256];
    printf("Enter command (e.g., 's supernova 1 10' to schedule Supernova at Year 1, Day 10, or 'q' to continue): ");
    if (fgets(input, sizeof(input), stdin)) {
        char command, event_name[32];
        int year, day;
        if (sscanf(input, "%c %s %d %d", &command, event_name, &year, &day) == 4 && command == 's') {
            schedule_event(m, event_name, year, day);
        }
    }
}

void update_multiverse(Multiverse* m) {
    advance_time(&m->time);
    check_random_events(m);
    check_scheduled_events(m);

    // Big Bang: Convert energy to matter
    if (m->stage == 0) {
        if (m->energy >= 1e9) {
            m->energy -= 1e9;
            m->matter += 1e6;
            if (m->matter >= 1e6) {
                m->stage = 1;
                printf("Universes cooling, matter forming... 🌌🪨\n");
            }
        }
    }
    // Matter: Form elements
    else if (m->stage == 1) {
        if (m->matter >= 1e5) {
            m->matter -= 1e5;
            for (int i = 0; i < m->element_count; i++) {
                if (strcmp(m->elements[i].name, "H2") == 0 || strcmp(m->elements[i].name, "O2") == 0) {
                    m->elements[i].quantity += 1e4;
                }
            }
            if (m->elements[0].quantity >= 1e4) { // Check H2 as proxy
                m->stage = 2;
                printf("Elements forming in cooling universes... 🧪\n");
            }
        }
    }
    // Elements: Form water and cells
    else if (m->stage == 2) {
        form_water(m);
        Element* water = NULL;
        for (int i = 0; i < m->element_count; i++) {
            if (strcmp(m->elements[i].name, "Water") == 0) water = &m->elements[i];
        }
        if (water && water->quantity >= 1e3) {
            water->quantity -= 1e3;
            m->cells += 1e2;
            if (m->cells >= 1e2) {
                m->stage = 3;
                printf("Simple cells emerging... 🦠\n");
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
                printf("Complex creatures evolving... 🐾\n");
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
                printf("Humanoids developing civilizations... 👤\n");
            }
        }
    }
    // Humanoids: Develop technology
    else if (m->stage == 5) {
        if (m->humanoids > 0) {
            m->humanoids -= 0.1;
            m->technology += 0.1;
            if (m->technology >= 1) {
                m->humanoids = 0;
                m->stage = 6;
                printf("Space travel achieved! 🚀\n");
            }
        }
    }
    // Space Travel: Expand multiverse
    else if (m->stage == 6) {
        if (m->technology >= 1) {
            m->technology -= 1;
            m->universes += 1;
            m->energy += 1e10;
            m->stage = 0;
            printf("New universe created in the multiverse! 🌌\n");
        }
    }
}

int main() {
    Multiverse m;
    srand(time(NULL));

    // Prompt for control mode
    printf("Select control mode (0: Computer 🤖, 1: Human 👤): ");
    int mode;
    scanf("%d", &mode);
    getchar(); // Clear newline
    m.control_mode = (mode == 1) ? 1 : 0;

    initialize_multiverse(&m);
    printf("Starting Multiverse Evolution... 🌌\n");

    while (1) {
        print_status(&m);
        if (m.control_mode == 1) {
            handle_human_input(&m);
        }
        update_multiverse(&m);
        sleep(1); // Update every second
    }

    free(m.elements);
    return 0;
}
