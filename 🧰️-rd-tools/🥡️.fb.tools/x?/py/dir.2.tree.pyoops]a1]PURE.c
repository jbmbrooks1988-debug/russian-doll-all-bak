#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <locale.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>

// Maximum number of directories (adjust as needed)
#define MAX_DIRS 10000
// Maximum length of directory name (increased for emoji support)
#define MAX_NAME 1024
// Maximum length of a matrix row
#define MAX_ROW (MAX_NAME + 64)

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
    // Escape commas in directory names to prevent CSV issues
    char escaped_name[MAX_NAME * 2];
    int j = 0;
    for (int i = 0; name[i] && j < MAX_NAME * 2 - 1; i++) {
        if (name[i] == ',') {
            escaped_name[j++] = '\\';
            escaped_name[j++] = ',';
        } else {
            escaped_name[j++] = name[i];
        }
    }
    escaped_name[j] = '\0';
    // Format row: name,level,index_in_level,parent_index
    snprintf(matrix[dir_count], MAX_ROW, "\"%s\",%d,%d,%d", 
             escaped_name, level, level_indices[level], parent_index);
    level_indices[level]++;
    dir_count++;
}

void walk_dirs(const char *path, const char *root_name, int level, int parent_index) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Warning: Cannot open %s: %s\n", path, strerror(errno));
        return;
    }

    // Use provided root_name for level 0, otherwise extract basename
    const char *name = (level == 0) ? root_name : (strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
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
        char subpath[2048];
        snprintf(subpath, sizeof(subpath), "%s/%s", path, subdirs[i]);
        walk_dirs(subpath, root_name, level + 1, current_index);
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
    // Write UTF-8 BOM for better emoji support in CSV
    fprintf(fp, "\xEF\xBB\xBF");
    fprintf(fp, "Directory_Name,Level,Index_In_Level,Parent_Index\n");
    for (int i = 0; i < dir_count; i++) {
        fprintf(fp, "%s\n", matrix[i]);
    }
    fclose(fp);
}

int main(int argc, char *argv[]) {
    // Set locale to support UTF-8 for emoji handling
    if (setlocale(LC_ALL, "en_US.UTF-8") == NULL) {
        fprintf(stderr, "Warning: Failed to set UTF-8 locale\n");
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <directory> [output.csv]\n", argv[0]);
        return 1;
    }

    // Resolve absolute path
    char abs_path[PATH_MAX];
    if (realpath(argv[1], abs_path) == NULL) {
        fprintf(stderr, "Error: Cannot resolve path '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    // Check if directory exists
    struct stat st;
    if (stat(abs_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a valid directory\n", abs_path);
        return 1;
    }

    // Get main directory name for root
    char *main_dir = basename(abs_path);
    printf("Processing main directory: %s\n", main_dir);

    // Default output file
    const char *output_file = (argc >= 3) ? argv[2] : "dir_tree.csv";

    walk_dirs(abs_path, main_dir, 0, -1);
    save_to_csv(output_file);
    printf("Directory tree saved to %s\n", output_file);

    return 0;
}
