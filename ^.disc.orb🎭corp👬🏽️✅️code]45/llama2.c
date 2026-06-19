#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LLAMA2_RESULT_FILE "llama2_result.txt"
#define SEND_QUEUE_FILE "send_queue.txt"
#define KEYS_FILE "#.bot_keys.txt"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        return 1;
    }

    char *channel_id = argv[1];
    char *message = argv[2];

    char command[4096];
    snprintf(command, sizeof(command), "'../llama2.c-master]slim]a0/run' '../llama2.c-master]slim]a0/model.bin' -z '../llama2.c-master]slim]a0/tokenizer.bin' -i '%s' > %s", message, LLAMA2_RESULT_FILE);
    system(command);

    FILE *result_file = fopen(LLAMA2_RESULT_FILE, "r");
    if (result_file) {
        char result[4096];
        size_t len = fread(result, 1, sizeof(result) - 1, result_file);
        result[len] = '\0';
        fclose(result_file);

        FILE *send_queue = fopen(SEND_QUEUE_FILE, "w");
        if (send_queue) {
            fprintf(send_queue, "%s|%s", channel_id, result);
            fclose(send_queue);
            
            system("./+x/send.+x");
        }
    }

    return 0;
}
