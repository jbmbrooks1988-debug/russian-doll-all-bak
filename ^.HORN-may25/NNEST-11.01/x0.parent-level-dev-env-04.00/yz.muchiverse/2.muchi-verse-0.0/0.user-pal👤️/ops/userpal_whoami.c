/* userpal_whoami - USER-PAL-STANDARD.txt sec. 2. Prints the current
 * current_user_id from user-pal's own current_login.txt, or "none" if
 * logged out/never logged in. Exists mainly for humans/debug scripts -
 * the real cross-app auto-fill read path (sec. 4) reads
 * current_login.txt directly, since a sibling project's own pal
 * runtime cannot invoke an op living in a different project's ops/
 * tree.
 *
 * Self-contained, no shared headers.
 * Usage: userpal_whoami.+x (no args) */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

int main(void) {
    resolve_root();

    char login_path[PATH_BUF];
    snprintf(login_path, sizeof(login_path), "%s/current_login.txt", project_root);
    FILE *f = fopen(login_path, "r");
    if (!f) { printf("none\n"); return 0; }

    char line[MAX_LINE];
    const char *key = "current_user_id";
    size_t key_len = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *v = line + key_len + 1;
            v[strcspn(v, "\r\n")] = '\0';
            fclose(f);
            printf("%s\n", v[0] ? v : "none");
            return 0;
        }
    }
    fclose(f);
    printf("none\n");
    return 0;
}
