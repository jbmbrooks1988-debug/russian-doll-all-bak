#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h> // Added for tolower()

#define MAX_ATOMS 100
#define MAX_BONDS 100
#define MAX_AA 21 // Increased to handle all 20 plus a potential empty line issue

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
    char three_letter[4];
    char one_letter[2];
    char property[20];
    float hydrophobicity;
    float molecular_weight;
    float pKa_COOH;
    float pKa_NH3;
    float pKa_side;
    float pI;
    struct Atom atoms[MAX_ATOMS];
    struct Bond bonds[MAX_BONDS];
    int num_atoms;
    int num_bonds;
};

struct Molecule molecules[MAX_AA];
int num_mols = 0;
int current_mol = 0;

float rot_x = 0, rot_y = 0;
float zoom = -5.0; // Zoom level
int last_mouse_x, last_mouse_y;

// Find a molecule by name, or create a new one
int find_or_create_mol(const char *name, const char *three_letter, const char *one_letter, 
                      const char *property, float hydrophobicity, float mw, float pKa_COOH, 
                      float pKa_NH3, float pKa_side, float pI) {
    for (int i = 0; i < num_mols; i++) {
        if (strcmp(molecules[i].name, name) == 0) {
            return i;
        }
    }
    if (num_mols < MAX_AA) {
        strcpy(molecules[num_mols].name, name);
        strcpy(molecules[num_mols].three_letter, three_letter);
        strcpy(molecules[num_mols].one_letter, one_letter);
        strcpy(molecules[num_mols].property, property);
        molecules[num_mols].hydrophobicity = hydrophobicity;
        molecules[num_mols].molecular_weight = mw;
        molecules[num_mols].pKa_COOH = pKa_COOH;
        molecules[num_mols].pKa_NH3 = pKa_NH3;
        molecules[num_mols].pKa_side = pKa_side;
        molecules[num_mols].pI = pI;
        molecules[num_mols].num_atoms = 0;
        molecules[num_mols].num_bonds = 0;
        return num_mols++;
    }
    return -1; // No space for new molecules
}

