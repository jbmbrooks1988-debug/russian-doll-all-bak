/* khtpm_core.c — shared KHTPM design logic (no X11, no Win32 UI). */
#include "khtpm_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#  define KHTPM_STAT_OK(p) (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES || path_exists_wide(p))
static int path_exists_wide(const char *utf8) {
    wchar_t w[KHTPM_PATH_BUF];
    if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, KHTPM_PATH_BUF) &&
        !MultiByteToWideChar(CP_ACP, 0, utf8, -1, w, KHTPM_PATH_BUF))
        return _access(utf8, 0) == 0;
    return GetFileAttributesW(w) != INVALID_FILE_ATTRIBUTES;
}
static FILE *khtpm_fopen(const char *path, const char *mode) {
    wchar_t wp[KHTPM_PATH_BUF], wm[16];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, KHTPM_PATH_BUF) &&
        !MultiByteToWideChar(CP_ACP, 0, path, -1, wp, KHTPM_PATH_BUF))
        return fopen(path, mode);
    MultiByteToWideChar(CP_ACP, 0, mode, -1, wm, 16);
    return _wfopen(wp, wm);
}
#else
#  include <sys/stat.h>
#  include <unistd.h>
#  define khtpm_fopen fopen
static int path_exists_wide(const char *p) {
    struct stat st;
    return stat(p, &st) == 0;
}
#  define KHTPM_STAT_OK path_exists_wide
#endif

void khtpm_path_join(char *out, size_t n, const char *a, const char *b) {
    if (!a || !a[0] || strcmp(a, ".") == 0) {
        snprintf(out, n, "%s", b ? b : "");
        return;
    }
    if (!b || !b[0]) {
        snprintf(out, n, "%s", a);
        return;
    }
    size_t al = strlen(a);
    if (a[al - 1] == '/' || a[al - 1] == '\\')
        snprintf(out, n, "%s%s", a, b);
    else
#ifdef _WIN32
        snprintf(out, n, "%s\\%s", a, b);
#else
        snprintf(out, n, "%s/%s", a, b);
#endif
}

void khtpm_path_norm(char *s) {
#ifdef _WIN32
    for (; *s; s++) if (*s == '/') *s = '\\';
#else
    (void)s;
#endif
}

static void trim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == '\t'))
        s[--n] = 0;
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

static void read_line_file(const char *path, char *out, size_t n) {
    out[0] = 0;
    FILE *f = khtpm_fopen(path, "r");
    if (!f) return;
    if (fgets(out, (int)n, f)) out[strcspn(out, "\r\n")] = 0;
    fclose(f);
}

void khtpm_history(const KhtpmEntity *e, const char *fmt, ...) {
    FILE *f = khtpm_fopen(e->history_path, "a");
    if (!f) return;
    fprintf(f, "%ld ", (long)time(NULL));
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

void khtpm_relay_clear(const KhtpmEntity *e) {
    FILE *f = khtpm_fopen(e->relay_path, "w");
    if (f) fclose(f);
}

int khtpm_relay_poll(const KhtpmEntity *e, int *raise_out, int *open_menu_out) {
    if (raise_out) *raise_out = 0;
    if (open_menu_out) *open_menu_out = 0;
    FILE *f = khtpm_fopen(e->relay_path, "r");
    if (!f) return 0;
    char line[512];
    int close_me = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "CLOSE", 5) == 0) close_me = 1;
        else if (strncmp(line, "OPEN_CONTEXT", 12) == 0 && open_menu_out) *open_menu_out = 1;
        else if (strncmp(line, "ACTIVATE", 8) == 0 && raise_out) *raise_out = 1;
    }
    fclose(f);
    khtpm_relay_clear(e);
    return close_me;
}

