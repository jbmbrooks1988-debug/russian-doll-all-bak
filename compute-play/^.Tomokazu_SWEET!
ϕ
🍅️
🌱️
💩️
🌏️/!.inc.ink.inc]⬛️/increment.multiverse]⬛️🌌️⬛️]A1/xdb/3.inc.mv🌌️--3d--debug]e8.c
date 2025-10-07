#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <GL/glut.h>

#define MAX_ELEMENTS 128
#define MAX_EVENTS 100
#define MAX_TECHS 50
#define MAX_VISUALS 50
#define MAX_PARTICLES 100
#define ELEMENTS_FILE "elements]a1.txt"
#define VISUAL_CONFIGS_FILE "visual_configs.txt"
#define TECH_TREE_FILE "tech_tree.txt"

typedef struct {
    int year, day, hour; // In-game time
} Time;

typedef struct {
    float x, y, z; // Position
    float vx, vy, vz; // Velocity
} Particle;

typedef struct {
    char name[32];
    int protons, neutrons, electrons, col4, col5; // From elements]a1.txt
    double quantity; // Amount in universe
    Particle particles[MAX_PARTICLES]; // Persistent particles
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
    char name[32];
    char resource_name[32];
    double resource_amount;
    int stage, min_universes;
    char effect_resource[32];
    double effect_multiplier;
    int unlocked;
} Tech;

typedef struct {
    char name[32];
    char shape[16]; // sphere or point
    float size; // radius for sphere, point size for point
    float r, g, b; // RGB colors
} VisualConfig;

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
    Tech* techs;        // 🔧 技术树
    int tech_count;
    VisualConfig* visuals; // 🖼️ 视觉配置
    int visual_count;
    int render_3d;      // 0: Terminal, 1: 3D
    int debug_mode;     // 0: Normal, 1: Fast time
} Multiverse;

