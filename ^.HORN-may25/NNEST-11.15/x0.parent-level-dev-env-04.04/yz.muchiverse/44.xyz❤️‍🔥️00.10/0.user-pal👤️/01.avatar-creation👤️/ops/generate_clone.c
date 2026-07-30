/* generate_clone - mint a new avatar "clone" with default DNA.
 * Writes:
 *   pieces/world_01/map_lobby/<avatar_uuid>/  (desktop / local piece)
 *   <xyzfs>/home/avatars/<avatar_uuid>/       (player fs source of truth)
 *   inventory lines in both places
 *
 * Usage: generate_clone.+x [label_name]
 * Prints avatar_uuid on stdout (first line) for buy_clone capture. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

#define MAX_PATH 4096
#define PATH_BUF (MAX_PATH + 256)
#define MAX_LINE 512

static char project_root[MAX_PATH] = ".";
static char login_root[MAX_PATH] = ".";
static char user_id[128] = "user_01";
static char user_uuid[128] = "";
static char xyzfs_rel[512] = "";

static void resolve_root(void) {
    const char *env = getenv("PRISC_PROJECT_ROOT");
    if (env && env[0]) snprintf(project_root, sizeof(project_root), "%s", env);
}

static void resolve_login_root(void) {
    const char *env = getenv("USERPAL_LOGIN_ROOT");
    if (env && env[0]) {
        snprintf(login_root, sizeof(login_root), "%s", env);
        return;
    }
    /* Sibling of this project: ../00.login-signup */
    char cand[PATH_BUF];
    snprintf(cand, sizeof(cand), "%s/../00.login-signup", project_root);
    char real[MAX_PATH];
    if (realpath(cand, real)) {
        snprintf(login_root, sizeof(login_root), "%s", real);
        return;
    }
    /* From session dir: project_root is session; install is a few links up */
    snprintf(cand, sizeof(cand), "%s/../../../00.login-signup", project_root);
    if (realpath(cand, real)) {
        snprintf(login_root, sizeof(login_root), "%s", real);
        return;
    }
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

/* Prefer xyzfs/session.pdl (authoritative for logged-in vs guest), then
 * current_login.txt, then local guest fallback. */
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

static void write_session_pdl_full(const char *mode, const char *uid, const char *uuuid,
                                   const char *dname, const char *xyz,
                                   const char *av_uuid, const char *av_path) {
    char dir[PATH_BUF], path[PATH_BUF];
    snprintf(dir, sizeof(dir), "%s/xyzfs", login_root);
    mkdir(dir, 0755);
    snprintf(path, sizeof(path), "%s/session.pdl", dir);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "SECTION      | KEY                | VALUE\n");
    fprintf(f, "----------------------------------------\n");
    fprintf(f, "META         | piece_id           | xyzfs_session\n");
    fprintf(f, "META         | version            | 1.0\n");
    fprintf(f, "META         | notes              | Current logged-in user or guest for this machine\n\n");
    fprintf(f, "STATE        | mode                 | %s\n", mode && mode[0] ? mode : "guest");
    fprintf(f, "STATE        | user_id              | %s\n", uid ? uid : "");
    fprintf(f, "STATE        | user_uuid            | %s\n", uuuid ? uuuid : "");
    fprintf(f, "STATE        | display_name         | %s\n", dname && dname[0] ? dname : "Guest");
    fprintf(f, "STATE        | xyzfs_path           | %s\n", xyz ? xyz : "");
    fprintf(f, "STATE        | logged_in_at         | %ld\n", (long)time(NULL));
    fprintf(f, "STATE        | active_avatar_uuid   | %s\n", av_uuid ? av_uuid : "");
    fprintf(f, "STATE        | active_avatar_path   | %s\n", av_path ? av_path : "");
    fprintf(f, "\n# updated by avatar-creation when a character is minted/opened\n");
    fclose(f);
}

static int mint_uuid(char *out, size_t out_sz); /* forward */

