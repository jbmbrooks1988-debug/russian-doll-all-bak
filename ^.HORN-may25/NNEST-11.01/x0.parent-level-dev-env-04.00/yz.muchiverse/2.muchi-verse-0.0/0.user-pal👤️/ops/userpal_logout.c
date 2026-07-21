/* userpal_logout - USER-PAL-STANDARD.txt sec. 2. Clears user-pal's own
 * current_login.txt (empty current_user_id) - matches
 * forum_menu_input.c's own LOGOUT handler shape (truncate, don't
 * delete, so the file always exists for a plain read).
 *
 * Self-contained, no shared headers.
 * Usage: userpal_logout.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

int main(void) {
    resolve_root();

    char login_path[PATH_BUF];
    snprintf(login_path, sizeof(login_path), "%s/current_login.txt", project_root);
    FILE *f = fopen(login_path, "w");
    if (!f) { fprintf(stderr, "Could not write current_login.txt.\n"); return 1; }
    fprintf(f, "current_user_id=\n");
    fclose(f);

    printf("Logged out.\n");
    return 0;
}
