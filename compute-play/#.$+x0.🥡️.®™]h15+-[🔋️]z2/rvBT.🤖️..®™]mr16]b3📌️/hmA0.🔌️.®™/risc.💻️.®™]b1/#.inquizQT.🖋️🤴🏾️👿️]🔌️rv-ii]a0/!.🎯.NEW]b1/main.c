#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 512
#define MAX_SIGNALS 32

// Process NAND gate: C = !(A & B)
void process_nand(int a, int b, int* c) {
    *c = !(a && b);
}

// Read inputs from tmp/input.txt
int read_inputs(const char* input_file, char signals[][MAX_LINE], int values[], int* count) {
    FILE* fp = fopen(input_file, "r");
    if (!fp) {
        printf("ERROR: Failed to open %s\n", input_file);
        return 1;
    }
    *count = 0;
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, fp) && *count < MAX_SIGNALS) {
        line[strcspn(line, "\n")] = '\0';
        char* eq = strchr(line, '=');
        if (!eq) {
            printf("ERROR: Invalid input line: %s\n", line);
            fclose(fp);
            return 1;
        }
        *eq = '\0';
        strncpy(signals[*count], line, MAX_LINE - 1);
        signals[*count][MAX_LINE - 1] = '\0';
        values[*count] = atoi(eq + 1);
        printf("DEBUG: Read %s=%d\n", signals[*count], values[*count]);
        (*count)++;
    }
    fclose(fp);
    return 0;
}

// Write output to tmp/output.txt
int write_output(const char* output_file, const char* signal, int value) {
    FILE* fp = fopen(output_file, "w");
    if (!fp) {
        printf("ERROR: Failed to open %s\n", output_file);
        return 1;
    }
    fprintf(fp, "%s=%d\n", signal, value);
    printf("DEBUG: Wrote %s=%d to %s\n", signal, value, output_file);
    fclose(fp);
    return 0;
}

// Process one NAND gate from netlist
int process_netlist(const char* netlist_file, char input_signals[][MAX_LINE], int input_values[], int input_count, const char* output_file) {
    FILE* fp = fopen(netlist_file, "r");
    if (!fp) {
        printf("ERROR: Failed to open %s\n", netlist_file);
        return 1;
    }
    char line[MAX_LINE];
    if (!fgets(line, MAX_LINE, fp)) {
        printf("ERROR: Empty netlist\n");
        fclose(fp);
        return 1;
    }
    fclose(fp);
    line[strcspn(line, "\n")] = '\0';

    char in1[MAX_LINE], in2[MAX_LINE], out[MAX_LINE];
    if (sscanf(line, "./+x/nand.+x %s %s %s", in1, in2, out) != 3) {
        printf("ERROR: Invalid netlist line: %s\n", line);
        return 1;
    }

    int a = -1, b = -1;
    for (int i = 0; i < input_count; i++) {
        if (strcmp(input_signals[i], in1) == 0) a = input_values[i];
        if (strcmp(input_signals[i], in2) == 0) b = input_values[i];
    }
    if (a == -1 || b == -1) {
        printf("ERROR: Unknown input signal: %s or %s\n", in1, in2);
        return 1;
    }

    int result;
    process_nand(a, b, &result);
    return write_output(output_file, out, result);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <netlist_file> <output_file>\n", argv[0]);
        return 1;
    }

    const char* netlist_file = argv[1];
    const char* output_file = argv[2];

    char input_signals[MAX_SIGNALS][MAX_LINE];
    int input_values[MAX_SIGNALS];
    int input_count;
    if (read_inputs("tmp/input.txt", input_signals, input_values, &input_count)) {
        printf("ERROR: Failed to read inputs\n");
        return 1;
    }

    if (process_netlist(netlist_file, input_signals, input_values, input_count, output_file)) {
        printf("ERROR: Failed to process netlist\n");
        return 1;
    }

    return 0;
}
