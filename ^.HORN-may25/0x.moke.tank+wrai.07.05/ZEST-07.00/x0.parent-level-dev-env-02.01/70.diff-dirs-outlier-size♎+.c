#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>

#define MAX_PATH_LEN PATH_MAX

/* Mirrors 69.list+dirs...CONT.c's output_file/print_to_both convention:
   every report line goes to both stdout and a results file, so a run can
   be pasted/grepped/diffed later without re-running the walk. */
static FILE *output_file = NULL;

static void print_to_both(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    if (output_file) {
        va_start(args, format);
        vfprintf(output_file, format, args);
        va_end(args);
    }
}

/* One file, recorded relative to whichever root it was found under, so
   the same relpath from tree A and tree B can be compared directly. */
typedef struct {
    char relpath[MAX_PATH_LEN];
    long long size;
} FileEntry;

typedef struct {
    FileEntry *items;
    size_t count;
    size_t capacity;
} FileList;

static void filelist_init(FileList *fl) {
    fl->items = NULL;
    fl->count = 0;
    fl->capacity = 0;
}

static void filelist_add(FileList *fl, const char *relpath, long long size) {
    if (fl->count == fl->capacity) {
        size_t new_cap = fl->capacity ? fl->capacity * 2 : 256;
        FileEntry *grown = realloc(fl->items, new_cap * sizeof(FileEntry));
        if (!grown) {
            fprintf(stderr, "Out of memory growing file list\n");
            exit(1);
        }
        fl->items = grown;
        fl->capacity = new_cap;
    }
    snprintf(fl->items[fl->count].relpath, MAX_PATH_LEN, "%s", relpath);
    fl->items[fl->count].size = size;
    fl->count++;
}

/* Recursively walks `root`/`rel`, recording every regular file (lstat'd,
   symlinks not followed) into fl with a path relative to `root` (so two
   different absolute roots produce directly-comparable relpaths).
   Directories are recursed into but not themselves recorded -- a dir's
   own st_size is filesystem block metadata, not real content size, and
   isn't useful for "what got bigger". Symlinks are skipped entirely
   (neither recursed into nor sized) to avoid loops and double-counting. */
static void walk_dir(const char *root, const char *rel, FileList *fl) {
    char full_path[MAX_PATH_LEN];
    DIR *dir;
    struct dirent *entry;

    if (rel[0] == '\0') {
        snprintf(full_path, MAX_PATH_LEN, "%s", root);
    } else {
        snprintf(full_path, MAX_PATH_LEN, "%s/%s", root, rel);
    }

    dir = opendir(full_path);
    if (!dir) {
        fprintf(stderr, "Warning: could not open %s: %s\n", full_path, strerror(errno));
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char child_full[MAX_PATH_LEN];
        char child_rel[MAX_PATH_LEN];
        struct stat st;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        if (snprintf(child_full, MAX_PATH_LEN, "%s/%s", full_path, entry->d_name) >= MAX_PATH_LEN) {
            fprintf(stderr, "Warning: path too long, skipping under %s/%s\n", full_path, entry->d_name);
            continue;
        }
        if (rel[0] == '\0') {
            snprintf(child_rel, MAX_PATH_LEN, "%s", entry->d_name);
        } else {
            snprintf(child_rel, MAX_PATH_LEN, "%s/%s", rel, entry->d_name);
        }

        if (lstat(child_full, &st) != 0) {
            fprintf(stderr, "Warning: could not stat %s: %s\n", child_full, strerror(errno));
            continue;
        }

        if (S_ISLNK(st.st_mode)) {
            continue; /* don't follow, don't size */
        } else if (S_ISDIR(st.st_mode)) {
            walk_dir(root, child_rel, fl);
        } else if (S_ISREG(st.st_mode)) {
            filelist_add(fl, child_rel, (long long)st.st_size);
        }
    }
    closedir(dir);
}

static int cmp_by_relpath(const void *a, const void *b) {
    return strcmp(((const FileEntry *)a)->relpath, ((const FileEntry *)b)->relpath);
}

static int cmp_by_size_desc(const void *a, const void *b) {
    long long sa = ((const FileEntry *)a)->size;
    long long sb = ((const FileEntry *)b)->size;
    if (sa > sb) return -1;
    if (sa < sb) return 1;
    return 0;
}

typedef struct {
    char relpath[MAX_PATH_LEN];
    long long size_a;
    long long size_b;
    long long delta; /* size_a - size_b; positive = grew in A */
} DiffEntry;