int khtpm_sprite_load(KhtpmEntity *e) {
    char path[KHTPM_PATH_BUF];
    khtpm_path_join(path, sizeof(path), e->package_dir, "sprite.csv");
    FILE *f = khtpm_fopen(path, "r");
    if (!f) return 0;
    char line[256];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "# resolution=", 13) == 0) { res = atoi(line + 13); break; }
    }
    if (res <= 0) { fclose(f); return 0; }
    unsigned char *px = (unsigned char *)malloc((size_t)res * res * 4);
    if (!px) { fclose(f); return 0; }
    int count = 0;
    while (count < res * res && fgets(line, sizeof(line), f)) {
        int r, g, b, a;
        if (sscanf(line, "%d,%d,%d,%d", &r, &g, &b, &a) == 4) {
            px[count * 4 + 0] = (unsigned char)r;
            px[count * 4 + 1] = (unsigned char)g;
            px[count * 4 + 2] = (unsigned char)b;
            px[count * 4 + 3] = (unsigned char)a;
            count++;
        }
    }
    fclose(f);
    if (count != res * res) { free(px); return 0; }
    khtpm_sprite_free(e);
    e->sprite_pixels = px;
    e->sprite_res = res;
    return 1;
}

void khtpm_sprite_free(KhtpmEntity *e) {
    if (e->sprite_pixels) {
        free(e->sprite_pixels);
        e->sprite_pixels = NULL;
    }
    e->sprite_res = 0;
}

void khtpm_pos_read(const KhtpmEntity *e, int *x, int *y) {
    char path[KHTPM_PATH_BUF];
    khtpm_path_join(path, sizeof(path), e->package_dir, "desktop_pos.txt");
    *x = 100; *y = 100;
    FILE *f = khtpm_fopen(path, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "x=", 2) == 0) *x = atoi(line + 2);
        if (strncmp(line, "y=", 2) == 0) *y = atoi(line + 2);
    }
    fclose(f);
}

void khtpm_pos_write(const KhtpmEntity *e, int x, int y) {
    char path[KHTPM_PATH_BUF];
    khtpm_path_join(path, sizeof(path), e->package_dir, "desktop_pos.txt");
    FILE *f = khtpm_fopen(path, "w");
    if (!f) return;
    fprintf(f, "x=%d\ny=%d\n", x, y);
    fclose(f);
}

void khtpm_pos_clamp(int *x, int *y, int win_px, int screen_w, int screen_h) {
    int max_x = screen_w - win_px - 8;
    /* Win livedesk bar sits at TOP (KTB_BAR_H); leave bottom for OS taskbar */
    int top_pad = 40;
    int bot_pad = 48;
    int max_y = screen_h - win_px - bot_pad;
    if (max_x < 0) max_x = 0;
    if (max_y < top_pad) max_y = top_pad;
    if (*x < 0) *x = 0;
    if (*y < top_pad) *y = top_pad;
    if (*x > max_x) *x = max_x;
    if (*y > max_y) *y = max_y;
    *x = (*x / KHTPM_GRID_CELL_PX) * KHTPM_GRID_CELL_PX;
    *y = (*y / KHTPM_GRID_CELL_PX) * KHTPM_GRID_CELL_PX;
    if (*y < top_pad) *y = top_pad;
}

int khtpm_registry_next_index(const char *house_root) {
    char path[KHTPM_PATH_BUF];
    khtpm_path_join(path, sizeof(path), house_root, "#.desktop/livedesk_next_index.txt");
    khtpm_path_norm(path);
    int idx = 1;
    FILE *f = khtpm_fopen(path, "r");
    if (f) { if (fscanf(f, "%d", &idx) != 1) idx = 1; fclose(f); }
    f = khtpm_fopen(path, "w");
    if (f) { fprintf(f, "%d\n", idx + 1); fclose(f); }
    return idx;
}

