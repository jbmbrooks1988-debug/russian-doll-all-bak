/* userpal_login - USER-PAL-STANDARD.txt sec. 2. Sets user-pal's own
 * current_login.txt (current_user_id + logged_in_at) to an EXISTING
 * user (refuses if the user directory doesn't exist - same rule as
 * forum_switch_user.c). No password (see userpal_create_account.c).
 *
 * IMPORTANT: unlike pal-forum's own net/session.txt (per-session,
 * lives inside pieces/sessions/<id>/), current_login.txt is written at
 * user-pal's REAL project root - it is deliberately SHARED, PERSISTENT
 * data that outlives any one session (sec. 2's own explanation of why
 * these are different shapes). Never write this file inside a session
 * directory.
 *
 * Self-contained, no shared headers.
 * Usage: userpal_login.+x <user_id> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: userpal_login.+x <user_id>\n");
        return 1;
    }
    resolve_root();
    const char *user_id = argv[1];

    char user_dir[PATH_BUF];
    snprintf(user_dir, sizeof(user_dir), "%s/users/%s", project_root, user_id);
    struct stat st;
    if (stat(user_dir, &st) != 0) {
        fprintf(stderr, "No such user '%s'.\n", user_id);
        return 1;
    }

    char login_path[PATH_BUF];
    snprintf(login_path, sizeof(login_path), "%s/current_login.txt", project_root);
    FILE *f = fopen(login_path, "w");
    if (!f) { fprintf(stderr, "Could not write current_login.txt.\n"); return 1; }
    fprintf(f, "current_user_id=%s\n", user_id);
    fprintf(f, "logged_in_at=%ld\n", (long)time(NULL));
    fclose(f);

    printf("Logged in as '%s'.\n", user_id);
    return 0;
}
