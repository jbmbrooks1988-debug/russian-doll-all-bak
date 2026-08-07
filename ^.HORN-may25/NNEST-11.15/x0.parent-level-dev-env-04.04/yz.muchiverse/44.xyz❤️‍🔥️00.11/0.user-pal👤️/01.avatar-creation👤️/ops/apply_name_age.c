/* apply_name_age - write name/age from gui_state onto selected avatar.
 * Usage: apply_name_age.+x <avatar_uuid> <name> <age> */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512

static char project_root[MAX_PATH] = ".";
static char login_root[MAX_PATH] = ".";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}
static void resolve_login_root(void) {
    const char *env = getenv("USERPAL_LOGIN_ROOT");
    if (env && env[0]) { snprintf(login_root, sizeof(login_root), "%s", env); return; }
    char cand[PATH_BUF], real[MAX_PATH];
    snprintf(cand, sizeof(cand), "%s/../00.login-signup", project_root);
    if (realpath(cand, real)) { snprintf(login_root, sizeof(login_root), "%s", real); return; }
    snprintf(cand, sizeof(cand), "%s/../../../00.login-signup", project_root);
    if (realpath(cand, real)) { snprintf(login_root, sizeof(login_root), "%s", real); return; }
    snprintf(login_root, sizeof(login_root), "%s", project_root);
}
static void read_kv(const char *path, const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t kl = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
            char *v = line + kl + 1;
            v[strcspn(v, "\r\n")] = '\0';
            snprintf(out, out_sz, "%s", v);
            break;
        }
    }
    fclose(f);
}
static void write_kv(const char *path, const char *key, const char *value) {
    FILE *f = fopen(path, "r");
    char lines[64][MAX_LINE];
    int n = 0;
    if (f) { while (n < 64 && fgets(lines[n], MAX_LINE, f)) n++; fclose(f); }
    size_t kl = strlen(key);
    f = fopen(path, "w");
    if (!f) return;
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (!found && strncmp(lines[i], key, kl) == 0 && lines[i][kl] == '=') {
            fprintf(f, "%s=%s\n", key, value); found = 1;
        } else fputs(lines[i], f);
    }
    if (!found) fprintf(f, "%s=%s\n", key, value);
    fclose(f);
}
static void read_session_state(const char *key, char *out, size_t out_sz) {
    out[0] = '\0';
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/xyzfs/session.pdl", login_root);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    size_t kl = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        char *p1 = strchr(line, '|');
        if (!p1) continue;
        char *k = p1 + 1;
        while (*k == ' ' || *k == '\t') k++;
        if (strncmp(k, key, kl) != 0) continue;
        char *after = k + kl;
        while (*after == ' ' || *after == '\t') after++;
        if (*after != '|') continue;
        char *v = after + 1;
        while (*v == ' ' || *v == '\t') v++;
        v[strcspn(v, "\r\n")] = '\0';
        size_t n = strlen(v);
        while (n > 0 && (v[n - 1] == ' ' || v[n - 1] == '\t')) v[--n] = '\0';
        snprintf(out, out_sz, "%s", v);
        break;
    }
    fclose(f);
}
static void sync_pair(const char *uuid, const char *key, const char *val) {
    char local[PATH_BUF];
    snprintf(local, sizeof(local), "%s/pieces/world_01/map_lobby/%s/state.txt", project_root, uuid);
    write_kv(local, key, val);
    char xyzfs[512];
    read_session_state("xyzfs_path", xyzfs, sizeof(xyzfs));
    if (!xyzfs[0]) {
        char login_path[PATH_BUF];
        snprintf(login_path, sizeof(login_path), "%s/current_login.txt", login_root);
        read_kv(login_path, "current_xyzfs", xyzfs, sizeof(xyzfs));
    }
    if (xyzfs[0]) {
        char remote[PATH_BUF];
        snprintf(remote, sizeof(remote), "%s/%s/home/avatars/%s/state.txt", login_root, xyzfs, uuid);
        write_kv(remote, key, val);
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: apply_name_age.+x <uuid> <name> <age>\n");
        return 1;
    }
    resolve_root();
    resolve_login_root();
    const char *uuid = argv[1];
    const char *name = argv[2];
    const char *age = argv[3];
    if (!name[0]) { printf("Enter an avatar name first.\n"); return 1; }
    int a = atoi(age);
    if (a < 1 || a > 200) a = 18;
    char abuf[16];
    snprintf(abuf, sizeof(abuf), "%d", a);
    sync_pair(uuid, "name", name);
    sync_pair(uuid, "age", abuf);
    printf("Saved name=%s age=%s\n", name, abuf);
    return 0;
}