void khtpm_registry_add(const KhtpmEntity *e, int pid) {
    char path[KHTPM_PATH_BUF];
    khtpm_path_join(path, sizeof(path), e->house_root, "#.desktop/livedesk_open.txt");
    khtpm_path_norm(path);
    FILE *f = khtpm_fopen(path, "a");
    if (f) {
        fprintf(f, "PID=%d|INDEX=%d|ENTITY=%s|PATH=%s\n",
                pid, e->livedesk_index, e->entity, e->package_dir);
        fclose(f);
    }
    khtpm_path_join(path, sizeof(path), e->house_root, "#.desktop/livedesk_nav_claims.txt");
    khtpm_path_norm(path);
    f = khtpm_fopen(path, "a");
    if (f) {
        fprintf(f, "KIND=tab|PID=%d|NAV=%d|ENTITY=%s|PATH=%s\n",
                pid, e->livedesk_index, e->entity, e->package_dir);
        fclose(f);
    }
}

void khtpm_registry_remove(const char *house_root, int pid) {
    const char *files[] = {
        "#.desktop/livedesk_open.txt",
        "#.desktop/livedesk_nav_claims.txt",
        NULL
    };
    char needle[64];
    snprintf(needle, sizeof(needle), "PID=%d", pid);
    for (int fi = 0; files[fi]; fi++) {
        char path[KHTPM_PATH_BUF], tmp[KHTPM_PATH_BUF];
        khtpm_path_join(path, sizeof(path), house_root, files[fi]);
        khtpm_path_norm(path);
        snprintf(tmp, sizeof(tmp), "%s.tmp", path);
        FILE *f = khtpm_fopen(path, "r");
        if (!f) continue;
        FILE *w = khtpm_fopen(tmp, "w");
        if (!w) { fclose(f); continue; }
        char line[KHTPM_PATH_BUF];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, needle)) continue;
            fputs(line, w);
        }
        fclose(f); fclose(w);
        remove(path);
        rename(tmp, path);
    }
}

/* ---- menus: port of Linux load_methods / load_objects design ---- */

static int load_methods(const char *package_dir, KhtpmMethod *items, int max) {
    char path[KHTPM_PATH_BUF];
    khtpm_path_join(path, sizeof(path), package_dir, "meta.pdl");
    FILE *f = khtpm_fopen(path, "r");
    if (!f) return 0;
    char line[KHTPM_PATH_BUF];
    int n = 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (strncmp(line, "METHOD", 6) != 0) continue;
        char *p = strchr(line, '|');
        if (!p) continue;
        p++;
        while (*p == ' ') p++;
        char *end = strchr(p, '|');
        if (!end) continue;
        char *label_end = end;
        while (label_end > p && label_end[-1] == ' ') label_end--;
        size_t llen = (size_t)(label_end - p);
        if (llen == 0 || llen >= sizeof(items[0].label)) continue;
        memcpy(items[n].label, p, llen);
        items[n].label[llen] = '\0';
        char *a = end + 1;
        while (*a == ' ') a++;
        char *a_end = a + strcspn(a, "\r\n");
        while (a_end > a && a_end[-1] == ' ') a_end--;
        size_t alen = (size_t)(a_end - a);
        if (alen == 0 || alen >= sizeof(items[0].action)) continue;
        memcpy(items[n].action, a, alen);
        items[n].action[alen] = '\0';
        n++;
    }
    fclose(f);
    return n;
}

