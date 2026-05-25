// 8-bit Instruction Decoder
// Decodes 16-bit instructions for the 8-bit RISC-V processor
module instruction_decoder (
    input [15:0] instruction,
    output [6:0] opcode,      // 7-bit opcode
    output [2:0] funct3,      // 3-bit funct3 field
    output [6:0] funct7,      // 7-bit funct7 field
    output [2:0] rs1,         // 3-bit source register 1
    output [2:0] rs2,         // 3-bit source register 2
    output [2:0] rd,          // 3-bit destination register
    output [11:0] immediate,  // 12-bit immediate value
    output reg_write,
    output mem_write,
    output mem_read,
    output [2:0] alu_op       // 3-bit ALU operation
);

    // Instruction format extraction
    assign opcode = instruction[6:0];      // bits [6:0]
    assign rs1 = instruction[19:17];       // bits [19:17]
    assign rs2 = instruction[24:20];       // bits [24:20] 
    assign rd = instruction[11:7];         // bits [11:7]
    assign funct3 = instruction[14:12];    // bits [14:12]
    assign funct7 = instruction[31:25];    // bits [31:25] (for 16-bit, this will be part of immediate)

    // Immediate value extraction (varies by instruction type)
    // For I-type: bits [11:0]
    // For S-type: bits [11:5] and [4:0] combined
    // For B-type: bits [12] + [10:5] + [4:1] + [11]
    // For U-type: bits [31:12] (for 16-bit, this will be different)
    // For J-type: bits [20] + [10:1] + [11] + [19:12]
    assign immediate = {
        instruction[15], instruction[14:12], instruction[11:8], instruction[7]
    }; // Simplified immediate for 16-bit instruction

    // Decode control signals based on opcode
    always @(*) begin
        case (opcode)
            7'b0110011: begin  // R-type
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                case (funct3)
                    3'b000: begin  // ADD, SUB
                        if (instruction[30] == 1'b1)  // funct7[5] check for SUB
                            alu_op = 3'b001;  // SUB
                        else
                            alu_op = 3'b000;  // ADD
                    end
                    3'b001: alu_op = 3'b101;  // SLL
                    3'b010: alu_op = 3'b111;  // SLT
                    3'b011: alu_op = 3'b111;  // SLTU (unsigned less than)
                    3'b100: alu_op = 3'b100;  // XOR
                    3'b101: begin  // SRL, SRA
                        if (instruction[30] == 1'b1)  // funct7[5] check for SRA
                            alu_op = 3'b110;  // SRL (we'll treat SRA same as SRL for simplicity)
                        else
                            alu_op = 3'b110;  // SRL
                    end
                    3'b110: alu_op = 3'b011;  // OR
                    3'b111: alu_op = 3'b010;  // AND
                    default: alu_op = 3'b000;  // Default to ADD
                endcase
            end
            7'b0010011: begin  // I-type (ADDI, ANDI, ORI, XORI, SLLI, SRLI)
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                case (funct3)
                    3'b000: alu_op = 3'b000;  // ADDI
                    3'b010: alu_op = 3'b111;  // SLTI
                    3'b100: alu_op = 3'b100;  // XORI
                    3'b110: alu_op = 3'b011;  // ORI
                    3'b111: alu_op = 3'b010;  // ANDI
                    3'b001: alu_op = 3'b101;  // SLLI
                    3'b101: alu_op = 3'b110;  // SRLI
                    default: alu_op = 3'b000;  // Default to ADDI
                endcase
            end
            7'b0000011: begin  // I-type (LW, LH, LB)
                reg_write = 1;
                mem_write = 0;
                mem_read = 1;
                alu_op = 3'b000;  // ADD for address calculation
            end
            7'b0100011: begin  // S-type (SW, SH, SB)
                reg_write = 0;
                mem_write = 1;
                mem_read = 0;
                alu_op = 3'b000;  // ADD for address calculation
            end
            7'b1100011: begin  // B-type (BEQ, BNE, BLT, BGE, BLTU, BGEU)
                reg_write = 0;
                mem_write = 0;
                mem_read = 0;
                alu_op = 3'b001;  // SUB for comparison
            end
            7'b1101111: begin  // J-type (JAL)
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                alu_op = 3'b000;  // ADD to increment PC
            end
            7'b1100111: begin  // I-type (JALR)
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                alu_op = 3'b000;  // ADD for address calculation
            end
            7'b0001111: begin  // FENCE
                reg_write = 0;
                mem_write = 0;
                mem_read = 0;
                alu_op = 3'b000;
            end
            7'b1110011: begin  // SYSTEM (ECALL, EBREAK, CSR instructions)
                reg_write = 0;
                mem_write = 0;
                mem_read = 0;
                alu_op = 3'b000;
            end
            default: begin  // Unknown opcode
                reg_write = 0;
                mem_write = 0;
                mem_read = 0;
                alu_op = 3'b000;
            end
        endcase
    end

endmodule