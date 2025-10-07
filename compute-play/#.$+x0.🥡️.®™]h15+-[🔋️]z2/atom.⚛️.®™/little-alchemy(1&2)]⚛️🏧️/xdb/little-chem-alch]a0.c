#include <stdio.h>
#include <string.h>

// Maximum number of elements (expandable)
#define MAX_ELEMENTS 50
#define MAX_COMBINATIONS 50
#define MAX_INVENTORY 100

// Element data (arrays instead of structs)
char element_names[MAX_ELEMENTS][20];  // Names of elements
int element_protons[MAX_ELEMENTS];    // Proton count (-1 for subatomic with no counts)
int element_neutrons[MAX_ELEMENTS];   // Neutron count
int element_electrons[MAX_ELEMENTS];  // Electron count
int element_count = 0;                // Tracks number of defined elements

// Combination data (index1, index2, result_index)
int combinations[MAX_COMBINATIONS][3];
int combination_count = 0;

// Player inventory (indices of elements)
int inventory[MAX_INVENTORY];
int inventory_count = 0;

// Function to add an element to the element list
void add_element(const char *name, int protons, int neutrons, int electrons) {
    strcpy(element_names[element_count], name);
    element_protons[element_count] = protons;
    element_neutrons[element_count] = neutrons;
    element_electrons[element_count] = electrons;
    element_count++;
}

// Function to add a combination rule
void add_combination(int index1, int index2, int result_index) {
    combinations[combination_count][0] = index1;
    combinations[combination_count][1] = index2;
    combinations[combination_count][2] = result_index;
    combination_count++;
}

// Function to find an element by name (returns index or -1 if not found)
int find_element(const char *name) {
    for (int i = 0; i < element_count; i++) {
        if (strcmp(element_names[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

// Function to check if two elements match an atom based on particle counts
int check_atom(int p, int n, int e) {
    for (int i = 0; i < element_count; i++) {
        if (element_protons[i] == p && element_neutrons[i] == n && element_electrons[i] == e) {
            return i;
        }
    }
    return -1;
}

// Function to combine two elements
int combine_elements(int index1, int index2) {
    // Check pair-based combinations first (for subatomic or specific rules)
    for (int i = 0; i < combination_count; i++) {
        if ((combinations[i][0] == index1 && combinations[i][1] == index2) ||
            (combinations[i][0] == index2 && combinations[i][1] == index1)) {
            return combinations[i][2];
        }
    }

    // If both elements have particle counts, sum them and check for atoms
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

// Function to initialize the game with elements and combinations
void initialize_game() {
    // Add elements (name, protons, neutrons, electrons)
    // -1 indicates subatomic particles with no counts yet
    add_element("Up quark", -1, -1, -1);
    add_element("Down quark", -1, -1, -1);
    add_element("Electron", 0, 0, 1);
    add_element("Pair up quarks", -1, -1, -1);
    add_element("Pair down quarks", -1, -1, -1);
    add_element("Proton", 1, 0, 0);
    add_element("Neutron", 0, 1, 0);
    add_element("Hydrogen", 1, 0, 1);
    add_element("Helium nucleus", 2, 2, 0);
    add_element("Helium", 2, 2, 2);
    add_element("H2 molecule", 2, 0, 2);
    add_element("Oxygen", 8, 8, 8); // Simplified as a basic element for now
    add_element("Water", 10, 8, 10);

    // Add combination rules
    add_combination(0, 0, 3);  // Up quark + Up quark -> Pair up quarks
    add_combination(3, 1, 5);  // Pair up quarks + Down quark -> Proton
    add_combination(1, 1, 4);  // Down quark + Down quark -> Pair down quarks
    add_combination(0, 4, 6);  // Up quark + Pair down quarks -> Neutron
    add_combination(7, 7, 9);  // Hydrogen + Hydrogen -> H2 molecule
    add_combination(11, 10, 12); // H2 molecule + Oxygen -> Water

    // Starting inventory
    inventory[0] = 0; // Up quark
    inventory[1] = 1; // Down quark
    inventory[2] = 2; // Electron
    inventory_count = 3;
}

// Function to display inventory
void display_inventory() {
    printf("\nInventory (%d discovered):\n", inventory_count);
    for (int i = 0; i < inventory_count; i++) {
        printf("%d: %s (p:%d, n:%d, e:%d)\n", i, element_names[inventory[i]],
               element_protons[inventory[i]], element_neutrons[inventory[i]],
               element_electrons[inventory[i]]);
    }
    printf("\n");
}

// Main game loop
int main() {
    initialize_game();
    char input1[20], input2[20];
    int running = 1;

    printf("Welcome to Subatomic Alchemy!\n");
    printf("Combine elements to discover new ones. Type 'quit' to exit.\n");

    while (running) {
        display_inventory();

        printf("Enter first element name: ");
        scanf("%19s", input1);
        if (strcmp(input1, "quit") == 0) break;

        printf("Enter second element name: ");
        scanf("%19s", input2);
        if (strcmp(input2, "quit") == 0) break;

        int index1 = find_element(input1);
        int index2 = find_element(input2);

        if (index1 < 0 || index2 < 0) {
            printf("One or both elements not found!\n");
            continue;
        }

        int result = combine_elements(index1, index2);
        if (result >= 0) {
            printf("Success! You discovered: %s\n", element_names[result]);
            // Check if already in inventory
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