static int load_objects(const char *package_dir, KhtpmPage *pages, int max_pages) {
    char path[KHTPM_PATH_BUF];
    khtpm_path_join(path, sizeof(path), package_dir, "objects.pdl");
    FILE *f = khtpm_fopen(path, "r");
    if (!f) return 0;
    char line[KHTPM_PATH_BUF];
    int n_pages = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "PAGE", 4) == 0) {
            char *p = strchr(line, '|');
            if (!p) continue;
            p++;
            while (*p == ' ') p++;
            char *end = p + strcspn(p, "\r\n");
            while (end > p && end[-1] == ' ') end--;
            size_t nlen = (size_t)(end - p);
            if (nlen == 0 || n_pages >= max_pages) continue;
            if (nlen >= sizeof(pages[n_pages].name)) nlen = sizeof(pages[n_pages].name) - 1;
            memcpy(pages[n_pages].name, p, nlen);
            pages[n_pages].name[nlen] = '\0';
            pages[n_pages].n_items = 0;
            n_pages++;
        } else if (strncmp(line, "OBJECT", 6) == 0 && n_pages > 0) {
            KhtpmPage *pg = &pages[n_pages - 1];
            if (pg->n_items >= KHTPM_MAX_METHODS) continue;
            KhtpmMethod *item = &pg->items[pg->n_items];
            item->label[0] = item->action[0] = '\0';
            char *tok = line;
            while ((tok = strchr(tok, '|')) != NULL) {
                tok++;
                while (*tok == ' ') tok++;
                char *tok_end = strchr(tok, '|');
                size_t tlen = tok_end ? (size_t)(tok_end - tok) : strcspn(tok, "\r\n");
                while (tlen > 0 && tok[tlen - 1] == ' ') tlen--;
                if (strncmp(tok, "label=", 6) == 0) {
                    size_t l = tlen - 6;
                    if (l >= sizeof(item->label)) l = sizeof(item->label) - 1;
                    memcpy(item->label, tok + 6, l);
                    item->label[l] = '\0';
                } else if (strncmp(tok, "action=", 7) == 0) {
                    size_t l = tlen - 7;
                    if (l >= sizeof(item->action)) l = sizeof(item->action) - 1;
                    memcpy(item->action, tok + 7, l);
                    item->action[l] = '\0';
                }
                tok = tok_end ? tok_end : tok + strlen(tok);
            }
            if (item->label[0]) pg->n_items++;
        }
    }
    fclose(f);
    /* auto Cancel like Linux */
    for (int pi = 0; pi < n_pages; pi++) {
        int has_cancel = 0;
        for (int i = 0; i < pages[pi].n_items; i++)
            if (strcmp(pages[pi].items[i].label, "Cancel") == 0) { has_cancel = 1; break; }
        if (!has_cancel && pages[pi].n_items < KHTPM_MAX_METHODS) {
            snprintf(pages[pi].items[pages[pi].n_items].label,
                     sizeof(pages[pi].items[0].label), "Cancel");
            snprintf(pages[pi].items[pages[pi].n_items].action,
                     sizeof(pages[pi].items[0].action), "void");
            pages[pi].n_items++;
        }
    }
    return n_pages;
}

static int read_footprint(const char *package_dir) {
    char path[KHTPM_PATH_BUF];
    khtpm_path_join(path, sizeof(path), package_dir, "meta.pdl");
    FILE *f = khtpm_fopen(path, "r");
    if (!f) return 1;
    char line[KHTPM_PATH_BUF];
    int result = 1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "STATE", 5) != 0) continue;
        if (!strstr(line, "footprint_tiles")) continue;
        char *v = strrchr(line, '|');
        if (v) {
            int t = atoi(v + 1);
            if (t > 0) result = t;
        }
        break;
    }
    fclose(f);
    return result;
}

