// audio.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void start_music() {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("sox", "sox", "-n", "-d", "synth", "0.2", "sine", "C4", "delay", "0.2", "0.4", "remix", "v1,v2", "gain", "-10", "loop", "1000", NULL);
        exit(0);
    }
}

void play_sfx(const char* freq) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("sox", "sox", "-n", "-d", "synth", "0.1", "sine", freq, "gain", "-3", NULL);
        exit(0);
    }
}
