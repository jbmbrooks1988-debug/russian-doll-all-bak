#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int counter = 0;

int main() {
    // Print initial state - counter value
    printf("VAR;page_counter;int;%d\n", counter);
    printf("VAR;page_status;string;Module loaded\n");
    fflush(stdout);
    
    char input[256];
    while (fgets(input, sizeof(input), stdin)) {
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "INCREMENT") == 0) {
            counter++;
            printf("VAR;page_counter;int;%d\n", counter);
            printf("VAR;page_status;string;Counter incremented to %d\n", counter);
        } else if (strcmp(input, "RESET") == 0) {
            counter = 0;
            printf("VAR;page_counter;int;%d\n", counter);
            printf("VAR;page_status;string;Counter reset\n");
        } else if (strncmp(input, "SLIDER:", 7) == 0) {
            // Handle slider input
            printf("VAR;page_status;string;Slider updated: %s\n", input);
        } else {
            // Process any other commands
            printf("VAR;page_status;string;Received: %s\n", input);
        }
        fflush(stdout);
    }
    
    return 0;
}