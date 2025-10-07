#include <stdio.h>
#include <stdint.h>

// Define constants for RV-I
#define NUM_REGS 4 // 4 1-bit registers (r0-r3)
#define MEM_SIZE 16 // 16 1-bit memory locations (mem[0-15])
#define PC_SIZE 4 // 4-bit program counter (0-15)

// CPU State: Arrays for registers, memory, and PC
int8_t registers[NUM_REGS]; // 1-bit registers (0 or 1)
int8_t memory[MEM_SIZE]; // 1-bit memory
int8_t pc = 0; // Program counter (4-bit, 0-15)

// Instruction format: Simple array for program (each instruction is 4 bytes for simplicity)
uint8_t program[] = {
    // Example program: NAND r1, r2 -> r1, then LOAD r1, mem[0]
    0x01, 0x01, 0x02, 0x00, // NAND r1, r2 -> r1 (opcode=1, rd=1, rs1=1, rs2=2)
    0x02, 0x01, 0x00, 0x04, // LOAD r1, mem[0] (opcode=2, rd=1, addr=4, unused=0)
    0xFF, 0xFF, 0xFF, 0xFF // End marker
};

// Execute one instruction
void execute_instruction() {
    // Fetch instruction (first 4 bytes at pc)
    uint8_t opcode = program[pc];
    uint8_t rd = program[pc + 1];
    uint8_t rs1 = program[pc + 2];
    uint8_t rs2 = program[pc + 3];

    // Check for end marker
    if (opcode == 0xFF) {
        printf("Program ended.\n");
        return;
    }

    // Execute based on opcode
    switch (opcode) {
        case 1: // NAND: r[rd] = ~(r[rs1] & r[rs2])
            registers[rd] = !(registers[rs1] & registers[rs2]);
            printf("NAND r%d, r%d -> r%d = %d\n", rs1, rs2, rd, registers[rd]);
            break;
        case 2: // LOAD: r[rd] = mem[rs1] (rs2 is address offset, here 4 for mem[0])
            registers[rd] = memory[rs1]; // Assuming rs1 is the memory address (0-15)
            printf("LOAD mem[%d] -> r%d = %d\n", rs1, rd, registers[rd]);
            break;
        case 3: // STORE: mem[rs1] = r[rd] (rs2 unused)
            memory[rs1] = registers[rd];
            printf("STORE r%d -> mem[%d] = %d\n", rd, rs1, memory[rs1]);
            break;
        case 4: // JUMP: pc = r[rs1] * 2 (simplified, assuming rs1 holds a valid PC)
            pc = (registers[rs1] * 2) & 0xF; // 4-bit PC, wrap around
            printf("JUMP to PC = %d\n", pc);
            return; // Don't increment PC, JUMP sets it
        default:
            printf("Unknown opcode: %d at PC=%d\n", opcode, pc);
    }
    pc += 4; // Move to next instruction (4 bytes)
}

int main() {
    // Initialize registers and memory
    for (int i = 0; i < NUM_REGS; i++) {
        registers[i] = 0; // All registers start at 0
    }
    for (int i = 0; i < MEM_SIZE; i++) {
        memory[i] = 0; // All memory starts at 0
    }

    // Set initial values for demonstration
    registers[1] = 1; // r1 = 1
    registers[2] = 0; // r2 = 0
    memory[0] = 1; // mem[0] = 1 for LOAD test

    // Main execution loop
    printf("Starting RV-I Emulator...\n");
    while (pc < sizeof(program) && pc >= 0) {
        execute_instruction();
    }

    // Print final state
    printf("\nFinal State:\n");
    printf("Registers: ");
    for (int i = 0; i < NUM_REGS; i++) {
        printf("r%d=%d ", i, registers[i]);
    }
    printf("\nMemory: ");
    for (int i = 0; i < MEM_SIZE; i++) {
        printf("mem[%d]=%d ", i, memory[i]);
    }
    printf("\n");

    return 0;
}
