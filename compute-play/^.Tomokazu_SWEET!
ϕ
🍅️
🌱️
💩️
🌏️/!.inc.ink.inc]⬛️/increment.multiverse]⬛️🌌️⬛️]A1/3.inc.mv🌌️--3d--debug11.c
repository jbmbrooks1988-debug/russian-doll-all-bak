#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <GL/glut.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define MAX_ELEMENTS 128
#define MAX_EVENTS 100
#define MAX_TECHS 50
#define MAX_VISUALS 50
#define MAX_PARTICLES 100
#define MAX_MESSAGES 10
#define ELEMENTS_FILE "elements]a1.txt"
#define VISUAL_CONFIGS_FILE "visual_configs.txt"
#define TECH_TREE_FILE "tech_tree.txt"
const char *emoji_font_path = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";

typedef struct {
    int year, day, hour;
} Time;

typedef struct {
    float x, y, z;
    float vx, vy, vz;
} Particle;

typedef struct {
    char name[32];
    int protons, neutrons, electrons, col4, col5;
    double quantity;
    Particle particles[MAX_PARTICLES];
} Element;

typedef struct {
    char name[32];
    double probability;
    int stage;
    int sub_stage;
    void (*effect)(void*);
} Event;

typedef struct {
    char name[32];
    int year, day;
} ScheduledEvent;

typedef struct {
    char text[256];
    double timestamp;
} Message;

typedef struct {
    char name[32];
    char resource_name[32];
    double resource_amount;
    int stage, sub_stage, min_universes;
    char effect_resource[32];
    double effect_multiplier;
    char prerequisite[32];
    int unlocked;
} Tech;

typedef struct {
    char name[32];
    char shape[16];
    float size;
    float r, g, b;
} VisualConfig;

typedef struct {
    double energy;
    double matter;
    Element* elements;
    int element_count;
    double cells;
    double creatures;
    double humanoids;
    double technology;
    int universes;
    int stage;
    int sub_stage;
    Time time;
    int control_mode;
    ScheduledEvent events[MAX_EVENTS];
    int event_count;
    Message messages[MAX_MESSAGES];
    int message_count;
    Tech* techs;
    int tech_count;
    VisualConfig* visuals;
    int visual_count;
    int render_3d;
    int debug_mode;
} Multiverse;

// FreeType variables
FT_Library ft_library;
FT_Face ft_face;
float emoji_scale;

// Global variables
Multiverse* global_m;
float cam_z = 10.0;
static int update_count = 0;

// Event effect functions
void supernova_effect(void* m_ptr) {
    Multiverse* m = (Multiverse*)m_ptr;
    m->matter += 1e6;
    if (m->message_count < MAX_MESSAGES) {
        snprintf(m->messages[m->message_count].text, 256, "Supernova 🌟 超新星: Matter +1e6!");
        m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
        m->message_count++;
    }
    printf("Supernova 🌟 超新星: Matter increased by 1e6!\n");
}

void quantum_breakthrough_effect(void* m_ptr) {
    Multiverse* m = (Multiverse*)m_ptr;
    for (int i = 0; i < m->element_count; i++) {
        if (strcmp(m->elements[i].name, "H2") == 0 || strcmp(m->elements[i].name, "O2") == 0) {
            m->elements[i].quantity += 1e3;
        }
    }
    if (m->message_count < MAX_MESSAGES) {
        snprintf(m->messages[m->message_count].text, 256, "Quantum Breakthrough ⚛️ 量子突破: H2 and O2 +1e3!");
        m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
        m->message_count++;
    }
    printf("Quantum Breakthrough ⚛️ 量子突破: H2 and O2 increased by 1e3!\n");
}

void alien_contact_effect(void* m_ptr) {
    Multiverse* m = (Multiverse*)m_ptr;
    m->technology += 0.5;
    if (m->message_count < MAX_MESSAGES) {
        snprintf(m->messages[m->message_count].text, 256, "Alien Contact 👽 外星接触: Technology +0.5!");
        m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
        m->message_count++;
    }
    printf("Alien Contact 👽 外星接触: Technology increased by 0.5!\n");
}

void industrial_revolution_effect(void* m_ptr) {
    Multiverse* m = (Multiverse*)m_ptr;
    m->technology += 1.0;
    m->matter -= 1e4;
    if (m->message_count < MAX_MESSAGES) {
        snprintf(m->messages[m->message_count].text, 256, "Industrial Revolution 🏭: Technology +1.0, Matter -1e4!");
        m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
        m->message_count++;
    }
    printf("Industrial Revolution 🏭: Technology +1.0, Matter -1e4!\n");
}

