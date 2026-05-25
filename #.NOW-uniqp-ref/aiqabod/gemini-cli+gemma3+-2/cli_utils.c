#include "cli_utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h> // For isatty

// Helper function for user confirmation
int ask_for_confirmation(const char* question, int auto_confirm) {
    if (auto_confirm) {
        return 1; // Auto-confirm if flag is set
    }

    if (!isatty(STDIN_FILENO)) {
        // If not interactive and not auto-confirm, default to 'no'
        fprintf(stderr, "Non-interactive mode: defaulting to 'no' for confirmation '%s'\n", question);
        return 0;
    }

    fprintf(stderr, "%s [y/N]: ", question);
    char response[10];
    if (fgets(response, sizeof(response), stdin) != NULL) {
        if (response[0] == 'y' || response[0] == 'Y') {
            return 1;
        }
    }
    return 0;
}
