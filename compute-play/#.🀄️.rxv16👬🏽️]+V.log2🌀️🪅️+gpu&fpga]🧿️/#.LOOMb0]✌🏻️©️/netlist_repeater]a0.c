#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 400 // Max 100 instructions * 4 lines each
#define LINE_LENGTH 17 // 16 bits + newline

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s chip_file.txt start_address repeat_count\n", argv[0]);
        return 1;
    }

    const char *chip_file = argv[1];
    int start_address = atoi(argv[2]);
    int repeat_count = atoi(argv[3]);

    if (start_address < 0 || start_address > 255 || repeat_count < 1) {
        printf("Error: Invalid start_address or repeat_count\n");
        return 1;
    }

    // Read chip file
    char lines[MAX_LINES][LINE_LENGTH];
    int line_count = 0;
    FILE *fp = fopen(chip_file, "r");
    if (!fp) {
        printf("Error opening %s\n", chip_file);
        return 1;
    }
    while (fgets(lines[line_count], LINE_LENGTH, fp) && line_count < MAX_LINES) {
        lines[line_count][strcspn(lines[line_count], "\n")] = '\0';
        if (strlen(lines[line_count]) == 16) line_count++;
    }
    fclose(fp);

    // Validate: Must be multiple of 4 (complete instructions)
    if (line_count % 4 != 0) {
        printf("Error: Chip file must have multiple of 4 lines\n");
        return 1;
    }

    // Generate output
    FILE *out_fp = fopen("program.txt", "w");
    if (!out_fp) {
        printf("Error opening program.txt\n");
        return 1;
    }

    for (int r = 0; r < repeat_count; r++) {
        for (int i = 0; i < line_count; i++) {
            if (i % 4 == 1) { // RAM output address field
                unsigned short addr = strtoul(lines[i], NULL, 2);
                addr += r; // Increment address for each repetition
                if (addr > 255) {
                    printf("Warning: Address %d exceeds 255, wrapping\n", addr);
                    addr %= 256;
                }
                char new_line[17];
                snprintf(new_line, 17, "%016d", addr); // Convert to 16-bit binary
                for (int j = 0; j < 16; j++) if (new_line[j] != '0') new_line[j] = '1';
                fprintf(out_fp, "%s\n", new_line);
            } else if (i % 4 == 2 || i % 4 == 3) { // Input A or B fields
                unsigned short input = strtoul(lines[i], NULL, 2);
                if (input > 15) { // RAM input address
                    input += r; // Increment RAM address
                    if (input > 255) input %= 256;
                    char new_line[17];
                    snprintf(new_line, 17, "%016d", input);
                    for (int j = 0; j < 16; j++) if (new_line[j] != '0') new_line[j] = '1';
                    fprintf(out_fp, "%s\n", new_line);
                } else {
                    fprintf(out_fp, "%s\n", lines[i]); // Copy unchanged
                }
            } else {
                fprintf(out_fp, "%s\n", lines[i]); // Copy chip_location
            }
        }
    }

    fclose(out_fp);
    printf("Generated program.txt with %d instructions\n", (line_count / 4) * repeat_count);
    return 0;
}
