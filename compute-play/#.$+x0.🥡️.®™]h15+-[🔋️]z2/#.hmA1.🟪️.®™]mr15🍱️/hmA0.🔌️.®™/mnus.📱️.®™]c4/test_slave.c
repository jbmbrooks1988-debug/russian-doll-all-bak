#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#define TIMEOUT_SECONDS 180 // 3 minutes
#define LOG_FILE "test_slave.txt"

int main() {
    // Redirect I/O to /dev/null to prevent interference
    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null == -1) {
        exit(1);
    }
    dup2(dev_null, STDIN_FILENO);
    dup2(dev_null, STDOUT_FILENO);
    dup2(dev_null, STDERR_FILENO);
    close(dev_null);

    // Open log file
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (!log_fp) {
        exit(1);
    }

    time_t start_time = time(NULL);
    int count = 0;

    // Count until killed or timeout
    while (time(NULL) - start_time < TIMEOUT_SECONDS) {
        // Get current timestamp
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

        // Write count and timestamp to log file
        fprintf(log_fp, "[%s] Count: %d\n", timestamp, count);
        fflush(log_fp);
        count++;

        // Sleep for 1 second to prevent excessive CPU usage
        sleep(1);
    }

    // Log completion or timeout
    time_t end_time = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&end_time));
    fprintf(log_fp, "[%s] Test slave completed or timed out\n", timestamp);
    fclose(log_fp);

    return 0;
}
