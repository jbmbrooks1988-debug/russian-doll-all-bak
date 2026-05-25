// Control Unit for 16-bit RISC-V processor
// Generates control signals based on opcode
module control_unit (
    input [6:0] opcode,
    output reg_write,
    output mem_write,
    output mem_read,
    output branch,
    output jump,
    output [2:0] alu_op,
    output [1:0] alu_src,      // 00: rs2, 01: sign-extended immediate, 10: PC+4, 11: others
    output [1:0] mem_to_reg,   // 00: ALU result, 01: memory data, 10: PC+4, 11: others
    output pc_src              // 0: PC+2, 1: branch/jump target
);

    // Decode control signals based on opcode
    always @(*) begin
        case (opcode)
            7'b0110011: begin  // R-type ALU operations
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 0;
                alu_op = 3'b000;  // ADD by default, actual operation determined by funct3
                alu_src = 2'b00;  // Use rs2 as second operand
                mem_to_reg = 2'b00;  // Write ALU result to register
                pc_src = 0;  // Increment PC
            end
            7'b0010011: begin  // I-type ALU operations with immediate
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 0;
                alu_op = 3'b000;  // ADD by default, actual operation determined by funct3
                alu_src = 2'b01;  // Use sign-extended immediate as second operand
                mem_to_reg = 2'b00;  // Write ALU result to register
                pc_src = 0;  // Increment PC
            end
            7'b0000011: begin  // Load operations
                reg_write = 1;
                mem_write = 0;
                mem_read = 1;
                branch = 0;
                jump = 0;
                alu_op = 3'b000;  // ADD for address calculation
                alu_src = 2'b01;  // Use offset as second operand
                mem_to_reg = 2'b01;  // Write memory data to register
                pc_src = 0;  // Increment PC
            end
            7'b0100011: begin  // Store operations
                reg_write = 0;
                mem_write = 1;
                mem_read = 0;
                branch = 0;
                jump = 0;
                alu_op = 3'b000;  // ADD for address calculation
                alu_src = 2'b01;  // Use offset as second operand
                mem_to_reg = 2'b00;  // Not used for store
                pc_src = 0;  // Increment PC
            end
            7'b1100011: begin  // Branch operations
                reg_write = 0;
                mem_write = 0;
                mem_read = 0;
                branch = 1;
                jump = 0;
                alu_op = 3'b001;  // SUB for comparison
                alu_src = 2'b00;  // Use rs2 as second operand
                mem_to_reg = 2'b00;  // Not used for branches
                pc_src = 1;  // Use branch target
            end
            7'b1101111: begin  // JAL (Jump and Link) - J-type
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 1;
                alu_op = 3'b000;  // ADD for PC calculation
                alu_src = 2'b10;  // Use PC+2 as second operand
                mem_to_reg = 2'b10;  // Write PC+2 to register
                pc_src = 1;  // Use jump target
            end
            7'b1100111: begin  // JALR (Jump and Link Register) - I-type
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 1;
                alu_op = 3'b000;  // ADD for address calculation
                alu_src = 2'b01;  // Use offset as second operand
                mem_to_reg = 2'b10;  // Write PC+2 to register
                pc_src = 1;  // Use jump target
            end
            7'b0000000: begin  // Special case - NOP or halt
                reg_write = 0;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 0;
                alu_op = 3'b000;
                alu_src = 2'b00;
                mem_to_reg = 2'b00;
                pc_src = 0;  // Increment PC
            end
            default: begin  // Default case - treat as NOP
                reg_write = 0;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 0;
                alu_op = 3'b000;
                alu_src = 2'b00;
                mem_to_reg = 2'b00;
                pc_src = 0;  // Increment PC
            end
        endcase
    end

endmodule