// Event definitions
Event event_list[] = {
    {"Supernova", 0.05, 1, -1, supernova_effect},
    {"Quantum Breakthrough", 0.03, 2, -1, quantum_breakthrough_effect},
    {"Alien Contact", 0.02, 5, 4, alien_contact_effect},
    {"Industrial Revolution", 0.04, 5, 3, industrial_revolution_effect}
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
    m->sub_stage = 0;
    m->time.year = 1;
    m->time.day = 1;
    m->time.hour = 0;
    m->event_count = 0;
    m->message_count = 0;
    m->tech_count = 0;
    m->visual_count = 0;
    m->debug_mode = 0;

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
        e->quantity = (strcmp(e->name, "H2") == 0 || strcmp(e->name, "O2") == 0) ? 1e3 : 0;
        for (int i = 0; i < MAX_PARTICLES; i++) {
            e->particles[i].x = (rand() % 1000 / 500.0) - 1;
            e->particles[i].y = (rand() % 1000 / 500.0) - 1;
            e->particles[i].z = (rand() % 1000 / 500.0) - 1;
            e->particles[i].vx = ((rand() % 100) / 5000.0) - 0.01;
            e->particles[i].vy = ((rand() % 100) / 5000.0) - 0.01;
            e->particles[i].vz = ((rand() % 100) / 5000.0) - 0.01;
        }
        m->element_count++;
    }
    fclose(file);

    file = fopen(VISUAL_CONFIGS_FILE, "r");
    if (!file) {
        printf("Error: Cannot open %s\n", VISUAL_CONFIGS_FILE);
        exit(1);
    }
    m->visuals = malloc(MAX_VISUALS * sizeof(VisualConfig));
    m->visual_count = 0;
    while (fgets(line, sizeof(line), file) && m->visual_count < MAX_VISUALS) {
        VisualConfig* v = &m->visuals[m->visual_count];
        sscanf(line, "%s %s %f %f %f %f", v->name, v->shape, &v->size, &v->r, &v->g, &v->b);
        m->visual_count++;
    }
    fclose(file);

    file = fopen(TECH_TREE_FILE, "r");
    if (!file) {
        printf("Error: Cannot open %s\n", TECH_TREE_FILE);
        exit(1);
    }
    m->techs = malloc(MAX_TECHS * sizeof(Tech));
    m->tech_count = 0;
    while (fgets(line, sizeof(line), file) && m->tech_count < MAX_TECHS) {
        Tech* t = &m->techs[m->tech_count];
        sscanf(line, "%s %s %lf %d %d %s %lf %s", t->name, t->resource_name, &t->resource_amount, &t->stage, &t->sub_stage, t->effect_resource, &t->effect_multiplier, t->prerequisite);
        t->unlocked = 0;
        m->tech_count++;
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
    printf("Techs 🔧 技术树:\n");
    for (int i = 0; i < m->tech_count; i++) {
        if (m->techs[i].unlocked) {
            printf("  %s (Unlocked)\n", m->techs[i].name);
        }
    }
    switch (m->stage) {
        case 0: printf("Stage 🕰️ 阶段: Big Bang 大爆炸\n"); break;
        case 1: printf("Stage 🕰️ 阶段: Matter Formation 物质形成\n"); break;
        case 2: printf("Stage 🕰️ 阶段: Chemical Evolution 化学进化\n"); break;
        case 3: printf("Stage 🕰️ 阶段: Cellular Life 细胞生命\n"); break;
        case 4: printf("Stage 🕰️ 阶段: Complex Creatures 复杂生物\n"); break;
        case 5:
            switch (m->sub_stage) {
                case 0: printf("Stage 🕰️ 阶段: Humanoid Civilizations (Stone Age 🪨)\n"); break;
                case 1: printf("Stage 🕰️ 阶段: Humanoid Civilizations (Bronze Age 🗡️)\n"); break;
                case 2: printf("Stage 🕰️ 阶段: Humanoid Civilizations (Iron Age ⚔️)\n"); break;
                case 3: printf("Stage 🕰️ 阶段: Humanoid Civilizations (Industrial Age 🏭)\n"); break;
                case 4: printf("Stage 🕰️ 阶段: Humanoid Civilizations (Information Age 💾)\n"); break;
                case 5: printf("Stage 🕰️ 阶段: Humanoid Civilizations (Space Age 🚀)\n"); break;
            }
            break;
        case 6: printf("Stage 🕰️ 阶段: Space Travel 太空旅行\n"); break;
    }
    for (int i = 0; i < m->message_count; i++) {
        if ((double)clock() / CLOCKS_PER_SEC - m->messages[i].timestamp < 5.0) {
            printf("%s\n", m->messages[i].text);
        } else {
            for (int j = i; j < m->message_count - 1; j++) {
                m->messages[j] = m->messages[j + 1];
            }
            m->message_count--;
            i--;
        }
    }
}

void form_reactions(Multiverse* m) {
    Element* h2 = NULL;
    Element* o2 = NULL;
    Element* water = NULL;
    Element* ch4 = NULL;
    Element* co2 = NULL;
    for (int i = 0; i < m->element_count; i++) {
        if (strcmp(m->elements[i].name, "H2") == 0) h2 = &m->elements[i];
        if (strcmp(m->elements[i].name, "O2") == 0) o2 = &m->elements[i];
        if (strcmp(m->elements[i].name, "Water") == 0) water = &m->elements[i];
        if (strcmp(m->elements[i].name, "CH4") == 0) ch4 = &m->elements[i];
        if (strcmp(m->elements[i].name, "CO2") == 0) co2 = &m->elements[i];
    }

    double multiplier = 1.0;
    for (int i = 0; i < m->tech_count; i++) {
        if (m->techs[i].unlocked && strcmp(m->techs[i].effect_resource, "elements") == 0) {
            multiplier *= m->techs[i].effect_multiplier;
        }
    }

    if (h2 && o2 && water && h2->quantity >= 2 && o2->quantity >= 1) {
        h2->quantity -= 2;
        o2->quantity -= 1;
        water->quantity += 2 * multiplier;
        if (m->message_count < MAX_MESSAGES) {
            snprintf(m->messages[m->message_count].text, 256, "Reaction ⚗️: 2H2 + O2 -> 2H2O, Water +%.2f!", 2 * multiplier);
            m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
            m->message_count++;
        }
        printf("Reaction ⚗️: 2H2 + O2 -> 2H2O, Water increased by %.2f!\n", 2 * multiplier);
    }

    if (ch4 && o2 && co2 && water && ch4->quantity >= 1 && o2->quantity >= 2) {
        ch4->quantity -= 1;
        o2->quantity -= 2;
        co2->quantity += 1 * multiplier;
        water->quantity += 2 * multiplier;
        if (m->message_count < MAX_MESSAGES) {
            snprintf(m->messages[m->message_count].text, 256, "Reaction ⚗️: CH4 + 2O2 -> CO2 + 2H2O, CO2 +%.2f, Water +%.2f!", 1 * multiplier, 2 * multiplier);
            m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
            m->message_count++;
        }
        printf("Reaction ⚗️: CH4 + 2O2 -> CO2 + 2H2O, CO2 +%.2f, Water +%.2f!\n", 1 * multiplier, 2 * multiplier);
    }
}

