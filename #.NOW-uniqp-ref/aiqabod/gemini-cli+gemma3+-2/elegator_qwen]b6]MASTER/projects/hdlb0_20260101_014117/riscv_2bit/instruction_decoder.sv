// Instruction Decoder for 2-bit RISC-V processor
// Decodes 4-bit instructions based on a simplified RISC-V specification
// Format: [3:2] opcode | [1:0] operands/data
module instruction_decoder (
    input [3:0] instruction,
    output [1:0] opcode,
    output [1:0] rs1,
    output [1:0] rs2,
    output [1:0] rd,
    output [1:0] funct3,
    output [1:0] immediate,
    output reg_write,
    output mem_write,
    output mem_read,
    output [1:0] alu_op
);

    // Extract fields based on instruction format
    assign opcode = instruction[3:2];      // [3:2] opcode
    assign immediate = instruction[1:0];   // [1:0] immediate (for I-type)

    // For this simplified implementation, we'll use a fixed register addressing scheme
    // For R-type: destination register is specified in lower 2 bits
    // For I-type: destination register is specified in lower 2 bits
    // For memory: operation type is specified in lower 2 bits

    // Default assignments
    assign funct3 = 2'b00;  // Not used in this simplified implementation

    // Decode register addresses based on instruction type
    always @(*) begin
        // Default values
        rs1 = 2'b00;  // Default to R0
        rs2 = 2'b00;  // Default to R0
        rd = instruction[1:0];  // Use lower 2 bits as destination register for most instructions
        reg_write = 1'b0;
        mem_write = 1'b0;
        mem_read = 1'b0;
        alu_op = 2'b00;  // Default to ADD

        case (opcode)
            2'b00: begin  // R-type instructions (ADD)
                reg_write = 1'b1;
                // For ADD R[rd], R0, R1 (simplified - always use R0 and R1 as sources)
                rs1 = 2'b00;  // Source 1 is always R0
                rs2 = 2'b01;  // Source 2 is always R1
                rd = instruction[1:0];  // Destination register
                alu_op = 2'b00;  // ADD
            end
            2'b01: begin  // R-type instructions (AND)
                reg_write = 1'b1;
                // For AND R[rd], R0, R1 (simplified - always use R0 and R1 as sources)
                rs1 = 2'b00;  // Source 1 is always R0
                rs2 = 2'b01;  // Source 2 is always R1
                rd = instruction[1:0];  // Destination register
                alu_op = 2'b10;  // AND
            end
            2'b10: begin  // I-type instruction (ADDI)
                reg_write = 1'b1;
                // For ADDI R[rd], R0, immediate (destination in lower bits, source is R0)
                rs1 = 2'b00;  // Source register is R0
                rd = instruction[1:0];  // Destination register
                alu_op = 2'b00;  // ADD for immediate
            end
            2'b11: begin  // Memory operations (LW, SW)
                case (instruction[1:0])
                    2'b00: begin  // LW (Load Word)
                        reg_write = 1'b1;
                        mem_read = 1'b1;
                        // For LW R[rd], 0(R0) (destination in lower bits, address source is R0)
                        rs1 = 2'b00;  // Address register is R0
                        rd = 2'b10;  // Destination register is R2 (fixed for this example)
                        alu_op = 2'b00;  // ADD for address calculation
                    end
                    2'b01: begin  // SW (Store Word)
                        mem_write = 1'b1;
                        // For SW R1, 0(R0) (source is R1, address source is R0)
                        rs1 = 2'b00;  // Address register is R0
                        rs2 = 2'b01;  // Data register is R1
                        alu_op = 2'b00;  // ADD for address calculation
                    end
                    default: begin  // Default to LW
                        reg_write = 1'b1;
                        mem_read = 1'b1;
                        rs1 = 2'b00;  // Address register is R0
                        rd = 2'b10;  // Destination register is R2
                        alu_op = 2'b00;  // ADD for address calculation
                    end
                endcase
            end
        endcase
    end

endmodule