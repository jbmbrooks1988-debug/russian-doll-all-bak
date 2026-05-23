#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// OP-ED Game Player Bootstrap
// Responsibility: Minimal C wrapper to launch the PAL-driven player loop.

int main(int argc, char* argv[]) {
    char *boot_script = "projects/op-ed/player/PAL/player_loop.asm";
    if (argc > 1) boot_script = argv[1];

    // Using the system's prisc+x
    char *cmd = NULL;
    asprintf(&cmd, "./pieces/system/prisc/prisc+x '%s'", boot_script);

    if (cmd) {
        printf("OP-ED PLAYER: Booting PAL Orchestrator...\n");
        system(cmd);
        free(cmd);
    }

    return 0;
}
