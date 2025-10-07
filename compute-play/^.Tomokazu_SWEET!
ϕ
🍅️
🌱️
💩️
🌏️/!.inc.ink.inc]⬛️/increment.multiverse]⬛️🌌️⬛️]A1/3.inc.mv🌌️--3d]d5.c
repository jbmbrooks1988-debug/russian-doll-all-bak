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
#define ELEMENTS_FILE "elements]a1.txt"
#define VISUAL_CONFIGS_FILE "visual_configs.txt"
#define TECH_TREE_FILE "tech_tree.txt"

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
        double multiplier = 1.0;
        for (int i = 0; i < m->tech_count; i++) {
            if (m->techs[i].unlocked && strcmp(m->techs[i].effect_resource, "elements") == 0) {
                multiplier *= m->techs[i].effect_multiplier;
            }
        }
        h2->quantity -= 2;
        o2->quantity -= 1;
        water->quantity += 2 * multiplier;
        printf("Reaction ⚗️ 反应: 2H2 + O2 -> 2H2O, Water increased by %.2f!\n", 2 * multiplier);
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

void handle_human_input(Multiverse* m) {
    char input[256];
    printf("Enter command (e.g., 's supernova 1 10' to schedule event, 't biotech' to unlock tech, or 'q' to continue): ");
    if (fgets(input, sizeof(input), stdin)) {
        char command, arg1[32], arg2[32];
        int year, day;
        if (sscanf(input, "%c %s %d %d", &command, arg1, &year, &day) == 4 && command == 's') {
            schedule_event(m, arg1, year, day);
        } else if (sscanf(input, "%c %s", &command, arg1) == 2 && command == 't') {
            unlock_tech(m, arg1);
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
        form_water(m);
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
}

void render_text(float x, float y, const char* text) {
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
}

void render_3d() {
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
                    for (int k = 0; k < particle_count && k < 100; k++) {
                        float x = (rand() % 1000 / 500.0) - 1;
                        float y = (rand() % 1000 / 500.0) - 1;
                        float z = (rand() % 1000 / 500.0) - 1;
                        glVertex3f(x, y, z);
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
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glutSwapBuffers();
}

void idle() {
    update_multiverse(global_m);
    if (global_m->render_3d) {
        glutPostRedisplay();
    } else {
        print_status(global_m);
        if (global_m->control_mode == 1) {
            handle_human_input(global_m);
        }
    }
    usleep(1000000); // 1 second
}

int main(int argc, char* argv[]) {
    Multiverse* m = malloc(sizeof(Multiverse));
    if (!m) {
        printf("Error: Memory allocation failed\n");
        return 1;
    }
    global_m = m;
    m->render_3d = (argc > 1 && strcmp(argv[1], "--3d") == 0) ? 1 : 0;

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
        glutMainLoop();
    } else {
        while (1) {
            print_status(m);
            if (m->control_mode == 1) {
                handle_human_input(m);
            }
            update_multiverse(m);
            sleep(1);
        }
    }

    free(m->elements);
    free(m->visuals);
    free(m->techs);
    free(m);
    return 0;
}
