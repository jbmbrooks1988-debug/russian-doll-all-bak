/* scene_reader - one verb, one binary, no shared headers.
 * Lists scenes/*.txt, reads first one, outputs to message_log.txt.
 * R2 minimal: no interactive paging yet. Self-contained. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_LINE 512
#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)

static char project_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

int main(void) {
    resolve_root();

    char scenes_dir[PATH_BUF];
    snprintf(scenes_dir, sizeof(scenes_dir), "%s/scenes", project_root);
    DIR *d = opendir(scenes_dir);
    if (!d) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "No scenes available.\n"); fclose(lf); }
        return 0;
    }

    char first_scene[256] = "";
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (strlen(entry->d_name) < 5 || strcmp(entry->d_name + strlen(entry->d_name) - 4, ".txt") != 0) continue;
        snprintf(first_scene, sizeof(first_scene), "%s", entry->d_name);
        break;
    }
    closedir(d);

    if (!first_scene[0]) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "No scenes available.\n"); fclose(lf); }
        return 0;
    }

    char scene_path[PATH_BUF];
    snprintf(scene_path, sizeof(scene_path), "%s/scenes/%s", project_root, first_scene);
    FILE *sf = fopen(scene_path, "r");
    if (!sf) {
        char log_path[PATH_BUF];
        snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
        FILE *lf = fopen(log_path, "a");
        if (lf) { fprintf(lf, "Could not read scene.\n"); fclose(lf); }
        return 0;
    }

    char log_path[PATH_BUF];
    snprintf(log_path, sizeof(log_path), "%s/pieces/display/message_log.txt", project_root);
    FILE *lf = fopen(log_path, "a");
    if (lf) {
        fprintf(lf, "[Scene: %s]\n", first_scene);
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), sf)) {
            line[strcspn(line, "\n")] = '\0';
            fprintf(lf, "%s\n", line);
        }
        fclose(lf);
    }
    fclose(sf);
    return 0;
}