static void load_current_user(void) {
    char mode[64] = "";
    read_session_state("mode", mode, sizeof(mode));
    read_session_state("user_id", user_id, sizeof(user_id));
    read_session_state("user_uuid", user_uuid, sizeof(user_uuid));
    read_session_state("xyzfs_path", xyzfs_rel, sizeof(xyzfs_rel));
    if (!user_id[0] || !user_uuid[0]) {
        char path[PATH_BUF];
        snprintf(path, sizeof(path), "%s/current_login.txt", login_root);
        read_kv(path, "current_user_id", user_id, sizeof(user_id));
        read_kv(path, "current_user_uuid", user_uuid, sizeof(user_uuid));
        read_kv(path, "current_xyzfs", xyzfs_rel, sizeof(xyzfs_rel));
    }
    /* Guest: durable guest-<uuid> under xyzfs/users/guest-<uuid>/ */
    int is_guest = (strcmp(mode, "logged_in") != 0) ||
                   !user_uuid[0] ||
                   (user_id[0] && strncmp(user_id, "guest", 5) == 0);
    if (is_guest) {
        if (!user_uuid[0]) mint_uuid(user_uuid, sizeof(user_uuid));
        if (!user_id[0] || strcmp(user_id, "guest") == 0)
            snprintf(user_id, sizeof(user_id), "guest-%.8s", user_uuid);
        if (!xyzfs_rel[0] || !strstr(xyzfs_rel, "guest-"))
            snprintf(xyzfs_rel, sizeof(xyzfs_rel), "xyzfs/users/guest-%s", user_uuid);
        char cpath[PATH_BUF];
        snprintf(cpath, sizeof(cpath), "%s/current_login.txt", login_root);
        FILE *cf = fopen(cpath, "w");
        if (cf) {
            fprintf(cf, "current_user_id=%s\n", user_id);
            fprintf(cf, "current_user_uuid=%s\n", user_uuid);
            fprintf(cf, "current_xyzfs=%s\n", xyzfs_rel);
            fclose(cf);
        }
    } else if (!xyzfs_rel[0] && user_uuid[0]) {
        snprintf(xyzfs_rel, sizeof(xyzfs_rel), "xyzfs/users/%s", user_uuid);
    }
    if (!user_id[0]) snprintf(user_id, sizeof(user_id), "guest");
}

static int mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_BUF];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp)) return -1;
    snprintf(tmp, sizeof(tmp), "%s", path);
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

static int mint_uuid(char *out, size_t out_sz) {
    FILE *f = fopen("/proc/sys/kernel/random/uuid", "r");
    if (f) {
        if (fgets(out, (int)out_sz, f)) {
            out[strcspn(out, "\r\n")] = '\0';
            fclose(f);
            if (strlen(out) >= 32) return 0;
        } else fclose(f);
    }
    unsigned int r = (unsigned int)(time(NULL) ^ (getpid() << 16));
    snprintf(out, out_sz, "%08x-%04x-%04x-%04x-%08x%04x",
             r, (unsigned)(getpid() & 0xffff),
             (unsigned)((r >> 8) & 0xffff), (unsigned)((r >> 16) & 0xffff),
             (unsigned)time(NULL), (unsigned)(getpid() & 0xffff));
    return 0;
}

static void append_inv(const char *path, const char *id) {
    FILE *f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", id);
    fclose(f);
}

static void write_state(const char *dir, const char *uuid, const char *name) {
    char path[PATH_BUF];
    snprintf(path, sizeof(path), "%s/state.txt", dir);
    FILE *sf = fopen(path, "w");
    if (!sf) return;
    fprintf(sf, "uuid=%s\n", uuid);
    fprintf(sf, "name=%s\n", name);
    fprintf(sf, "type=avatar\n");
    fprintf(sf, "age=18\n");
    fprintf(sf, "gender=male\n");
    fprintf(sf, "skin_index=1\n");
    fprintf(sf, "skin_emoji=👨🏻\n");
    fprintf(sf, "species_emoji=👨🏻\n");
    fprintf(sf, "hair_color=brown\n");
    fprintf(sf, "shirt_color=blue\n");
    fprintf(sf, "pants_color=black\n");
    fprintf(sf, "height=170\n");
    fprintf(sf, "weight=70\n");
    fprintf(sf, "energy=100\n");
    fprintf(sf, "asleep=0\n");
    fprintf(sf, "hunger=100\n");
    fprintf(sf, "owner_user_id=%s\n", user_id);
    fprintf(sf, "owner_user_uuid=%s\n", user_uuid);
    fprintf(sf, "created_at=%ld\n", (long)time(NULL));
    fprintf(sf, "grid_x=2\n");
    fprintf(sf, "grid_y=2\n");
    fprintf(sf, "z=0\n");
    fprintf(sf, "facing_left=0\n");
    fprintf(sf, "tick_seq=0\n");
    fclose(sf);

    char pdl[PATH_BUF];
    snprintf(pdl, sizeof(pdl), "%s/piece.pdl", dir);
    FILE *pf = fopen(pdl, "w");
    if (pf) {
        fprintf(pf, "SECTION      | KEY                | VALUE\n");
        fprintf(pf, "----------------------------------------\n\n");
        fprintf(pf, "META         | piece_id           | %s\n", uuid);
        fprintf(pf, "META         | version            | 1.0\n\n");
        fprintf(pf, "METHOD       | customize            | CUSTOMIZE\n");
        fclose(pf);
    }
}

