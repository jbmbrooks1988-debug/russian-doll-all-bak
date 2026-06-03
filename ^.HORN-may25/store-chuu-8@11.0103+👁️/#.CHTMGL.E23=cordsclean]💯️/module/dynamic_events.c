#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int counter = 0;

int main() {
    // Print initial state
    printf("VAR;page_counter;int;%d\n", counter);
    printf("VAR;page_status;string;Dynamic Events Module Loaded\n");
    fflush(stdout);
    
    char input[256];
    while (fgets(input, sizeof(input), stdin)) {
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "INCREMENT") == 0) {
            // Handle legacy INCREMENT command
            counter++;
            printf("VAR;page_counter;int;%d\n", counter);
            printf("VAR;page_status;string;Legacy increment: %d\n", counter);
        } else if (strcmp(input, "RESET") == 0) {
            // Handle legacy RESET command
            counter = 0;
            printf("VAR;page_counter;int;%d\n", counter);
            printf("VAR;page_status;string;Legacy reset\n");
        } else if (strncmp(input, "EVENT:", 6) == 0) {
            // Handle dynamic events from our new system
            char* handler_name = input + 6; // Skip "EVENT:" prefix
            
            // Process different types of events
            if (strstr(handler_name, "increment") || strstr(handler_name, "Increment")) {
                counter++;
                printf("VAR;page_counter;int;%d\n", counter);
                printf("VAR;page_status;string;Dynamic increment via %s: %d\n", handler_name, counter);
            } else if (strstr(handler_name, "reset") || strstr(handler_name, "Reset")) {
                counter = 0;
                printf("VAR;page_counter;int;%d\n", counter);
                printf("VAR;page_status;string;Dynamic reset via %s\n", handler_name);
            } else if (strstr(handler_name, "click") || strstr(handler_name, "Click")) {
                printf("VAR;page_status;string;Click event %s processed\n", handler_name);
            } else {
                // Generic handler for any custom event
                printf("VAR;page_status;string;Event %s handled dynamically\n", handler_name);
            }
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