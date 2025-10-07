#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

// Maximum number of directories (adjust as needed)
#define MAX_DIRS 10000
// Maximum length of directory name
#define MAX_NAME 256
// Maximum length of a matrix row (name + 3 integers)
#define MAX_ROW (MAX_NAME + 3 * 20)

int dir_count = 0;
// Matrix: [name, level, index_in_level, parent_index]
char matrix[MAX_DIRS][MAX_ROW];
// Track indices for each level
int level_indices[100] = {0};

void add_to_matrix(const char *name, int level, int parent_index) {
    if (dir_count >= MAX_DIRS) {
        fprintf(stderr, "Error: Maximum directory limit reached\n");
        return;
    }
    // Format row: name,level,index_in_level,parent_index
    snprintf(matrix[dir_count], MAX_ROW, "%s,%d,%d,%d", 
             name, level, level_indices[level], parent_index);
    level_indices[level]++;
    dir_count++;
}

void walk_dirs(const char *path, int level, int parent_index) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Warning: Cannot open %s: %s\n", path, strerror(errno));
        return;
    }

    // Get directory name (basename)
    char *name = strrchr(path, '/');
    name = name ? name + 1 : (char *)path;
    if (strlen(name) == 0) name = path;

    // Add current directory to matrix
    add_to_matrix(name, level, parent_index);

    int current_index = dir_count - 1;
    struct dirent *entry;
    char **subdirs = malloc(MAX_DIRS * sizeof(char *));
    int subdir_count = 0;

    // Collect subdirectories
    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_DIR || strcmp(entry->d_name, ".") == 0 || 
            strcmp(entry->d_name, "..") == 0) continue;

        subdirs[subdir_count] = strdup(entry->d_name);
        subdir_count++;
    }
    closedir(dir);

    // Sort subdirectories for consistent output
    for (int i = 0; i < subdir_count - 1; i++) {
        for (int j = i + 1; j < subdir_count; j++) {
            if (strcmp(subdirs[i], subdirs[j]) > 0) {
                char *temp = subdirs[i];
                subdirs[i] = subdirs[j];
                subdirs[j] = temp;
            }
        }
    }

    // Process subdirectories
    for (int i = 0; i < subdir_count; i++) {
        char subpath[1024];
        snprintf(subpath, sizeof(subpath), "%s/%s", path, subdirs[i]);
        walk_dirs(subpath, level + 1, current_index);
        free(subdirs[i]);
    }
    free(subdirs);
}

void save_to_csv(const char *output_file) {
    FILE *fp = fopen(output_file, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open %s: %s\n", output_file, strerror(errno));
        return;
    }
    fprintf(fp, "Directory_Name,Level,Index_In_Level,Parent_Index\n");
    for (int i = 0; i < dir_count; i++) {
        fprintf(fp, "%s\n", matrix[i]);
    }
    fclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <directory> [output.csv]\n", argv[0]);
        return 1;
    }

    // Check if directory exists
    struct stat st;
    if (stat(argv[1], &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a valid directory\n", argv[1]);
        return 1;
    }

    // Default output file
    const char *output_file = (argc >= 3) ? argv[2] : "dir_tree.csv";

    walk_dirs(argv[1], 0, -1);
    save_to_csv(output_file);
    printf("Directory tree saved to %s\n", output_file);

    return 0;
}