int khtpm_entity_init(KhtpmEntity *e, const char *package_dir, const char *house_root) {
    memset(e, 0, sizeof(*e));
    snprintf(e->package_dir, sizeof(e->package_dir), "%s", package_dir ? package_dir : ".");
    snprintf(e->house_root, sizeof(e->house_root), "%s",
             (house_root && house_root[0]) ? house_root : ".");
    khtpm_path_norm(e->package_dir);
    if (strncmp(e->package_dir, ".\\", 2) == 0)
        memmove(e->package_dir, e->package_dir + 2, strlen(e->package_dir + 2) + 1);

    const char *base = e->package_dir;
    const char *s = strrchr(e->package_dir, '\\');
    const char *s2 = strrchr(e->package_dir, '/');
    if (s2 && (!s || s2 > s)) s = s2;
    if (s && s[1]) base = s + 1;
    snprintf(e->entity, sizeof(e->entity), "%.120s", base);

    char path[KHTPM_PATH_BUF];
    khtpm_path_join(path, sizeof(path), e->package_dir, "glyph.txt");
    read_line_file(path, e->glyph, sizeof(e->glyph));
    if (!e->glyph[0]) snprintf(e->glyph, sizeof(e->glyph), "?");

    char iid[32] = "";
    khtpm_path_join(path, sizeof(path), e->package_dir, "instance_id.txt");
    read_line_file(path, iid, sizeof(iid));
    if (iid[0])
        snprintf(e->full_id, sizeof(e->full_id), "%s-%s", e->entity, iid);
    else
        snprintf(e->full_id, sizeof(e->full_id), "%s", e->entity);

    e->footprint_tiles = read_footprint(e->package_dir);
    if (e->footprint_tiles < 1) e->footprint_tiles = 1;
    if (e->footprint_tiles > 8) e->footprint_tiles = 8;
    e->win_px = KHTPM_DEFAULT_WIN_PX * e->footprint_tiles;

    khtpm_path_join(e->history_path, sizeof(e->history_path), e->package_dir, "history.txt");
    khtpm_path_join(e->relay_path, sizeof(e->relay_path), e->package_dir, "interact_relay.txt");
    khtpm_path_norm(e->history_path);
    khtpm_path_norm(e->relay_path);
    khtpm_relay_clear(e);

    e->livedesk_index = khtpm_registry_next_index(e->house_root);
    e->cur_page = -1;
    e->page_stack_n = 0;
    e->n_pages = 0;
    e->n_view = 0;
    return 1;
}

void khtpm_entity_shutdown(KhtpmEntity *e) {
    khtpm_sprite_free(e);
}

int khtpm_menu_load(KhtpmEntity *e) {
    e->n_pages = load_objects(e->package_dir, e->pages, KHTPM_MAX_PAGES);
    e->page_stack_n = 0;
    e->cur_page = -1;
    if (e->n_pages > 0) {
        /* prefer page named main */
        e->cur_page = 0;
        for (int i = 0; i < e->n_pages; i++)
            if (strcmp(e->pages[i].name, "main") == 0) { e->cur_page = i; break; }
    }
    khtpm_menu_build_view(e);
    return e->n_view;
}

void khtpm_menu_build_view(KhtpmEntity *e) {
    e->n_view = 0;
    if (e->n_pages > 0 && e->cur_page >= 0 && e->cur_page < e->n_pages) {
        KhtpmPage *pg = &e->pages[e->cur_page];
        for (int i = 0; i < pg->n_items && e->n_view < KHTPM_MAX_METHODS; i++)
            e->view[e->n_view++] = pg->items[i];
        /* also surface meta methods on main page */
        if (strcmp(pg->name, "main") == 0) {
            KhtpmMethod methods[KHTPM_MAX_METHODS];
            int nm = load_methods(e->package_dir, methods, KHTPM_MAX_METHODS);
            for (int i = 0; i < nm && e->n_view < KHTPM_MAX_METHODS; i++) {
                int dup = 0;
                for (int j = 0; j < e->n_view; j++)
                    if (strcmp(e->view[j].label, methods[i].label) == 0) { dup = 1; break; }
                if (!dup) e->view[e->n_view++] = methods[i];
            }
        }
    } else {
        e->n_view = load_methods(e->package_dir, e->view, KHTPM_MAX_METHODS);
    }
    if (e->n_view == 0) {
        snprintf(e->view[0].label, sizeof(e->view[0].label), "Close");
        snprintf(e->view[0].action, sizeof(e->view[0].action), "CLOSE");
        e->n_view = 1;
    }
}

