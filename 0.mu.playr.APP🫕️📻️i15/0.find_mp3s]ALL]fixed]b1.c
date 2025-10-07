#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ftw.h>

FILE *playlist_file;

// This function is called for each file found by nftw().
static int find_mp3(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    // Check if it's a file and the name ends with .mp3
    if (typeflag == FTW_F) {
        // Get the file name (not the full path) to check for hidden files
        const char *filename = strrchr(fpath, '/');
        filename = (filename != NULL) ? filename + 1 : fpath; // Skip the '/' or use full path if no '/'

        // Skip files starting with "._" (macOS metadata files)
        if (strncmp(filename, "._", 2) == 0) {
            return 0; // Continue traversal, skip this file
        }

        // Check for .mp3 extension
        const char *ext = strrchr(fpath, '.');
        if (ext && strcmp(ext, ".mp3") == 0) {
            fprintf(playlist_file, "%s\n", fpath);
            printf("Found: %s\n", fpath);
        }
    }
    return 0; // Continue traversal
}

int main() {
    printf("Searching for .mp3 files...\n");

    playlist_file = fopen("playlist.txt", "w");
    if (playlist_file == NULL) {
        perror("Failed to open playlist.txt for writing");
        return 1;
    }

    // Start the file tree walk from the root directory "/"
    // FTW_PHYS tells nftw not to follow symbolic links.
    if (nftw("/", find_mp3, 20, FTW_PHYS) == -1) {
        perror("nftw failed");
        fclose(playlist_file);
        return 1;
    }

    printf("Search complete. Playlist saved to playlist.txt\n");
    fclose(playlist_file);
    return 0;
}
