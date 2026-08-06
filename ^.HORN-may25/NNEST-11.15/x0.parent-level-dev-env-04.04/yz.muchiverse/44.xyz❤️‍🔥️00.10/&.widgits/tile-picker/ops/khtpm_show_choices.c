/* khtpm_show_choices - real, generic "Show Choices" event command,
 * 2026-08-05. Real RPG Maker command (confirmed,
 * #.ref/menu/event.commands.1.txt), first real proof-of-concept target:
 * MUCHI_RANCHER's own Change Gold. This is the SECOND, harder half -
 * Show Choices, real branching - built for a real book-stack reading
 * app, reusable by ANY entity's own event.pal.
 *
 * Blocking: writes a real SHOW_PAGE relay command into the entity's own
 * already-running tp_desktop_window.c process (via its interact_relay.txt),
 * then polls a real result file until the user picks (or times out).
 * Prints the picked action to stdout so a wrapper script/PAL exec chain
 * can branch on it.
 *
 * Usage: khtpm_show_choices.+x <entity_package_dir> <choices_objects_file>
 *   choices_objects_file: a real, flat "OBJECT | label=.. | action=.."
 *   list (no PAGE header needed - always exactly one page). The caller
 *   is responsible for generating this file (real content, not
 *   hardcoded here) before calling this op.
 *
 * Real result file: a fresh temp file under /tmp, cleaned up after.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PATH_BUF 4352
#define POLL_TIMEOUT_SEC 120

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: khtpm_show_choices.+x <entity_package_dir> <choices_objects_file>\n");
        return 1;
    }
    const char *package_dir = argv[1];
    const char *choices_file = argv[2];

    char result_path[PATH_BUF];
    snprintf(result_path, sizeof(result_path), "/tmp/khtpm_choice_result_%d.txt", (int)getpid());
    unlink(result_path);

    char relay_path[PATH_BUF];
    snprintf(relay_path, sizeof(relay_path), "%s/interact_relay.txt", package_dir);
    FILE *rf = fopen(relay_path, "w");
    if (!rf) {
        fprintf(stderr, "khtpm_show_choices: cannot open %s\n", relay_path);
        return 1;
    }
    fprintf(rf, "SHOW_PAGE:%s|%s\n", choices_file, result_path);
    fclose(rf);

    int waited = 0;
    char picked[256] = "";
    while (waited < POLL_TIMEOUT_SEC * 10) {
        FILE *f = fopen(result_path, "r");
        if (f) {
            if (fgets(picked, sizeof(picked), f)) {
                picked[strcspn(picked, "\r\n")] = '\0';
            }
            fclose(f);
            if (picked[0]) break;
        }
        usleep(100000);
        waited++;
    }
    unlink(result_path);

    if (!picked[0]) {
        fprintf(stderr, "khtpm_show_choices: timed out waiting for a pick\n");
        return 2;
    }
    printf("%s\n", picked);
    return 0;
}