void check_random_events(Multiverse* m) {
    for (int i = 0; i < event_list_size; i++) {
        if ((m->stage == event_list[i].stage || event_list[i].stage == -1) &&
            (m->sub_stage == event_list[i].sub_stage || event_list[i].sub_stage == -1)) {
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
    if (m->message_count < MAX_MESSAGES) {
        snprintf(m->messages[m->message_count].text, 256, "Scheduled %s at Year %d, Day %d", event_name, year, day);
        m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
        m->message_count++;
    }
    printf("Scheduled %s at Year %d, Day %d\n", event_name, year, day);
}

void unlock_tech(Multiverse* m, char* tech_name) {
    for (int i = 0; i < m->tech_count; i++) {
        if (strcmp(m->techs[i].name, tech_name) == 0 && !m->techs[i].unlocked) {
            int can_unlock = 1;
            if (m->stage < m->techs[i].stage || (m->stage == 5 && m->sub_stage < m->techs[i].sub_stage) || m->universes < m->techs[i].min_universes) {
                can_unlock = 0;
            }
            double resource_value = 0;
            if (strcmp(m->techs[i].resource_name, "Energy") == 0) resource_value = m->energy;
            else if (strcmp(m->techs[i].resource_name, "Matter") == 0) resource_value = m->matter;
            else if (strcmp(m->techs[i].resource_name, "Cells") == 0) resource_value = m->cells;
            else if (strcmp(m->techs[i].resource_name, "Creatures") == 0) resource_value = m->creatures;
            else if (strcmp(m->techs[i].resource_name, "Humanoids") == 0) resource_value = m->humanoids;
            else if (strcmp(m->techs[i].resource_name, "Technology") == 0) resource_value = m->technology;
            else {
                for (int j = 0; j < m->element_count; j++) {
                    if (strcmp(m->techs[i].resource_name, m->elements[j].name) == 0) {
                        resource_value = m->elements[j].quantity;
                        break;
                    }
                }
            }
            if (resource_value < m->techs[i].resource_amount) {
                can_unlock = 0;
            }
            if (strcmp(m->techs[i].prerequisite, "None") != 0) {
                int prereq_met = 0;
                for (int j = 0; j < m->tech_count; j++) {
                    if (strcmp(m->techs[j].name, m->techs[i].prerequisite) == 0 && m->techs[j].unlocked) {
                        prereq_met = 1;
                        break;
                    }
                }
                if (!prereq_met) can_unlock = 0;
            }
            if (can_unlock) {
                m->techs[i].unlocked = 1;
                if (m->message_count < MAX_MESSAGES) {
                    snprintf(m->messages[m->message_count].text, 256, "Tech Unlocked 🔧: %s", m->techs[i].name);
                    m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                    m->message_count++;
                }
                printf("Tech Unlocked 🔧: %s\n", m->techs[i].name);
            } else {
                if (m->message_count < MAX_MESSAGES) {
                    snprintf(m->messages[m->message_count].text, 256, "Cannot unlock %s: Prerequisites not met", tech_name);
                    m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                    m->message_count++;
                }
                printf("Cannot unlock %s: Prerequisites not met\n", tech_name);
            }
            break;
        }
    }
}

void set_stage(Multiverse* m, int stage, int sub_stage) {
    if (stage < 0 || stage > 6 || (stage == 5 && sub_stage > 5)) {
        printf("Invalid stage/sub-stage: %d/%d (stage 0-6, sub-stage 0-5 for stage 5)\n", stage, sub_stage);
        return;
    }
    m->stage = stage;
    m->sub_stage = (stage == 5) ? sub_stage : 0;
    switch (stage) {
        case 0:
            m->energy = 1e10;
            m->matter = 0;
            m->cells = 0;
            m->creatures = 0;
            m->humanoids = 0;
            m->technology = 0;
            break;
        case 1:
            m->energy = 1e9;
            m->matter = 1e6;
            break;
        case 2:
            m->matter = 1e5;
            for (int i = 0; i < m->element_count; i++) {
                if (strcmp(m->elements[i].name, "H2") == 0 || strcmp(m->elements[i].name, "O2") == 0) {
                    m->elements[i].quantity = 1e4;
                }
            }
            break;
        case 3:
            for (int i = 0; i < m->element_count; i++) {
                if (strcmp(m->elements[i].name, "Water") == 0) m->elements[i].quantity = 1e3;
            }
            m->cells = 1e2;
            break;
        case 4:
            m->cells = 1e2;
            m->creatures = 10;
            break;
        case 5:
            m->creatures = 10;
            m->humanoids = 1;
            m->technology = 0.5;
            switch (sub_stage) {
                case 0: m->matter = 1e4; break;
                case 1: m->matter = 1e4; m->technology = 1.0; break;
                case 2: m->matter = 1e4; m->technology = 2.0; break;
                case 3: m->matter = 1e5; m->technology = 5.0; break;
                case 4: m->matter = 1e5; m->technology = 10.0; break;
                case 5: m->matter = 1e5; m->technology = 20.0; break;
            }
            break;
        case 6:
            m->humanoids = 1;
            m->technology = 1;
            break;
    }
    if (m->message_count < MAX_MESSAGES) {
        char buffer[256];
        snprintf(buffer, 256, "Jumped to Stage %d, Sub-stage %d", stage, sub_stage);
        snprintf(m->messages[m->message_count].text, 256, "%s", buffer);
        m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
        m->message_count++;
    }
    printf("Jumped to Stage %d, Sub-stage %d\n", stage, sub_stage);
}

void handle_human_input(Multiverse* m) {
    char input[256];
    printf("Enter command (e.g., 's supernova 1 10', 't BronzeWorking', 'd stage 5 1', or 'q' to continue): ");
    if (fgets(input, sizeof(input), stdin)) {
        char command, arg1[32], arg2[32];
        int year, day, stage, sub_stage;
        if (sscanf(input, "%c %s %d %d", &command, arg1, &year, &day) == 4 && command == 's') {
            schedule_event(m, arg1, year, day);
        } else if (sscanf(input, "%c %s", &command, arg1) == 2 && command == 't') {
            unlock_tech(m, arg1);
        } else if (sscanf(input, "%c %s %d %d", &command, arg1, &stage, &sub_stage) == 4 && command == 'd' && strcmp(arg1, "stage") == 0) {
            set_stage(m, stage, sub_stage);
        } else if (sscanf(input, "%c %s %d", &command, arg1, &stage) == 3 && command == 'd' && strcmp(arg1, "stage") == 0) {
            set_stage(m, stage, 0);
        }
    }
}

void apply_tech_effects(Multiverse* m, double* energy, double* matter, double* cells, double* creatures, double* humanoids, double* technology) {
    for (int i = 0; i < m->tech_count; i++) {
        if (m->techs[i].unlocked) {
            if (strcmp(m->techs[i].effect_resource, "energy") == 0) *energy *= m->techs[i].effect_multiplier;
            else if (strcmp(m->techs[i].effect_resource, "matter") == 0) *matter *= m->techs[i].effect_multiplier;
            else if (strcmp(m->techs[i].effect_resource, "cells") == 0) *cells *= m->techs[i].effect_multiplier;
            else if (strcmp(m->techs[i].effect_resource, "creatures") == 0) *creatures *= m->techs[i].effect_multiplier;
            else if (strcmp(m->techs[i].effect_resource, "humanoids") == 0) *humanoids *= m->techs[i].effect_multiplier;
            else if (strcmp(m->techs[i].effect_resource, "technology") == 0) *technology *= m->techs[i].effect_multiplier;
        }
    }
}

void update_multiverse(Multiverse* m) {
    advance_time(&m->time);
    check_random_events(m);
    check_scheduled_events(m);

    double energy_delta = 0, matter_delta = 0, cells_delta = 0, creatures_delta = 0, humanoids_delta = 0, technology_delta = 0;

    if (m->stage == 0) {
        if (m->energy >= 1e9) {
            m->energy -= 1e9;
            matter_delta += 1e6;
            if (m->message_count < MAX_MESSAGES) {
                snprintf(m->messages[m->message_count].text, 256, "Universes cooling, matter forming... 🌌🪨");
                m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                m->message_count++;
            }
            if (m->matter + matter_delta >= 1e6) {
                m->stage = 1;
                if (m->message_count < MAX_MESSAGES) {
                    snprintf(m->messages[m->message_count].text, 256, "Advancing to Matter Formation 🪨");
                    m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                    m->message_count++;
                }
                printf("Universes cooling, matter forming... 🌌🪨\n");
            }
        }
    } else if (m->stage == 1) {
        if (m->matter >= 1e5) {
            m->matter -= 1e5;
            for (int i = 0; i < m->element_count; i++) {
                if (strcmp(m->elements[i].name, "H2") == 0 || strcmp(m->elements[i].name, "O2") == 0) {
                    m->elements[i].quantity += 1e4;
                } else if (strcmp(m->elements[i].name, "CH4") == 0) {
                    m->elements[i].quantity += 1e3;
                }
            }
            for (int i = 0; i < m->element_count; i++) {
                if (strcmp(m->elements[i].name, "H2") == 0 && m->elements[i].quantity >= 1e4) {
                    m->stage = 2;
                    if (m->message_count < MAX_MESSAGES) {
                        snprintf(m->messages[m->message_count].text, 256, "Elements forming in cooling universes... 🧪");
                        m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                        m->message_count++;
                    }
                    printf("Elements forming in cooling universes... 🧪\n");
                    break;
                }
            }
        }
    } else if (m->stage == 2) {
        form_reactions(m);
        Element* water = NULL;
        for (int i = 0; i < m->element_count; i++) {
            if (strcmp(m->elements[i].name, "Water") == 0) water = &m->elements[i];
        }
        if (water && water->quantity >= 1e3) {
            water->quantity -= 1e3;
            cells_delta += 1e2;
            if (m->cells + cells_delta >= 1e2) {
                m->stage = 3;
                if (m->message_count < MAX_MESSAGES) {
                    snprintf(m->messages[m->message_count].text, 256, "Simple cells emerging... 🦠");
                    m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                    m->message_count++;
                }
                printf("Simple cells emerging... 🦠\n");
            }
        }
    } else if (m->stage == 3) {
        if (m->cells >= 1e2) {
            m->cells -= 1e2;
            creatures_delta += 10;
            if (m->creatures + creatures_delta >= 10) {
                m->stage = 4;
                if (m->message_count < MAX_MESSAGES) {
                    snprintf(m->messages[m->message_count].text, 256, "Complex creatures evolving... 🐾");
                    m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                    m->message_count++;
                }
                printf("Complex creatures evolving... 🐾\n");
            }
        }
    } else if (m->stage == 4) {
        if (m->creatures >= 10) {
            m->creatures -= 10;
            humanoids_delta += 1;
            if (m->humanoids + humanoids_delta >= 1) {
                m->stage = 5;
                m->sub_stage = 0;
                if (m->message_count < MAX_MESSAGES) {
                    snprintf(m->messages[m->message_count].text, 256, "Humanoids developing civilizations (Stone Age)... 🪨👤");
                    m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                    m->message_count++;
                }
                printf("Humanoids developing civilizations (Stone Age)... 🪨👤\n");
            }
        }
    } else if (m->stage == 5) {
        if (m->humanoids > 0) {
            humanoids_delta += 0.1;
            technology_delta += 0.1;
            matter_delta -= 100;
            if (matter_delta < -m->matter) matter_delta = -m->matter;

            if (m->sub_stage == 0 && m->technology >= 1.0) {
                m->sub_stage = 1;
                if (m->message_count < MAX_MESSAGES) {
                    snprintf(m->messages[m->message_count].text, 256, "Advancing to Bronze Age 🗡️: Tools and trade emerge!");
                    m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                    m->message_count++;
                }
                printf("Advancing to Bronze Age 🗡️: Tools and trade emerge!\n");
            } else if (m->sub_stage == 1 && m->technology >= 2.0) {
                m->sub_stage = 2;
                if (m->message_count < MAX_MESSAGES) {
                    snprintf(m->messages[m->message_count].text, 256, "Advancing to Iron Age ⚔️: Stronger weapons and structures!");
                    m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                    m->message_count++;
                }
                printf("Advancing to Iron Age ⚔️: Stronger weapons and structures!\n");
            } else if (m->sub_stage == 2 && m->technology >= 5.0) {
                m->sub_stage = 3;
                if (m->message_count < MAX_MESSAGES) {
                    snprintf(m->messages[m->message_count].text, 256, "Advancing to Industrial Age 🏭: Factories and mass production!");
                    m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                    m->message_count++;
                }
                printf("Advancing to Industrial Age 🏭: Factories and mass production!\n");
            } else if (m->sub_stage == 3 && m->technology >= 10.0) {
                m->sub_stage = 4;
                if (m->message_count < MAX_MESSAGES) {
                    snprintf(m->messages[m->message_count].text, 256, "Advancing to Information Age 💾: Computers and networks!");
                    m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                    m->message_count++;
                }
                printf("Advancing to Information Age 💾: Computers and networks!\n");
            } else if (m->sub_stage == 4 && m->technology >= 20.0) {
                m->sub_stage = 5;
                if (m->message_count < MAX_MESSAGES) {
                    snprintf(m->messages[m->message_count].text, 256, "Advancing to Space Age 🚀: Preparing for interstellar travel!");
                    m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                    m->message_count++;
                }
                printf("Advancing to Space Age 🚀: Preparing for interstellar travel!\n");
            } else if (m->sub_stage == 5 && m->technology >= 30.0) {
                m->stage = 6;
                if (m->message_count < MAX_MESSAGES) {
                    snprintf(m->messages[m->message_count].text, 256, "Space travel achieved! 🚀");
                    m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                    m->message_count++;
                }
                printf("Space travel achieved! 🚀\n");
            }
        }
    } else if (m->stage == 6) {
        if (m->technology >= 1) {
            m->technology -= 1;
            m->universes += 1;
            energy_delta += 1e10;
            m->stage = 0;
            m->sub_stage = 0;
            if (m->message_count < MAX_MESSAGES) {
                snprintf(m->messages[m->message_count].text, 256, "New universe created in the multiverse! 🌌");
                m->messages[m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                m->message_count++;
            }
            printf("New universe created in the multiverse! 🌌\n");
        }
    }

    apply_tech_effects(m, &energy_delta, &matter_delta, &cells_delta, &creatures_delta, &humanoids_delta, &technology_delta);
    m->energy += energy_delta;
    m->matter += matter_delta;
    m->cells += cells_delta;
    m->creatures += creatures_delta;
    m->humanoids += humanoids_delta;
    m->technology += technology_delta;

    printf("Update #%d at %.3fs\n", ++update_count, (double)clock() / CLOCKS_PER_SEC);
}

// FreeType emoji rendering
typedef struct {
    GLuint texture;
    int width, height;
    int advance_x;
    int bearing_x, bearing_y;
    unsigned int codepoint;
} Glyph;

Glyph glyphs[256];
int glyph_count = 0;

int decode_utf8(const unsigned char* str, unsigned int* codepoint) {
    if (str[0] < 0x80) {
        *codepoint = str[0];
        return 1;
    }
    if ((str[0] & 0xE0) == 0xC0) {
        if ((str[1] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
            return 2;
        }
    }
    if ((str[0] & 0xF0) == 0xE0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
            return 3;
        }
    }
    if ((str[0] & 0xF8) == 0xF0) {
        if ((str[1] & 0xC0) == 0x80 && (str[2] & 0xC0) == 0x80 && (str[3] & 0xC0) == 0x80) {
            *codepoint = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
            return 4;
        }
    }
    *codepoint = '?';
    return 1;
}

void init_freetype() {
    if (FT_Init_FreeType(&ft_library)) {
        printf("Error: FreeType initialization failed\n");
        exit(1);
    }
    if (FT_New_Face(ft_library, emoji_font_path, 0, &ft_face)) {
        printf("Error: Failed to load font %s\n", emoji_font_path);
        exit(1);
    }
    FT_Set_Pixel_Sizes(ft_face, 0, 24); // Set to 24px
    int loaded_emoji_size = ft_face->size->metrics.y_ppem;
    emoji_scale = 32.0f / loaded_emoji_size; // Fit within 32px
    printf("Emoji font loaded, size: %d, scale: %f\n", loaded_emoji_size, emoji_scale);
}

void load_glyph(unsigned int codepoint) {
    if (glyph_count >= 256 || FT_Load_Char(ft_face, codepoint, FT_LOAD_RENDER | FT_LOAD_COLOR)) return;

    FT_GlyphSlot slot = ft_face->glyph;
    glGenTextures(1, &glyphs[glyph_count].texture);
    glBindTexture(GL_TEXTURE_2D, glyphs[glyph_count].texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->bitmap.width, slot->bitmap.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glyphs[glyph_count].width = slot->bitmap.width;
    glyphs[glyph_count].height = slot->bitmap.rows;
    glyphs[glyph_count].advance_x = slot->advance.x >> 6;
    glyphs[glyph_count].bearing_x = slot->bitmap_left;
    glyphs[glyph_count].bearing_y = slot->bitmap_top;
    glyphs[glyph_count].codepoint = codepoint;
    glyph_count++;
}

void render_text_freetype(float x, float y, const char* text, float z) {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const unsigned char* str = (const unsigned char*)text;
    while (*str) {
        unsigned int codepoint;
        int len = decode_utf8(str, &codepoint);
        if (codepoint >= 0x1F300 && codepoint <= 0x1F5FF) { // Emoji range
            int idx = -1;
            for (int i = 0; i < glyph_count; i++) {
                if (glyphs[i].codepoint == codepoint) {
                    idx = i;
                    break;
                }
            }
            if (idx == -1) {
                load_glyph(codepoint);
                idx = glyph_count - 1;
            }
            if (glyphs[idx].texture) {
                glBindTexture(GL_TEXTURE_2D, glyphs[idx].texture);
                float w = glyphs[idx].width * emoji_scale;
                float h = glyphs[idx].height * emoji_scale;
                float x2 = x + glyphs[idx].bearing_x * emoji_scale - w / 2;
                float y2 = y - (glyphs[idx].height - glyphs[idx].bearing_y) * emoji_scale + h / 2;
                glBegin(GL_QUADS);
                glTexCoord2f(0, 1); glVertex3f(x2, y2, z);
                glTexCoord2f(1, 1); glVertex3f(x2 + w, y2, z);
                glTexCoord2f(1, 0); glVertex3f(x2 + w, y2 + h, z);
                glTexCoord2f(0, 0); glVertex3f(x2, y2 + h, z);
                glEnd();
                x += glyphs[idx].advance_x * emoji_scale;
            }
        }
        str += len;
    }
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

void render_text_bitmap(const char* text, float x, float y, float z) {
    glRasterPos3f(x, y, z);
    while (*text) {
        unsigned int codepoint;
        int len = decode_utf8((const unsigned char*)text, &codepoint);
        if (codepoint < 0x1F300 || codepoint > 0x1F5FF) { // Non-emoji
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, codepoint);
        }
        text += len;
    }
}

void render_3d() {
    static int frame_count = 0;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(0, 0, cam_z, 0, 0, 0, 0, 1, 0);

    for (int i = 0; i < global_m->visual_count; i++) {
        if (strcmp(global_m->visuals[i].name, "Universe") == 0 && strcmp(global_m->visuals[i].shape, "sphere") == 0) {
            glPushMatrix();
            glTranslatef(0, 0, 0);
            glColor3f(global_m->visuals[i].r, global_m->visuals[i].g, global_m->visuals[i].b);
            glutSolidSphere(global_m->visuals[i].size * global_m->universes, 20, 20);
            glPopMatrix();
        }
    }

    if (global_m->stage == 5) {
        for (int i = 0; i < global_m->visual_count; i++) {
            if (strcmp(global_m->visuals[i].name, "City") == 0 && strcmp(global_m->visuals[i].shape, "city") == 0) {
                glPushMatrix();
                glTranslatef(0, 0, 0);
                glColor3f(global_m->visuals[i].r, global_m->visuals[i].g, global_m->visuals[i].b);
                glutSolidCube(global_m->visuals[i].size * global_m->humanoids);
                glPopMatrix();
            }
        }
    }

    glPointSize(2.0);
    glBegin(GL_POINTS);
    for (int i = 0; i < global_m->element_count; i++) {
        if (global_m->elements[i].quantity > 0) {
            for (int j = 0; j < global_m->visual_count; j++) {
                if (strcmp(global_m->elements[i].name, global_m->visuals[j].name) == 0 && strcmp(global_m->visuals[j].shape, "point") == 0) {
                    glPointSize(global_m->visuals[j].size * 10);
                    glColor3f(global_m->visuals[j].r, global_m->visuals[j].g, global_m->visuals[j].b);
                    int particle_count = (int)(global_m->elements[i].quantity / 1000);
                    if (particle_count > MAX_PARTICLES) particle_count = MAX_PARTICLES;
                    float avg_x = 0, avg_y = 0, avg_z = 0;
                    int valid_particles = 0;
                    for (int k = 0; k < particle_count; k++) {
                        Particle* p = &global_m->elements[i].particles[k];
                        p->x += p->vx;
                        p->y += p->vy;
                        p->z += p->vz;
                        if (p->x < -1 || p->x > 1) p->vx = -p->vx;
                        if (p->y < -1 || p->y > 1) p->vy = -p->vy;
                        if (p->z < -1 || p->z > 1) p->vz = -p->vz;
                        glVertex3f(p->x, p->y, p->z);
                        avg_x += p->x;
                        avg_y += p->y;
                        avg_z += p->z;
                        valid_particles++;
                    }
                    if (valid_particles > 0) {
                        avg_x /= valid_particles;
                        avg_y /= valid_particles;
                        avg_z /= valid_particles;
                        GLdouble model[16], proj[16];
                        GLint viewport[4];
                        GLdouble win_x, win_y, win_z;
                        glGetDoublev(GL_MODELVIEW_MATRIX, model);
                        glGetDoublev(GL_PROJECTION_MATRIX, proj);
                        glGetIntegerv(GL_VIEWPORT, viewport);
                        gluProject(avg_x, avg_y, avg_z, model, proj, viewport, &win_x, &win_y, &win_z);
                        glMatrixMode(GL_PROJECTION);
                        glPushMatrix();
                        glLoadIdentity();
                        gluOrtho2D(0, 800, 0, 600);
                        glMatrixMode(GL_MODELVIEW);
                        glPushMatrix();
                        glLoadIdentity();
                        glColor3f(1.0, 1.0, 1.0);
                        render_text_bitmap(global_m->elements[i].name, win_x, 600 - win_y + i * 15, 0.0);
                        glMatrixMode(GL_PROJECTION);
                        glPopMatrix();
                        glMatrixMode(GL_MODELVIEW);
                        glPopMatrix();
                    }
                }
            }
        }
    }
    glEnd();

    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 600);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    char buffer[256];
    snprintf(buffer, 256, "Universes 🌌 宇宙: %d", global_m->universes);
    render_text_freetype(10, 580, buffer, -0.1);
    render_text_bitmap(buffer, 10, 580, 0.0);
    snprintf(buffer, 256, "Energy ⚡️ 能量: %.2e", global_m->energy);
    render_text_freetype(10, 560, buffer, -0.1);
    render_text_bitmap(buffer, 10, 560, 0.0);
    snprintf(buffer, 256, "Matter 🪨 物质: %.2e", global_m->matter);
    render_text_freetype(10, 540, buffer, -0.1);
    render_text_bitmap(buffer, 10, 540, 0.0);
    snprintf(buffer, 256, "Elements 🧪 化学物质:");
    render_text_freetype(10, 520, buffer, -0.1);
    render_text_bitmap(buffer, 10, 520, 0.0);
    int y_offset = 500;
    for (int i = 0; i < global_m->element_count; i++) {
        if (global_m->elements[i].quantity > 0) {
            snprintf(buffer, 256, "  %s: %.2e", global_m->elements[i].name, global_m->elements[i].quantity);
            render_text_freetype(10, y_offset, buffer, -0.1);
            render_text_bitmap(buffer, 10, y_offset, 0.0);
            y_offset -= 20;
        }
    }
    snprintf(buffer, 256, "Cells 🦠 细胞: %.2e", global_m->cells);
    render_text_freetype(10, y_offset, buffer, -0.1);
    render_text_bitmap(buffer, 10, y_offset, 0.0);
    y_offset -= 20;
    snprintf(buffer, 256, "Creatures 🐾 生物: %.2e", global_m->creatures);
    render_text_freetype(10, y_offset, buffer, -0.1);
    render_text_bitmap(buffer, 10, y_offset, 0.0);
    y_offset -= 20;
    snprintf(buffer, 256, "Humanoids 👤 人形生物: %.2e", global_m->humanoids);
    render_text_freetype(10, y_offset, buffer, -0.1);
    render_text_bitmap(buffer, 10, y_offset, 0.0);
    y_offset -= 20;
    snprintf(buffer, 256, "Technology 💻 技术: %.2e", global_m->technology);
    render_text_freetype(10, y_offset, buffer, -0.1);
    render_text_bitmap(buffer, 10, y_offset, 0.0);
    y_offset -= 20;
    snprintf(buffer, 256, "Techs 🔧 技术树:");
    render_text_freetype(10, y_offset, buffer, -0.1);
    render_text_bitmap(buffer, 10, y_offset, 0.0);
    y_offset -= 20;
    for (int i = 0; i < global_m->tech_count; i++) {
        if (global_m->techs[i].unlocked) {
            snprintf(buffer, 256, "  %s (Unlocked)", global_m->techs[i].name);
            render_text_freetype(10, y_offset, buffer, -0.1);
            render_text_bitmap(buffer, 10, y_offset, 0.0);
            y_offset -= 20;
        }
    }
    switch (global_m->stage) {
        case 0: snprintf(buffer, 256, "Stage 🕰️ 阶段: Big Bang 大爆炸"); break;
        case 1: snprintf(buffer, 256, "Stage 🕰️ 阶段: Matter Formation 物质形成"); break;
        case 2: snprintf(buffer, 256, "Stage 🕰️ 阶段: Chemical Evolution 化学进化"); break;
        case 3: snprintf(buffer, 256, "Stage 🕰️ 阶段: Cellular Life 细胞生命"); break;
        case 4: snprintf(buffer, 256, "Stage 🕰️ 阶段: Complex Creatures 复杂生物"); break;
        case 5:
            switch (global_m->sub_stage) {
                case 0: snprintf(buffer, 256, "Stage 🕰️ 阶段: Humanoid Civilizations (Stone Age 🪨)"); break;
                case 1: snprintf(buffer, 256, "Stage 🕰️ 阶段: Humanoid Civilizations (Bronze Age 🗡️)"); break;
                case 2: snprintf(buffer, 256, "Stage 🕰️ 阶段: Humanoid Civilizations (Iron Age ⚔️)"); break;
                case 3: snprintf(buffer, 256, "Stage 🕰️ 阶段: Humanoid Civilizations (Industrial Age 🏭)"); break;
                case 4: snprintf(buffer, 256, "Stage 🕰️ 阶段: Humanoid Civilizations (Information Age 💾)"); break;
                case 5: snprintf(buffer, 256, "Stage 🕰️ 阶段: Humanoid Civilizations (Space Age 🚀)"); break;
            }
            break;
        case 6: snprintf(buffer, 256, "Stage 🕰️ 阶段: Space Travel 太空旅行"); break;
    }
    render_text_freetype(10, y_offset, buffer, -0.1);
    render_text_bitmap(buffer, 10, y_offset, 0.0);
    y_offset -= 20;
    snprintf(buffer, 256, "Time 🕰️ 时间: Year %d, Day %d, %02d:00", global_m->time.year, global_m->time.day, global_m->time.hour);
    render_text_freetype(10, y_offset, buffer, -0.1);
    render_text_bitmap(buffer, 10, y_offset, 0.0);
    y_offset -= 20;
    if (global_m->debug_mode) {
        snprintf(buffer, 256, "Debug Mode: 0.1s/update");
        render_text_freetype(10, y_offset, buffer, -0.1);
        render_text_bitmap(buffer, 10, y_offset, 0.0);
        y_offset -= 20;
    }
    snprintf(buffer, 256, "Frame #%d", ++frame_count);
    render_text_freetype(10, y_offset, buffer, -0.1);
    render_text_bitmap(buffer, 10, y_offset, 0.0);
    y_offset -= 20;
    snprintf(buffer, 256, "Zoom: %.1f (use +,-)", cam_z);
    render_text_freetype(10, y_offset, buffer, -0.1);
    render_text_bitmap(buffer, 10, y_offset, 0.0);
    y_offset -= 20;
    for (int i = 0; i < global_m->message_count; i++) {
        if ((double)clock() / CLOCKS_PER_SEC - global_m->messages[i].timestamp < 5.0) {
            render_text_freetype(10, y_offset, global_m->messages[i].text, -0.1);
            render_text_bitmap(global_m->messages[i].text, 10, y_offset, 0.0);
            y_offset -= 20;
        } else {
            for (int j = i; j < global_m->message_count - 1; j++) {
                global_m->messages[j] = global_m->messages[j + 1];
            }
            global_m->message_count--;
            i--;
        }
    }
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glDisable(GL_DEPTH_TEST);

    glutSwapBuffers();
}

void keyboard_func(unsigned char key, int x, int y) {
    switch (key) {
        case '+':
            cam_z -= 0.5;
            if (cam_z < 2.0) cam_z = 2.0;
            if (global_m->message_count < MAX_MESSAGES) {
                snprintf(global_m->messages[global_m->message_count].text, 256, "Zoom in: cam_z = %.1f", cam_z);
                global_m->messages[global_m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                global_m->message_count++;
            }
            printf("Zoom in: cam_z = %.1f\n", cam_z);
            glutPostRedisplay();
            break;
        case '-':
            cam_z += 0.5;
            if (cam_z > 20.0) cam_z = 20.0;
            if (global_m->message_count < MAX_MESSAGES) {
                snprintf(global_m->messages[global_m->message_count].text, 256, "Zoom out: cam_z = %.1f", cam_z);
                global_m->messages[global_m->message_count].timestamp = (double)clock() / CLOCKS_PER_SEC;
                global_m->message_count++;
            }
            printf("Zoom out: cam_z = %.1f\n", cam_z);
            glutPostRedisplay();
            break;
    }
}

void timer_func(int value) {
    update_multiverse(global_m);
    glutPostRedisplay();
    glutTimerFunc(global_m->debug_mode ? 100 : 1000, timer_func, 0);
}

void idle() {
    glutPostRedisplay();
}

int main(int argc, char* argv[]) {
    Multiverse* m = malloc(sizeof(Multiverse));
    if (!m) {
        printf("Error: Memory allocation failed\n");
        return 1;
    }
    global_m = m;
    m->render_3d = 0;
    m->debug_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--3d") == 0) {
            m->render_3d = 1;
            printf("3D mode enabled\n");
        } else if (strcmp(argv[i], "--debug") == 0) {
            m->debug_mode = 1;
            printf("Debug mode enabled: Time accelerated to 0.1s/update\n");
        }
    }

    srand(time(NULL));
    printf("Select control mode (0: Computer 🤖, 1: Human 👤): ");
    int mode;
    scanf("%d", &mode);
    getchar();
    m->control_mode = (mode == 1) ? 1 : 0;

    initialize_multiverse(m);
    init_freetype();
    printf("Starting Multiverse Evolution... 🌌\n");

    if (m->render_3d) {
        glutInit(&argc, argv);
        glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
        glutInitWindowSize(800, 600);
        glutCreateWindow("Multiverse Evolution");
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glMatrixMode(GL_PROJECTION);
        gluPerspective(45, 800.0/600.0, 0.1, 100);
        glMatrixMode(GL_MODELVIEW);
        glutDisplayFunc(render_3d);
        glutIdleFunc(idle);
        glutKeyboardFunc(keyboard_func);
        glutTimerFunc(global_m->debug_mode ? 100 : 1000, timer_func, 0);
        glutMainLoop();
    } else {
        clock_t last_update = clock();
        double accumulated_time = 0.0;
        double interval = m->debug_mode ? 0.1 : 1.0;
        while (1) {
            clock_t now = clock();
            double elapsed = (double)(now - last_update) / CLOCKS_PER_SEC;
            accumulated_time += elapsed;
            last_update = now;
            while (accumulated_time >= interval) {
                print_status(m);
                if (m->control_mode == 1 && !m->debug_mode) {
                    handle_human_input(m);
                }
                update_multiverse(m);
                accumulated_time -= interval;
            }
        }
    }

    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_library);
    free(m->elements);
    free(m->visuals);
    free(m->techs);
    free(m);
    return 0;
}
