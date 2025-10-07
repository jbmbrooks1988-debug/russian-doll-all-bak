#include <stdio.h>
#include <string.h>

// Maximum limits
#define MAX_ELEMENTS 200
#define MAX_INVENTORY 400

// Element data
char element_names[MAX_ELEMENTS][20];
int element_protons[MAX_ELEMENTS];
int element_neutrons[MAX_ELEMENTS];
int element_electrons[MAX_ELEMENTS];
int element_combo1[MAX_ELEMENTS];  // First element index in combination
int element_combo2[MAX_ELEMENTS];  // Second element index in combination
int element_count = 0;

// Player inventory
int inventory[MAX_INVENTORY];
int inventory_count = 0;

// Load elements and combinations from file
void load_elements(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open %s\n", filename);
        return;
    }

    char line[100];
    while (fgets(line, sizeof(line), file) && element_count < MAX_ELEMENTS) {
        // Format: name protons neutrons electrons combo1 combo2
        int protons, neutrons, electrons, combo1, combo2;
        if (sscanf(line, "%19s %d %d %d %d %d", element_names[element_count],
                   &protons, &neutrons, &electrons, &combo1, &combo2) == 6) {
            element_protons[element_count] = protons;
            element_neutrons[element_count] = neutrons;
            element_electrons[element_count] = electrons;
            element_combo1[element_count] = combo1;
            element_combo2[element_count] = combo2;

            // Add starting elements to inventory
            if (combo1 == -1 && combo2 == -1 && inventory_count < MAX_INVENTORY) {
                inventory[inventory_count++] = element_count;
            }
            element_count++;
        }
    }

    fclose(file);
    printf("Loaded %d elements from %s\n", element_count, filename);
}

// Check if particle counts match an atom
int check_atom(int p, int n, int e) {
    for (int i = 0; i < element_count; i++) {
        if (element_protons[i] == p && element_neutrons[i] == n && element_electrons[i] == e) {
            return i;
        }
    }
    return -1;
}

// Combine two elements by index
int combine_elements(int index1, int index2) {
    // Check if this pair matches any element's combination
    for (int i = 0; i < element_count; i++) {
        if ((element_combo1[i] == index1 && element_combo2[i] == index2) ||
            (element_combo1[i] == index2 && element_combo2[i] == index1)) {
            return i;
        }
    }

    // Sum particle counts if both have them
    if (element_protons[index1] >= 0 && element_protons[index2] >= 0) {
        int total_p = element_protons[index1] + element_protons[index2];
        int total_n = element_neutrons[index1] + element_neutrons[index2];
        int total_e = element_electrons[index1] + element_electrons[index2];
        int atom_index = check_atom(total_p, total_n, total_e);
        if (atom_index >= 0) {
            return atom_index;
        }
    }

    return -1; // No valid combination
}

// Display inventory
void display_inventory() {
    printf("\nInventory (%d discovered):\n", inventory_count);
    for (int i = 0; i < inventory_count; i++) {
        int idx = inventory[i];
        printf("%d: %s (p:%d, n:%d, e:%d)\n", i, element_names[idx],
               element_protons[idx], element_neutrons[idx], element_electrons[idx]);
    }
    printf("\n");
}

int main() {
    load_elements("elements.txt");
    if (element_count == 0) {
        printf("No elements loaded. Exiting.\n");
        return 1;
    }

    int index1, index2;
    int running = 1;

    printf("Welcome to Subatomic Alchemy!\n");
    printf("Combine elements by entering their inventory indices. Enter -1 to quit.\n");

    while (running) {
        display_inventory();

        printf("Enter first index: ");
        scanf("%d", &index1);
        if (index1 == -1) break;

        printf("Enter second index: ");
        scanf("%d", &index2);
        if (index2 == -1) break;

        if (index1 < 0 || index1 >= inventory_count || index2 < 0 || index2 >= inventory_count) {
            printf("Invalid index!\n");
            continue;
        }

        int result = combine_elements(inventory[index1], inventory[index2]);
        if (result >= 0) {
            printf("Success! You discovered: %s\n", element_names[result]);
            int exists = 0;
            for (int i = 0; i < inventory_count; i++) {
                if (inventory[i] == result) {
                    exists = 1;
                    break;
                }
            }
            if (!exists && inventory_count < MAX_INVENTORY) {
                inventory[inventory_count++] = result;
            }
        } else {
            printf("No combination found.\n");
        }
    }

    printf("Thanks for playing!\n");
    return 0;
}