/* Best-effort emoji -> sprite for desktop window (circle fallback if tools missing). */
static void try_build_sprite(const char *dir, const char *emoji) {
    char atlas[PATH_BUF], csv[PATH_BUF], cmd[PATH_BUF * 2];
    snprintf(atlas, sizeof(atlas), "%s/atlas.png", dir);
    snprintf(csv, sizeof(csv), "%s/sprite.csv", dir);
    snprintf(cmd, sizeof(cmd),
             "cd '%s' && ./system/emoji_gen_atlas '%s' '%s' >/dev/null 2>&1 && "
             "./system/emoji_xtract '%s' '%s' >/dev/null 2>&1",
             project_root, emoji, atlas, atlas, csv);
    /* project_root may be session - binaries via symlink */
    system(cmd);
}

int main(int argc, char **argv) {
    resolve_root();
    resolve_login_root();
    load_current_user();

    const char *name = (argc >= 2 && argv[1][0]) ? argv[1] : "Clone";

    char uuid[64];
    mint_uuid(uuid, sizeof(uuid));

    /* Local piece for desktop windows (egg_window path contract). */
    char local_dir[PATH_BUF];
    snprintf(local_dir, sizeof(local_dir), "%s/pieces/world_01/map_lobby/%s", project_root, uuid);
    mkdir_p(local_dir, 0755);
    write_state(local_dir, uuid, name);
    try_build_sprite(local_dir, "👨🏻");

    char local_inv[PATH_BUF];
    snprintf(local_inv, sizeof(local_inv), "%s/pieces/world_01/map_lobby/user_01/inventory.txt", project_root);
    mkdir_p(project_root[0] ? project_root : ".", 0755);
    {
        char d[PATH_BUF];
        snprintf(d, sizeof(d), "%s/pieces/world_01/map_lobby/user_01", project_root);
        mkdir_p(d, 0755);
    }
    append_inv(local_inv, uuid);

    /* Always save under xyzfs (logged_in users/<uuid> or guest-<uuid>). */
    if (!xyzfs_rel[0] || !user_uuid[0]) {
        /* last resort guest root */
        if (!user_uuid[0]) mint_uuid(user_uuid, sizeof(user_uuid));
        snprintf(user_id, sizeof(user_id), "guest-%.8s", user_uuid);
        snprintf(xyzfs_rel, sizeof(xyzfs_rel), "xyzfs/users/guest-%s", user_uuid);
    }
    {
        char xyz_dir[PATH_BUF];
        snprintf(xyz_dir, sizeof(xyz_dir), "%s/%s/home/avatars/%s", login_root, xyzfs_rel, uuid);
        mkdir_p(xyz_dir, 0755);
        write_state(xyz_dir, uuid, name);
        try_build_sprite(xyz_dir, "👨🏻");

        char inv[PATH_BUF];
        snprintf(inv, sizeof(inv), "%s/%s/home/avatars/inventory.txt", login_root, xyzfs_rel);
        {
            char d[PATH_BUF];
            snprintf(d, sizeof(d), "%s/%s/home/avatars", login_root, xyzfs_rel);
            mkdir_p(d, 0755);
        }
        append_inv(inv, uuid);

        char wallet[PATH_BUF];
        snprintf(wallet, sizeof(wallet), "%s/%s/home/wallet.txt", login_root, xyzfs_rel);
        struct stat st;
        if (stat(wallet, &st) != 0) {
            FILE *wf = fopen(wallet, "w");
            if (wf) { fprintf(wf, "tokens=0\n"); fclose(wf); }
        }

        char dname[128], mode[64];
        read_session_state("display_name", dname, sizeof(dname));
        read_session_state("mode", mode, sizeof(mode));
        if (!mode[0])
            snprintf(mode, sizeof(mode),
                     (strncmp(user_id, "guest", 5) == 0) ? "guest" : "logged_in");
        if (!dname[0]) snprintf(dname, sizeof(dname), "%s", user_id);
        char av_path[PATH_BUF];
        snprintf(av_path, sizeof(av_path), "%s/home/avatars/%s", xyzfs_rel, uuid);
        write_session_pdl_full(mode, user_id, user_uuid, dname, xyzfs_rel, uuid, av_path);

        char cpath[PATH_BUF];
        snprintf(cpath, sizeof(cpath), "%s/current_login.txt", login_root);
        FILE *cf = fopen(cpath, "w");
        if (cf) {
            fprintf(cf, "current_user_id=%s\n", user_id);
            fprintf(cf, "current_user_uuid=%s\n", user_uuid);
            fprintf(cf, "current_xyzfs=%s\n", xyzfs_rel);
            fclose(cf);
        }
    }

    printf("%s\n", uuid);
    return 0;
}