static int cmp_diff_delta_desc(const void *a, const void *b) {
    long long da = ((const DiffEntry *)a)->delta;
    long long db = ((const DiffEntry *)b)->delta;
    if (da > db) return -1;
    if (da < db) return 1;
    return 0;
}

static int cmp_diff_delta_asc(const void *a, const void *b) {
    return cmp_diff_delta_desc(b, a);
}

static void print_size(long long bytes, char *out, size_t out_sz) {
    double v = (double)bytes;
    const char *unit = "B";
    if (v < 0) v = -v;
    if (v >= 1024.0 * 1024.0 * 1024.0) { v /= 1024.0 * 1024.0 * 1024.0; unit = "GB"; }
    else if (v >= 1024.0 * 1024.0) { v /= 1024.0 * 1024.0; unit = "MB"; }
    else if (v >= 1024.0) { v /= 1024.0; unit = "KB"; }
    snprintf(out, out_sz, "%.2f %s", (bytes < 0 ? -v : v), unit);
}

int main(int argc, char *argv[]) {
    FileList list_a, list_b;
    size_t ia, ib;
    size_t top_n = 30;
    long long total_a = 0, total_b = 0, total_new = 0, total_removed = 0, total_grew = 0, total_shrank = 0;
    FileList only_a, only_b;
    DiffEntry *diffs;
    size_t diff_count = 0, diff_cap = 0;
    char buf[64];

    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <dir_a_newer> <dir_b_older_or_reference> [top_n]\n", argv[0]);
        fprintf(stderr, "Reports files only in A, only in B, and files present in both\n");
        fprintf(stderr, "whose size differs -- sorted so the biggest contributors to A\n");
        fprintf(stderr, "being bigger than B show up first. [top_n] caps entries shown\n");
        fprintf(stderr, "per section (default 30, 0 = show all).\n");
        return 1;
    }
    if (argc == 4) {
        top_n = (size_t)atoi(argv[3]);
    }

    output_file = fopen("diff_dirs_outliers.txt", "w");
    if (!output_file) {
        fprintf(stderr, "Warning: could not open diff_dirs_outliers.txt for writing: %s (continuing, stdout only)\n", strerror(errno));
    }

    filelist_init(&list_a);
    filelist_init(&list_b);
    walk_dir(argv[1], "", &list_a);
    walk_dir(argv[2], "", &list_b);

    qsort(list_a.items, list_a.count, sizeof(FileEntry), cmp_by_relpath);
    qsort(list_b.items, list_b.count, sizeof(FileEntry), cmp_by_relpath);

    for (ia = 0; ia < list_a.count; ia++) total_a += list_a.items[ia].size;
    for (ib = 0; ib < list_b.count; ib++) total_b += list_b.items[ib].size;

    filelist_init(&only_a);
    filelist_init(&only_b);
    diffs = NULL;

    ia = 0;
    ib = 0;
    while (ia < list_a.count && ib < list_b.count) {
        int cmp = strcmp(list_a.items[ia].relpath, list_b.items[ib].relpath);
        if (cmp == 0) {
            long long sa = list_a.items[ia].size;
            long long sb = list_b.items[ib].size;
            if (sa != sb) {
                if (diff_count == diff_cap) {
                    size_t new_cap = diff_cap ? diff_cap * 2 : 256;
                    DiffEntry *grown = realloc(diffs, new_cap * sizeof(DiffEntry));
                    if (!grown) { fprintf(stderr, "Out of memory\n"); return 1; }
                    diffs = grown;
                    diff_cap = new_cap;
                }
                snprintf(diffs[diff_count].relpath, MAX_PATH_LEN, "%s", list_a.items[ia].relpath);
                diffs[diff_count].size_a = sa;
                diffs[diff_count].size_b = sb;
                diffs[diff_count].delta = sa - sb;
                if (sa > sb) total_grew += (sa - sb); else total_shrank += (sb - sa);
                diff_count++;
            }
            ia++;
            ib++;
        } else if (cmp < 0) {
            filelist_add(&only_a, list_a.items[ia].relpath, list_a.items[ia].size);
            total_new += list_a.items[ia].size;
            ia++;
        } else {
            filelist_add(&only_b, list_b.items[ib].relpath, list_b.items[ib].size);
            total_removed += list_b.items[ib].size;
            ib++;
        }
    }
    for (; ia < list_a.count; ia++) {
        filelist_add(&only_a, list_a.items[ia].relpath, list_a.items[ia].size);
        total_new += list_a.items[ia].size;
    }
    for (; ib < list_b.count; ib++) {
        filelist_add(&only_b, list_b.items[ib].relpath, list_b.items[ib].size);
        total_removed += list_b.items[ib].size;
    }

    qsort(only_a.items, only_a.count, sizeof(FileEntry), cmp_by_size_desc);
    qsort(only_b.items, only_b.count, sizeof(FileEntry), cmp_by_size_desc);
    if (diff_count > 0) {
        qsort(diffs, diff_count, sizeof(DiffEntry), cmp_diff_delta_desc);
    }

    print_size(total_a, buf, sizeof(buf));
    print_to_both("A (%s): %s total, %zu files\n", argv[1], buf, list_a.count);
    print_size(total_b, buf, sizeof(buf));
    print_to_both("B (%s): %s total, %zu files\n", argv[2], buf, list_b.count);
    print_size(total_a - total_b, buf, sizeof(buf));
    print_to_both("A - B: %s%s\n\n", (total_a - total_b) >= 0 ? "+" : "-", buf);

    print_to_both("=== Files only in A (new since B) -- sorted by size, biggest first ===\n");
    print_to_both("(%zu files, %lld bytes total)\n", only_a.count, total_new);
    {
        size_t shown = (top_n == 0 || only_a.count < top_n) ? only_a.count : top_n;
        size_t k;
        for (k = 0; k < shown; k++) {
            print_size(only_a.items[k].size, buf, sizeof(buf));
            print_to_both("  %-10s %s\n", buf, only_a.items[k].relpath);
        }
        if (shown < only_a.count) print_to_both("  ... and %zu more\n", only_a.count - shown);
    }
    print_to_both("\n");

    print_to_both("=== Files present in both, but grew (A > B) -- sorted by delta, biggest first ===\n");
    print_to_both("(%lld bytes total growth across changed files)\n", total_grew);
    {
        size_t k, shown_count = 0;
        for (k = 0; k < diff_count && (top_n == 0 || shown_count < top_n); k++) {
            if (diffs[k].delta <= 0) continue;
            print_size(diffs[k].delta, buf, sizeof(buf));
            print_to_both("  +%-9s %s (was %lld bytes, now %lld bytes)\n", buf, diffs[k].relpath, diffs[k].size_b, diffs[k].size_a);
            shown_count++;
        }
    }
    print_to_both("\n");

    print_to_both("=== Files present in both, but shrank (A < B) -- sorted by delta, biggest shrink first ===\n");
    print_to_both("(%lld bytes total shrinkage across changed files)\n", total_shrank);
    {
        DiffEntry *shrunk_sorted = malloc(diff_count * sizeof(DiffEntry));
        size_t sc = 0, k;
        if (!shrunk_sorted) { fprintf(stderr, "Out of memory\n"); return 1; }
        for (k = 0; k < diff_count; k++) {
            if (diffs[k].delta < 0) shrunk_sorted[sc++] = diffs[k];
        }
        qsort(shrunk_sorted, sc, sizeof(DiffEntry), cmp_diff_delta_asc);
        {
            size_t shown = (top_n == 0 || sc < top_n) ? sc : top_n;
            for (k = 0; k < shown; k++) {
                print_size(shrunk_sorted[k].delta, buf, sizeof(buf));
                print_to_both("  %-10s %s (was %lld bytes, now %lld bytes)\n", buf, shrunk_sorted[k].relpath, shrunk_sorted[k].size_b, shrunk_sorted[k].size_a);
            }
            if (shown < sc) print_to_both("  ... and %zu more\n", sc - shown);
        }
        free(shrunk_sorted);
    }
    print_to_both("\n");

    print_to_both("=== Files only in B (removed since B, i.e. not present in A) -- sorted by size, biggest first ===\n");
    print_to_both("(%zu files, %lld bytes total)\n", only_b.count, total_removed);
    {
        size_t shown = (top_n == 0 || only_b.count < top_n) ? only_b.count : top_n;
        size_t k;
        for (k = 0; k < shown; k++) {
            print_size(only_b.items[k].size, buf, sizeof(buf));
            print_to_both("  %-10s %s\n", buf, only_b.items[k].relpath);
        }
        if (shown < only_b.count) print_to_both("  ... and %zu more\n", only_b.count - shown);
    }

    if (output_file) {
        fflush(output_file);
        fclose(output_file);
        output_file = NULL;
        fprintf(stderr, "\n(also written to diff_dirs_outliers.txt)\n");
    }

    free(list_a.items);
    free(list_b.items);
    free(only_a.items);
    free(only_b.items);
    free(diffs);
    return 0;
}
