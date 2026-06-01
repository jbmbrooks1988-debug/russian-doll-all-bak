/*
 * Example module demonstrating inline CHTML functionality via shared memory
 * This module will send CHTML content to the main process to be rendered dynamically
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
    
    printf("Inline CHTML Example Module starting with module index: %d\n", module_idx);
    
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
    
    // Example: Send CHTML content to the main process
    const char* example_chtml = "<button x=\"10\" y=\"10\" width=\"100\" height=\"30\" label=\"Dynamic Button\" color=\"#FF0000\" onClick=\"button_clicked\"/>";
    
    printf("Sending CHTML content to main process:\n%s\n", example_chtml);
    
    // Wait for a moment to ensure main process is ready
    sleep(1);
    
    // Send the CHTML content to the main process via shared memory
    int chtml_len = strlen(example_chtml);
    if (chtml_len >= sizeof(shmem_ptr->chtml_content)) {
        printf("CHTML content too long!\n");
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
    
    // Copy CHTML content to shared buffer
    strcpy(shmem_ptr->chtml_content, example_chtml);
    shmem_ptr->chtml_content_size = chtml_len;
    shmem_ptr->chtml_ready = 1;
    
    printf("CHTML content sent to main process successfully\n");
    
    // Wait for main process to process the CHTML
    sleep(2);
    
    // Send another CHTML content after a delay
    const char* second_chtml = "<panel x=\"10\" y=\"50\" width=\"200\" height=\"100\" color=\"#00FF00\" alpha=\"0.5\"><text x=\"10\" y=\"10\" label=\"Dynamic Panel\"/></panel>";
    
    printf("\nSending second CHTML content to main process:\n%s\n", second_chtml);
    
    // Wait again if previous content is still being processed
    attempts = 0;
    while (shmem_ptr->chtml_ready && attempts < 100) {
        usleep(10000); // Sleep for 10ms
        attempts++;
    }
    
    if (shmem_ptr->chtml_ready) {
        printf("Warning: Previous CHTML content still pending, overwriting\n");
    }
    
    // Copy second CHTML content to shared buffer
    chtml_len = strlen(second_chtml);
    strcpy(shmem_ptr->chtml_content, second_chtml);
    shmem_ptr->chtml_content_size = chtml_len;
    shmem_ptr->chtml_ready = 1;
    
    printf("Second CHTML content sent to main process successfully\n");
    
    // Wait for main process to process the second CHTML
    sleep(2);
    
    // Detach from shared memory
    if (shmdt(shmem_ptr) == -1) {
        perror("shmdt");
    }
    
    printf("Module finished successfully\n");
    
    return 0;
}