// Global Multiverse pointer for GLUT callbacks
Multiverse* global_m;

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
    m->tech_count = 0;
    m->visual_count = 0;
    m->debug_mode = 0;

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
        e->quantity = (strcmp(e->name, "H2") == 0 || strcmp(e->name, "O2") == 0) ? 1e3 : 0;
        // Initialize particles
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

    // Parse visual_configs.txt
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

    // Parse tech_tree.txt
    file = fopen(TECH_TREE_FILE, "r");
    if (!file) {
        printf("Error: Cannot open %s\n", TECH_TREE_FILE);
        exit(1);
    }
    m->techs = malloc(MAX_TECHS * sizeof(Tech));
    m->tech_count = 0;
    while (fgets(line, sizeof(line), file) && m->tech_count < MAX_TECHS) {
        Tech* t = &m->techs[m->tech_count];
        sscanf(line, "%s %s %lf %d %d %s %lf", t->name, t->resource_name, &t->resource_amount, &t->stage, &t->min_universes, t->effect_resource, &t->effect_multiplier);
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
        case 5: printf("Stage 🕰️ 阶段: Humanoid Civilizations 人形文明\n"); break;
        case 6: printf("Stage 🕰️ 阶段: Space Travel 太空旅行\n"); break;
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

    // H2 + O2 -> H2O
    if (h2 && o2 && water && h2->quantity >= 2 && o2->quantity >= 1) {
        h2->quantity -= 2;
        o2->quantity -= 1;
        water->quantity += 2 * multiplier;
        printf("Reaction ⚗️: 2H2 + O2 -> 2H2O, Water increased by %.2f!\n", 2 * multiplier);
    }

    // CH4 + 2O2 -> CO2 + 2H2O
    if (ch4 && o2 && co2 && water && ch4->quantity >= 1 && o2->quantity >= 2) {
        ch4->quantity -= 1;
        o2->quantity -= 2;
        co2->quantity += 1 * multiplier;
        water->quantity += 2 * multiplier;
        printf("Reaction ⚗️: CH4 + 2O2 -> CO2 + 2H2O, CO2 +%.2f, Water +%.2f!\n", 1 * multiplier, 2 * multiplier);
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

void unlock_tech(Multiverse* m, char* tech_name) {
    for (int i = 0; i < m->tech_count; i++) {
        if (strcmp(m->techs[i].name, tech_name) == 0 && !m->techs[i].unlocked) {
            int can_unlock = 1;
            if (m->stage < m->techs[i].stage || m->universes < m->techs[i].min_universes) {
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
            if (can_unlock) {
                m->techs[i].unlocked = 1;
                printf("Tech Unlocked 🔧: %s\n", m->techs[i].name);
            } else {
                printf("Cannot unlock %s: Prerequisites not met\n", tech_name);
            }
            break;
        }
    }
}

void set_stage(Multiverse* m, int stage) {
    if (stage < 0 || stage > 6) {
        printf("Invalid stage: %d (must be 0-6)\n", stage);
        return;
    }
    m->stage = stage;
    // Set minimum resources for the stage
    switch (stage) {
        case 0: // Big Bang
            m->energy = 1e10;
            m->matter = 0;
            m->cells = 0;
            m->creatures = 0;
            m->humanoids = 0;
            m->technology = 0;
            break;
        case 1: // Matter Formation
            m->energy = 1e9;
            m->matter = 1e6;
            break;
        case 2: // Chemical Evolution
            m->matter = 1e5;
            for (int i = 0; i < m->element_count; i++) {
                if (strcmp(m->elements[i].name, "H2") == 0 || strcmp(m->elements[i].name, "O2") == 0) {
                    m->elements[i].quantity = 1e4;
                }
            }
            break;
        case 3: // Cellular Life
            for (int i = 0; i < m->element_count; i++) {
                if (strcmp(m->elements[i].name, "Water") == 0) m->elements[i].quantity = 1e3;
            }
            m->cells = 1e2;
            break;
        case 4: // Complex Creatures
            m->cells = 1e2;
            m->creatures = 10;
            break;
        case 5: // Humanoid Civilizations
            m->creatures = 10;
            m->humanoids = 1;
            m->technology = 0.5;
            break;
        case 6: // Space Travel
            m->humanoids = 1;
            m->technology = 1;
            break;
    }
    printf("Jumped to Stage %d\n", stage);
}

void handle_human_input(Multiverse* m) {
    char input[256];
    printf("Enter command (e.g., 's supernova 1 10', 't biotech', 'd stage 5', or 'q' to continue): ");
    if (fgets(input, sizeof(input), stdin)) {
        char command, arg1[32], arg2[32];
        int year, day, stage;
        if (sscanf(input, "%c %s %d %d", &command, arg1, &year, &day) == 4 && command == 's') {
            schedule_event(m, arg1, year, day);
        } else if (sscanf(input, "%c %s", &command, arg1) == 2 && command == 't') {
            unlock_tech(m, arg1);
        } else if (sscanf(input, "%c %s %d", &command, arg1, &stage) == 3 && command == 'd' && strcmp(arg1, "stage") == 0) {
            set_stage(m, stage);
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
    static int update_count = 0;
    advance_time(&m->time);
    check_random_events(m);
    check_scheduled_events(m);

    // Calculate tech effects
    double energy_delta = 0, matter_delta = 0, cells_delta = 0, creatures_delta = 0, humanoids_delta = 0, technology_delta = 0;

    // Big Bang: Convert energy to matter
    if (m->stage == 0) {
        if (m->energy >= 1e9) {
            m->energy -= 1e9;
            matter_delta += 1e6;
            if (m->matter + matter_delta >= 1e6) {
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
                } else if (strcmp(m->elements[i].name, "CH4") == 0) {
                    m->elements[i].quantity += 1e3; // Produce some CH4
                }
            }
            for (int i = 0; i < m->element_count; i++) {
                if (strcmp(m->elements[i].name, "H2") == 0 && m->elements[i].quantity >= 1e4) {
                    m->stage = 2;
                    printf("Elements forming in cooling universes... 🧪\n");
                    break;
                }
            }
        }
    }
    // Elements: Form water and cells
    else if (m->stage == 2) {
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
                printf("Simple cells emerging... 🦠\n");
            }
        }
    }
    // Cells: Form creatures
    else if (m->stage == 3) {
        if (m->cells >= 1e2) {
            m->cells -= 1e2;
            creatures_delta += 10;
            if (m->creatures + creatures_delta >= 10) {
                m->stage = 4;
                printf("Complex creatures evolving... 🐾\n");
            }
        }
    }
    // Creatures: Form humanoids
    else if (m->stage == 4) {
        if (m->creatures >= 10) {
            m->creatures -= 10;
            humanoids_delta += 1;
            if (m->humanoids + humanoids_delta >= 1) {
                m->stage = 5;
                printf("Humanoids developing civilizations... 👤\n");
            }
        }
    }
    // Humanoids: Develop technology
    else if (m->stage == 5) {
        if (m->humanoids > 0) {
            m->humanoids -= 0.1;
            technology_delta += 0.1;
            if (m->technology + technology_delta >= 1) {
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
            energy_delta += 1e10;
            m->stage = 0;
            printf("New universe created in the multiverse! 🌌\n");
        }
    }

    // Apply tech effects
    apply_tech_effects(m, &energy_delta, &matter_delta, &cells_delta, &creatures_delta, &humanoids_delta, &technology_delta);
    m->energy += energy_delta;
    m->matter += matter_delta;
    m->cells += cells_delta;
    m->creatures += creatures_delta;
    m->humanoids += humanoids_delta;
    m->technology += technology_delta;

    printf("Update #%d at %.3fs\n", ++update_count, (double)clock() / CLOCKS_PER_SEC);
}

static int update_count = 0;

void render_text(float x, float y, const char* text) {
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
}

void render_3d() {
    static int frame_count = 0;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(0, 0, 10, 0, 0, 0, 0, 1, 0);

    // Render universes as spheres
    for (int i = 0; i < global_m->visual_count; i++) {
        if (strcmp(global_m->visuals[i].name, "Universe") == 0 && strcmp(global_m->visuals[i].shape, "sphere") == 0) {
            glPushMatrix();
            glTranslatef(0, 0, 0);
            glColor3f(global_m->visuals[i].r, global_m->visuals[i].g, global_m->visuals[i].b);
            glutSolidSphere(global_m->visuals[i].size * global_m->universes, 20, 20);
            glPopMatrix();
        }
    }

    // Render elements as particles
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
                    for (int k = 0; k < particle_count; k++) {
                        Particle* p = &global_m->elements[i].particles[k];
                        p->x += p->vx;
                        p->y += p->vy;
                        p->z += p->vz;
                        // Bounce off boundaries of 2x2x2 cube
                        if (p->x < -1 || p->x > 1) p->vx = -p->vx;
                        if (p->y < -1 || p->y > 1) p->vy = -p->vy;
                        if (p->z < -1 || p->z > 1) p->vz = -p->vz;
                        glVertex3f(p->x, p->y, p->z);
                    }
                }
            }
        }
    }
    glEnd();

    // Render text overlays
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 600);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    char buffer[256];
    sprintf(buffer, "Universes: %d", global_m->universes);
    render_text(10, 580, buffer);
    printf("Render Universes: %d\n", global_m->universes); // Debug output
    sprintf(buffer, "Energy: %.2e", global_m->energy);
    render_text(10, 560, buffer);
    sprintf(buffer, "Matter: %.2e", global_m->matter);
    render_text(10, 540, buffer);
    for (int i = 0; i < global_m->element_count; i++) {
        if (global_m->elements[i].quantity > 0) {
            sprintf(buffer, "%s: %.2e", global_m->elements[i].name, global_m->elements[i].quantity);
            render_text(10, 520 - i * 20, buffer);
        }
    }
    sprintf(buffer, "Cells: %.2e", global_m->cells);
    render_text(10, 460, buffer);
    sprintf(buffer, "Creatures: %.2e", global_m->creatures);
    render_text(10, 440, buffer);
    sprintf(buffer, "Humanoids: %.2e", global_m->humanoids);
    render_text(10, 420, buffer);
    sprintf(buffer, "Technology: %.2e", global_m->technology);
    render_text(10, 400, buffer);
    sprintf(buffer, "Time: Year %d, Day %d, %02d:00", global_m->time.year, global_m->time.day, global_m->time.hour);
    render_text(10, 380, buffer);
    if (global_m->debug_mode) {
        sprintf(buffer, "Debug Mode: 0.1s/update");
        render_text(10, 360, buffer);
    }
    sprintf(buffer, "Frame #%d", ++frame_count);
    render_text(10, 340, buffer);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glutSwapBuffers();
}