void khtpm_action_make_portable(const char *in, char *out, size_t out_sz) {
    if (!in) { out[0] = 0; return; }
    /* strip surrounding quotes from meta.pdl METHOD values */
    char buf[KHTPM_PATH_BUF];
    snprintf(buf, sizeof(buf), "%s", in);
    size_t bl = strlen(buf);
    while (bl && (buf[bl - 1] == ' ' || buf[bl - 1] == '\r' || buf[bl - 1] == '\n' ||
                  buf[bl - 1] == '"' || buf[bl - 1] == '\''))
        buf[--bl] = 0;
    char *start = buf;
    while (*start == ' ' || *start == '"' || *start == '\'') start++;

    const char *markers[] = {
        "/@.apps/", "/#.desktop/", "/&.widgits/", "/$.crypts/",
        "\\@.apps\\", "\\#.desktop\\", "\\&.widgits\\", "\\$.crypts\\",
        NULL
    };
    for (int i = 0; markers[i]; i++) {
        const char *m = strstr(start, markers[i]);
        if (m) {
            snprintf(out, out_sz, "%s", m + 1);
            return;
        }
    }
    snprintf(out, out_sz, "%s", start);
}

int khtpm_menu_apply(KhtpmEntity *e, const char *action,
                     char *out_cmd, size_t out_cmd_sz) {
    if (out_cmd && out_cmd_sz) out_cmd[0] = 0;
    if (!action || !action[0] || strcmp(action, "void") == 0)
        return KHTPM_ACT_NONE;
    if (strcmp(action, "CLOSE") == 0) {
        khtpm_history(e, "menu CLOSE");
        return KHTPM_ACT_CLOSE;
    }
    if (strncmp(action, "GOTO:", 5) == 0) {
        const char *name = action + 5;
        for (int i = 0; i < e->n_pages; i++) {
            if (strcmp(e->pages[i].name, name) == 0) {
                if (e->page_stack_n < KHTPM_MAX_PAGES)
                    e->page_stack[e->page_stack_n++] = e->cur_page;
                e->cur_page = i;
                khtpm_menu_build_view(e);
                khtpm_history(e, "menu GOTO %s", name);
                return KHTPM_ACT_RAISE_MENU;
            }
        }
        return KHTPM_ACT_NONE;
    }
    if (strncmp(action, "BACK", 4) == 0) {
        if (e->page_stack_n > 0) {
            e->cur_page = e->page_stack[--e->page_stack_n];
            khtpm_menu_build_view(e);
            khtpm_history(e, "menu BACK");
            return KHTPM_ACT_RAISE_MENU;
        }
        /* fallback main */
        for (int i = 0; i < e->n_pages; i++)
            if (strcmp(e->pages[i].name, "main") == 0) {
                e->cur_page = i;
                khtpm_menu_build_view(e);
                return KHTPM_ACT_RAISE_MENU;
            }
        return KHTPM_ACT_NONE;
    }
    if (strncmp(action, "STATE:", 6) == 0) {
        khtpm_history(e, "menu STATE deferred %s", action);
        return KHTPM_ACT_SKIP;
    }
    if (strcmp(action, "xdg-open") == 0) {
        if (out_cmd) snprintf(out_cmd, out_cmd_sz, "%s", e->package_dir);
        return KHTPM_ACT_OPEN_DIR;
    }
    if (strstr(action, "gedit") || strstr(action, "gnome-terminal")) {
        khtpm_history(e, "linux-only skipped: %s", action);
        return KHTPM_ACT_SKIP;
    }
    if (out_cmd) {
        khtpm_action_make_portable(action, out_cmd, out_cmd_sz);
        khtpm_history(e, "menu RUN %s", out_cmd);
    }
    return KHTPM_ACT_RUN;
}

int khtpm_package_exists(const KhtpmEntity *e) {
    char path[KHTPM_PATH_BUF];
    khtpm_path_join(path, sizeof(path), e->package_dir, "glyph.txt");
    khtpm_path_norm(path);
    return path_exists_wide(path);
}

void khtpm_taskbar_exe_rel(char *out, size_t n) {
    snprintf(out, n, "&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.exe");
}