void load_csvs(const char *atoms_file, const char *bonds_file) {
    FILE *fp_atoms = fopen(atoms_file, "r");
    if (!fp_atoms) {
        printf("Error: Could not open atoms file '%s'!\n", atoms_file);
        exit(1);
    }

    char line[512];
    // Skip header
    if (!fgets(line, sizeof(line), fp_atoms)) {
        printf("Error: Empty or invalid atoms CSV header!\n");
        exit(1);
    }

    // Load atoms
    while (fgets(line, sizeof(line), fp_atoms)) {
        char name[20], three_letter[4], one_letter[2], property[20], pKa_side_str[20], sym[3];
        int idx;
        float x, y, z, hydrophobicity, mw, pKa_COOH, pKa_NH3, pI;
        int ret = sscanf(line, "%[^,],%[^,],%[^,],%[^,],%f,%f,%f,%f,%[^,],%f,%d,%[^,],%f,%f,%f", 
                        name, three_letter, one_letter, property, &hydrophobicity, &mw, 
                        &pKa_COOH, &pKa_NH3, pKa_side_str, &pI, &idx, sym, &x, &y, &z);
        if (ret < 10) {
            if (strlen(line) > 5) printf("Warning: Skipping invalid atom line: %s", line);
            continue;
        }

        // Handle missing pKa_side values (represented as "-")
        float pKa_side = 0.0;
        if (strcmp(pKa_side_str, "-") != 0) {
            pKa_side = atof(pKa_side_str);
        }

        int mol_idx = find_or_create_mol(name, three_letter, one_letter, property, 
                                        hydrophobicity, mw, pKa_COOH, pKa_NH3, pKa_side, pI);
        if (mol_idx == -1) {
            printf("Error: Too many molecules (max %d)!\n", MAX_AA);
            continue; // Skip if we can't add more molecules
        }

        struct Molecule *mol = &molecules[mol_idx];
        if (mol->num_atoms < MAX_ATOMS) {
            struct Atom *a = &mol->atoms[mol->num_atoms++];
            a->index = idx;
            strcpy(a->symbol, sym);
            a->x = x; a->y = y; a->z = z;
        }
    }
    fclose(fp_atoms);

    FILE *fp_bonds = fopen(bonds_file, "r");
    if (!fp_bonds) {
        printf("Error: Could not open bonds file '%s'!\n", bonds_file);
        exit(1);
    }
    // Skip header
    if (!fgets(line, sizeof(line), fp_bonds)) {
        printf("Error: Empty or invalid bonds CSV header!\n");
        exit(1);
    }

    // Load bonds
    while (fgets(line, sizeof(line), fp_bonds)) {
        char name[20];
        int a1, a2;
        float order;
        int ret = sscanf(line, "%[^,],%d,%d,%f", name, &a1, &a2, &order);
        if (ret < 4) {
            if (strlen(line) > 5) printf("Warning: Skipping invalid bond line: %s", line);
            continue;
        }

        // Find existing molecule (should already exist from atoms loading)
        int mol_idx = -1;
        for (int i = 0; i < num_mols; i++) {
            if (strcmp(molecules[i].name, name) == 0) {
                mol_idx = i;
                break;
            }
        }
        
        if (mol_idx == -1) {
            printf("Warning: Bond for unknown molecule '%s' skipped!\n", name);
            continue;
        }
        
        struct Molecule *mol = &molecules[mol_idx];
        if (mol->num_bonds < MAX_BONDS) {
            struct Bond *b = &mol->bonds[mol->num_bonds++];
            b->a1 = a1; b->a2 = a2; b->order = order;
        }
    }
    fclose(fp_bonds);

    printf("Loaded %d molecules successfully.\n", num_mols);
    printf("Atom Color Legend:\n");
    printf("  C: Gray (0.5, 0.5, 0.5)\n");
    printf("  O: Red (1.0, 0.0, 0.0)\n");
    printf("  N: Blue (0.0, 0.0, 1.0)\n");
    printf("  S: Yellow (1.0, 1.0, 0.0)\n");
    printf("  H: White (1.0, 1.0, 1.0)\n");
    printf("  Other: Light Gray (0.8, 0.8, 0.8)\n");
    printf("To customize colors, edit get_atom_color() function.\n");
    printf("Controls: Press first letter of amino acid name to switch (e.g. 'g' for Glycine)\n");
    printf("          Left mouse drag to rotate\n");
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
    glTranslatef(0, 0, zoom); // Use zoom variable instead of fixed -5
    glRotatef(rot_x, 1, 0, 0);
    glRotatef(rot_y, 0, 1, 0);

    if (num_mols == 0) {
        glutSwapBuffers();
        return;
    }
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

    // Draw label
    glColor3f(1.0, 1.0, 1.0); // White color for the text
    glRasterPos3f(-1.5, 1.5, 0); // Position the text (z=0 for screen space)
    
    // Display detailed information about the current molecule
    char info_line[200];
    sprintf(info_line, "%s (%s) - %s", mol->name, mol->three_letter, mol->property);
    for (int i = 0; info_line[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, info_line[i]);
    }
    
    // Display molecular properties
    glRasterPos3f(-1.5, 1.3, 0);
    sprintf(info_line, "MW: %.2f Da, pI: %.2f", mol->molecular_weight, mol->pI);
    for (int i = 0; info_line[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, info_line[i]);
    }
    
    // Display available molecules at the bottom
    glColor3f(0.8, 0.8, 0.8); // Light gray for the list
    glRasterPos3f(-1.5, -1.5, 0); // Position at bottom
    char mol_list[500] = "Available: ";
    int pos = strlen(mol_list);
    
    for (int i = 0; i < num_mols && pos < 450; i++) {
        if (i > 0) {
            mol_list[pos++] = ',';
            mol_list[pos++] = ' ';
        }
        strcpy(mol_list + pos, molecules[i].name);
        pos += strlen(molecules[i].name);
        if (pos >= 450) break;
    }
    mol_list[pos] = '\0';
    
    // If the list is too long, truncate it
    if (pos >= 450) {
        mol_list[447] = '.';
        mol_list[448] = '.';
        mol_list[449] = '.';
        mol_list[450] = '\0';
    }
    
    for (int i = 0; mol_list[i] != '\0' && i < 500; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, mol_list[i]);
    }
    
    // Display instruction
    glColor3f(1.0, 1.0, 1.0); // White color for the text
    glRasterPos3f(-1.5, -1.7, 0); // Position below the list
    char *instruction = "Press first letter to switch (e.g. 'g' for Glycine)";
    for (int i = 0; instruction[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, instruction[i]);
    }

    glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {
    int key_lower = tolower(key);
    for (int i = 0; i < num_mols; i++) {
        if (key_lower == tolower(molecules[i].name[0])) {
            current_mol = i;
            printf("Showing %s\n", molecules[i].name);
            glutPostRedisplay();
            return;
        }
    }
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        last_mouse_x = x;
        last_mouse_y = y;
    } else if (button == 3) { // Scroll up
        zoom += 0.5;
        // Limit zoom range
        if (zoom > -1.0) zoom = -1.0;
        glutPostRedisplay();
    } else if (button == 4) { // Scroll down
        zoom -= 0.5;
        // Limit zoom range
        if (zoom < -20.0) zoom = -20.0;
        glutPostRedisplay();
    }
}

// Handle mouse wheel scrolling for zoom
void mouseWheel(int wheel, int direction, int x, int y) {
    if (direction > 0) {
        // Zoom in
        zoom += 0.5;
    } else {
        // Zoom out
        zoom -= 0.5;
    }
    
    // Limit zoom range
    if (zoom > -1.0) zoom = -1.0;
    if (zoom < -20.0) zoom = -20.0;
    
    glutPostRedisplay();
}

void motion(int x, int y) {
    rot_y += (x - last_mouse_x);
    rot_x += (y - last_mouse_y);
    last_mouse_x = x;
    last_mouse_y = y;
    glutPostRedisplay();
}

void init() {
    glClearColor(0.1, 0.1, 0.1, 1.0);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45, 1, 1, 100);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <atoms_csv> <bonds_csv>\
", argv[0]);
        return 1;
    }
    load_csvs(argv[1], argv[2]);
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
