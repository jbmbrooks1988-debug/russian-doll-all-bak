/*
 * Example module demonstrating inline CHTML file loading
 * This module sends a command to load an external CHTML file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <time.h>
#include <errno.h>

// Define the same shared memory structure as in the main model
typedef struct {
    int module_id;
    int active;                    // Whether this module is active
    int command_ready;             // Flag: 1 if command is ready for module
    int response_ready;            // Flag: 1 if response is ready for main
    char command_buffer[4096];     // Command from main to module
    char response_buffer[4096];    // Response from module to main
    int last_command_timestamp;    // For detecting stale commands
    int last_response_timestamp;   // For detecting stale responses
    int sem_id;                    // Semaphore ID for synchronization
    // New fields for CHTML content via shared memory
    int chtml_ready;               // Flag: 1 if CHTML content is ready for main
    int chtml_content_size;        // Size of CHTML content
    char chtml_content[4096];      // CHTML content from module to main
} ModuleSharedMem;

// Generate a unique key for shared memory based on module index (same as in main)
int generate_shm_key(int module_idx) {
    char pathname[256];
    snprintf(pathname, sizeof(pathname), "/tmp/chtml_shmem_%d", module_idx);
    
    // Create the file if it doesn't exist
    FILE* temp_file = fopen(pathname, "w");
    if (temp_file) {
        fprintf(temp_file, "Shared memory for chtml module %d", module_idx);
        fclose(temp_file);
    }
    
    return ftok(pathname, 'R' + module_idx); // 'R' is arbitrary project identifier
}

int main(int argc, char** argv) {
    int module_idx = 0;
    
    if (argc > 1) {
        module_idx = atoi(argv[1]);
    }
    
    printf("Inline CHTML File Loader Module starting with module index: %d\n", module_idx);
    
    // Generate the same key that the main process would use
    key_t shm_key = generate_shm_key(module_idx);
    if (shm_key == -1) {
        perror("ftok for shared memory in module");
        exit(EXIT_FAILURE);
    }
    
    // Connect to the existing shared memory segment
    int shmem_id = shmget(shm_key, sizeof(ModuleSharedMem), 0666);
    if (shmem_id == -1) {
        perror("shmget in module - shared memory segment not found");
        printf("This module expects the main process to create the shared memory segment first.\n");
        exit(EXIT_FAILURE);
    }
    
    // Attach to the shared memory
    ModuleSharedMem* shmem_ptr = (ModuleSharedMem*)shmat(shmem_id, NULL, 0);
    if (shmem_ptr == (ModuleSharedMem*)-1) {
        perror("shmat in module");
        exit(EXIT_FAILURE);
    }
    
    printf("Module connected to shared memory segment with ID: %d\n", shmem_id);
    
    // Example: Send a command to load an external CHTML file
    const char* load_command = "LOAD_FILE:example.chtml:400:300";  // Load example.chtml in a 400x300 window
    
    printf("Sending load file command to main process: %s\n", load_command);
    
    // Wait for a moment to ensure main process is ready
    sleep(1);
    
    // Send the load command via shared memory
    int cmd_len = strlen(load_command);
    if (cmd_len >= sizeof(shmem_ptr->chtml_content)) {
        printf("Load command too long!\n");
        shmdt(shmem_ptr);
        exit(EXIT_FAILURE);
    }
    
    // Wait if previous CHTML content is still being processed
    int attempts = 0;
    while (shmem_ptr->chtml_ready && attempts < 100) {
        usleep(10000); // Sleep for 10ms
        attempts++;
    }
    
    if (shmem_ptr->chtml_ready) {
        printf("Warning: Previous CHTML content still pending, overwriting\n");
    }
    
    // Copy command to shared buffer
    strcpy(shmem_ptr->chtml_content, load_command);
    shmem_ptr->chtml_content_size = cmd_len;
    shmem_ptr->chtml_ready = 1;
    
    printf("Load command sent to main process successfully\n");
    
    // Wait for main process to process the command
    sleep(3);
    
    // Detach from shared memory
    if (shmdt(shmem_ptr) == -1) {
        perror("shmdt");
    }
    
    printf("Module finished successfully\n");
    
    return 0;
}