/* mr_show_text - "Show Text" event command
 * Displays a message to the player, optionally with a speaker name.
 *
 * Usage: mr_show_text.+x <package_dir> <message_text> [speaker_name]
 *   message_text: the text to display
 *   speaker_name: optional, name of who is speaking (defaults to empty)
 *
 * Outputs message to game UI via a messages queue file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PATH_BUF 4352

static void write_message(const char *package_dir, const char *speaker, const char *text) {
    char msg_path[PATH_BUF];
    snprintf(msg_path, sizeof(msg_path), "%s/messages.txt", package_dir);

    FILE *mf = fopen(msg_path, "a");
    if (!mf) {
        fprintf(stderr, "Could not open messages.txt for writing\n");
        return;
    }

    time_t now = time(NULL);
    fprintf(mf, "[%ld] SHOW_TEXT", (long)now);
    if (speaker && speaker[0]) {
        fprintf(mf, " speaker=%s", speaker);
    }
    fprintf(mf, " text=%s\n", text);
    fclose(mf);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: mr_show_text.+x <package_dir> <message_text> [speaker_name]\n");
        return 1;
    }
    const char *package_dir = argv[1];
    const char *message_text = argv[2];
    const char *speaker_name = (argc > 3) ? argv[3] : "";

    write_message(package_dir, speaker_name, message_text);

    char hist_path[PATH_BUF];
    snprintf(hist_path, sizeof(hist_path), "%s/history.txt", package_dir);
    FILE *hf = fopen(hist_path, "a");
    if (hf) {
        if (speaker_name && speaker_name[0]) {
            fprintf(hf, "SHOW_TEXT speaker=%s text=%s\n", speaker_name, message_text);
        } else {
            fprintf(hf, "SHOW_TEXT text=%s\n", message_text);
        }
        fclose(hf);
    }

    printf("SHOW_TEXT");
    if (speaker_name && speaker_name[0]) {
        printf(" [%s]", speaker_name);
    }
    printf(": %s\n", message_text);
    return 0;
}
