#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_csv_file> <output_csv_file>\n", argv[0]);
        return 1;
    }

    const char* input_file = argv[1];
    const char* output_file = argv[2];

    FILE* in_fp = fopen(input_file, "r");
    if (!in_fp) {
        perror("Error opening input CSV file");
        return 1;
    }

    int number = 0;
    if (fscanf(in_fp, "%d", &number) != 1) {
        fprintf(stderr, "Error reading number from input CSV file\n");
        fclose(in_fp);
        return 1;
    }
    fclose(in_fp);

    // Perform a simple operation
    number++;

    FILE* out_fp = fopen(output_file, "w");
    if (!out_fp) {
        perror("Error opening output CSV file");
        return 1;
    }

    fprintf(out_fp, "%d\n", number);
    fclose(out_fp);

    return 0;
}
