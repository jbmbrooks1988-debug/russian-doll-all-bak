#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Parse Verilog file and generate HDLb0 instructions
void convert_verilog_to_hdlb0(const char *verilog_file, const char *output_file) {
    FILE *in = fopen(verilog_file, "r");
    FILE *out = fopen(output_file, "w");
    if (!in || !out) {
        printf("Error opening files\n");
        if (in) fclose(in);
        if (out) fclose(out);
        return;
    }

    char line[256];
    int input_a = 5, input_b = 6, output_addr = 16; // Default: switches 5,6, RAM[16]
    int found_nand = 0;

    // Simple parsing: Look for NAND gate module
    while (fgets(line, sizeof(line), in)) {
        // Remove whitespace and comments
        line[strcspn(line, "\n")] = 0;
        if (strstr(line, "//") || line[0] == 0) continue;

        // Detect module (e.g., "module nand_gate(a, b, out)")
        if (strstr(line, "module") && strstr(line, "nand_gate")) {
            found_nand = 1;
            // Extract inputs/output (simplified, assumes a, b, out)
            char *inputs = strstr(line, "(");
            if (inputs) {
                // Assume inputs are switches, output to RAM[16]
                input_a = 5; // switch_0
                input_b = 6; // switch_1
                output_addr = 16; // RAM[16]
            }
        }

        // Detect transistor-level NAND (simplified: look for pmos/nmos patterns)
        if (found_nand && (strstr(line, "pmos") || strstr(line, "nmos"))) {
            // Transistor-level NAND: a & b -> ~out
            // HDLb0: chip_location=1 (NAND), output to RAM[16], inputs from switches
            fprintf(out, "0000000000000001 %016u %016u %016u # NAND switch_0(%d), switch_1(%d) -> RAM[%d]\n",
                    output_addr, input_a, input_b, input_a, input_b, output_addr);
            fprintf(out, "0000000000000000 %000000000000000 %016u %0000000000000011 # Pass RAM[%d] to tape\n",
                    output_addr, output_addr);
        }
    }

    if (!found_nand) {
        printf("No NAND gate found in Verilog\n");
    }

    fclose(in);
    fclose(out);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s input.v output.txt\n", argv[0]);
        return 1;
    }
    convert_verilog_to_hdlb0(argv[1], argv[2]);
    printf("Converted %s to %s\n", argv[1], argv[2]);
    return 0;
}