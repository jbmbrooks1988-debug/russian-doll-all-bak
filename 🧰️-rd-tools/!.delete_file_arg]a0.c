#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // Check for correct number of arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    // Get filename
    char *filename = argv[1];

    // Attempt to delete the file
    if (remove(filename) == 0) {
        printf("Deleted %s successfully\n", filename);
    } else if (errno != ENOENT) {
        // Print error only if file exists but deletion fails
        fprintf(stderr, "Error deleting %s: %s\n", filename, strerror(errno));
        return 1;
    }

    return 0;
}