void timer_func(int value) {
    glutPostRedisplay();
    glutTimerFunc(global_m->debug_mode ? 100 : 1000, timer_func, 0); // 100ms or 1000ms
}

void idle() {
    static clock_t last_update = 0;
    static double accumulated_time = 0.0;
    clock_t now = clock();
    double elapsed = (double)(now - last_update) / CLOCKS_PER_SEC;
    accumulated_time += elapsed;
    last_update = now;

    double interval = global_m->debug_mode ? 0.1 : 1.0; // 0.1s in debug, 1s normal
    int updates = 0;
    while (accumulated_time >= interval) {
        update_multiverse(global_m);
        accumulated_time -= interval;
        updates++;
    }
    if (updates > 0 && global_m->render_3d) {
        glutPostRedisplay();
    }
    if (!global_m->render_3d) {
        print_status(global_m);
        if (global_m->control_mode == 1 && !global_m->debug_mode) {
            handle_human_input(global_m);
        }
    }
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

    // Parse command-line arguments
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

    // Prompt for control mode
    printf("Select control mode (0: Computer 🤖, 1: Human 👤): ");
    int mode;
    scanf("%d", &mode);
    getchar();
    m->control_mode = (mode == 1) ? 1 : 0;

    initialize_multiverse(m);
    printf("Starting Multiverse Evolution... 🌌\n");

    if (m->render_3d) {
        glutInit(&argc, argv);
        glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
        glutInitWindowSize(800, 600);
        glutCreateWindow("Multiverse Evolution");
        glEnable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        gluPerspective(45, 800.0/600.0, 0.1, 100);
        glMatrixMode(GL_MODELVIEW);
        glutDisplayFunc(render_3d);
        glutIdleFunc(idle);
        glutTimerFunc(global_m->debug_mode ? 100 : 1000, timer_func, 0); // Start timer
        glutMainLoop();
    } else {
        clock_t last_update = clock();
        double accumulated_time = 0.0;
        double interval = m->debug_mode ? 0.1 : 1.0; // 0.1s in debug, 1s normal
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

    free(m->elements);
    free(m->visuals);
    free(m->techs);
    free(m);
    return 0;
}
