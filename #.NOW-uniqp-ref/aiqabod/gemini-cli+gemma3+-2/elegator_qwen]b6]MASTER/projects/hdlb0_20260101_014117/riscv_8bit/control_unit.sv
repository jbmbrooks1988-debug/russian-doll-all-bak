// 8-bit Control Unit
// Generates control signals based on opcode for the 8-bit RISC-V processor
module control_unit (
    input [6:0] opcode,
    output reg reg_write,
    output reg mem_write,
    output reg mem_read,
    output reg branch,
    output reg jump,
    output reg pc_src,      // 0 for PC+4, 1 for branch/jump target
    output [2:0] alu_op,    // 3-bit ALU operation
    output [1:0] alu_src,   // 0 for register, 1 for immediate, 2 for PC, 3 for others
    output [1:0] mem_to_reg // 0 for ALU result, 1 for memory data
);

    // Assign ALU operation based on opcode and funct3
    // This is simplified - in a real implementation, this would be more complex
    assign alu_op = (opcode == 7'b0110011) ? 3'b000 :  // R-type operations handled by instruction_decoder
                  (opcode == 7'b0010011) ? 3'b000 :  // ADDI
                  (opcode == 7'b0000011) ? 3'b000 :  // Load uses ADD for address calculation
                  (opcode == 7'b0100011) ? 3'b000 :  // Store uses ADD for address calculation
                  (opcode == 7'b1100011) ? 3'b001 :  // Branch uses SUB for comparison
                  3'b000;  // Default

    // Assign ALU source
    assign alu_src = (opcode == 7'b0010011) ? 2'b01 :  // I-type: use immediate
                    (opcode == 7'b0000011) ? 2'b01 :  // Load: use immediate for address offset
                    (opcode == 7'b0100011) ? 2'b01 :  // Store: use immediate for address offset
                    2'b00;  // Default: use register

    // Assign memory to register source
    assign mem_to_reg = (opcode == 7'b0000011) ? 2'b01 :  // Load: use memory data
                        2'b00;  // Default: use ALU result

    // Generate control signals based on opcode
    always @(*) begin
        case (opcode)
            7'b0110011: begin  // R-type
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 0;
                pc_src = 0;
            end
            7'b0010011: begin  // I-type (ADDI, etc.)
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 0;
                pc_src = 0;
            end
            7'b0000011: begin  // I-type (LW, LH, LB)
                reg_write = 1;
                mem_write = 0;
                mem_read = 1;
                branch = 0;
                jump = 0;
                pc_src = 0;
            end
            7'b0100011: begin  // S-type (SW, SH, SB)
                reg_write = 0;
                mem_write = 1;
                mem_read = 0;
                branch = 0;
                jump = 0;
                pc_src = 0;
            end
            7'b1100011: begin  // B-type (BEQ, BNE, etc.)
                reg_write = 0;
                mem_write = 0;
                mem_read = 0;
                branch = 1;
                jump = 0;
                pc_src = 1;
            end
            7'b1101111: begin  // J-type (JAL)
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 1;
                pc_src = 1;
            end
            7'b1100111: begin  // I-type (JALR)
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 1;
                pc_src = 1;
            end
            7'b1110011: begin  // SYSTEM (ECALL, EBREAK, CSR instructions)
                reg_write = 0;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 0;
                pc_src = 0;
            end
            default: begin  // Unknown opcode
                reg_write = 0;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 0;
                pc_src = 0;
            end
        endcase
    end

endmodule