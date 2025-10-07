
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define MAX_ATOMS 100
#define MAX_BONDS 100
#define MAX_AA 20

struct Atom {
    int index;
    char symbol[3];
    float x, y, z;
};

struct Bond {
    int a1, a2;
    float order;
};

struct Molecule {
    char name[20];
    struct Atom atoms[MAX_ATOMS];
    struct Bond bonds[MAX_BONDS];
    int num_atoms;
    int num_bonds;
};

struct Molecule molecules[MAX_AA];
int num_mols = 0;
int current_mol = 0;

float rot_x = 0, rot_y = 0;
int last_mouse_x, last_mouse_y;

void load_csvs() {
    FILE *fp_atoms = fopen("amino_acids_atoms.csv", "r");
    FILE *fp_bonds = fopen("amino_acids_bonds.csv", "r");
    if (!fp_atoms || !fp_bonds) {
        printf("CSV files not found!\n");
        exit(1);
    }

    char line[256];
    // Skip headers
    fgets(line, sizeof(line), fp_atoms);
    fgets(line, sizeof(line), fp_bonds);

    char prev_name[20] = "";
    int mol_idx = -1;

    // Load atoms
    while (fgets(line, sizeof(line), fp_atoms)) {
        char name[20], sym[3];
        int idx;
        float x, y, z;
        // Parse only relevant columns (skip others)
        sscanf(line, "%[^,],%*[^,],%*[^,],%*[^,],%*f,%*f,%*f,%*f,%*[^,],%*f,%d,%[^,],%f,%f,%f", name, &idx, sym, &x, &y, &z);

        if (strcmp(name, prev_name) != 0) {
            mol_idx++;
            strcpy(molecules[mol_idx].name, name);
            molecules[mol_idx].num_atoms = 0;
            molecules[mol_idx].num_bonds = 0;
            strcpy(prev_name, name);
            num_mols = mol_idx + 1;
        }

        struct Atom *a = &molecules[mol_idx].atoms[molecules[mol_idx].num_atoms++];
        a->index = idx;
        strcpy(a->symbol, sym);
        a->x = x; a->y = y; a->z = z;
    }

    // Load bonds
    mol_idx = -1;
    strcpy(prev_name, "");
    while (fgets(line, sizeof(line), fp_bonds)) {
        char name[20];
        int a1, a2;
        float order;
        sscanf(line, "%[^,],%d,%d,%f", name, &a1, &a2, &order);

        if (strcmp(name, prev_name) != 0) {
            mol_idx++;
            strcpy(prev_name, name);
        }

        struct Bond *b = &molecules[mol_idx].bonds[molecules[mol_idx].num_bonds++];
        b->a1 = a1; b->a2 = a2; b->order = order;
    }

    fclose(fp_atoms);
    fclose(fp_bonds);
}

void get_atom_color(char *sym, float *r, float *g, float *b) {
    if (strcmp(sym, "C") == 0) { *r=0.5; *g=0.5; *b=0.5; }
    else if (strcmp(sym, "O") == 0) { *r=1.0; *g=0.0; *b=0.0; }
    else if (strcmp(sym, "N") == 0) { *r=0.0; *g=0.0; *b=1.0; }
    else if (strcmp(sym, "S") == 0) { *r=1.0; *g=1.0; *b=0.0; }
    else if (strcmp(sym, "H") == 0) { *r=1.0; *g=1.0; *b=1.0; }
    else { *r=0.8; *g=0.8; *b=0.8; }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0, 0, -5);
    glRotatef(rot_x, 1, 0, 0);
    glRotatef(rot_y, 0, 1, 0);

    struct Molecule *mol = &molecules[current_mol];

    // Draw bonds
    glColor3f(0.2, 0.2, 0.2);
    glBegin(GL_LINES);
    for (int i = 0; i < mol->num_bonds; i++) {
        struct Bond *b = &mol->bonds[i];
        struct Atom *a1 = NULL, *a2 = NULL;
        for (int j = 0; j < mol->num_atoms; j++) {
            if (mol->atoms[j].index == b->a1) a1 = &mol->atoms[j];
            if (mol->atoms[j].index == b->a2) a2 = &mol->atoms[j];
        }
        if (a1 && a2) {
            glVertex3f(a1->x, a1->y, a1->z);
            glVertex3f(a2->x, a2->y, a2->z);
        }
    }
    glEnd();

    // Draw atoms
    glPointSize(10.0);
    glBegin(GL_POINTS);
    for (int i = 0; i < mol->num_atoms; i++) {
        struct Atom *a = &mol->atoms[i];
        float r, g, b;
        get_atom_color(a->symbol, &r, &g, &b);
        glColor3f(r, g, b);
        glVertex3f(a->x, a->y, a->z);
    }
    glEnd();

    glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {
    // Switch molecules by first letter (e.g., 'g' for Glycine)
    for (int i = 0; i < num_mols; i++) {
        if (tolower(key) == tolower(molecules[i].name[0])) {
            current_mol = i;
            printf("Showing %s\n", molecules[i].name);
            break;
        }
    }
    glutPostRedisplay();
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        last_mouse_x = x;
        last_mouse_y = y;
    }
}

void motion(int x, int y) {
    rot_y += (x - last_mouse_x);
    rot_x += (y - last_mouse_y);
    last_mouse_x = x;
    last_mouse_y = y;
    glutPostRedisplay();
}

void init() {
    glClearColor(0, 0, 0, 0);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45, 1, 1, 100);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv) {
    load_csvs();
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 800);
    glutCreateWindow("Amino Acid 3D Visualizer");
    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutMainLoop();
    return 0;
}
