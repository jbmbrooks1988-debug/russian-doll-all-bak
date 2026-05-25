// Control Unit for 2-bit RISC-V processor
// Generates control signals based on opcode
module control_unit (
    input [1:0] opcode,
    input [1:0] instruction_lower,  // Lower 2 bits of instruction for additional decoding
    output reg_write,
    output mem_write,
    output mem_read,
    output [1:0] alu_op,
    output pc_enable  // Enable PC increment
);

    // Decode control signals based on opcode
    always @(*) begin
        // Default values
        reg_write = 1'b0;
        mem_write = 1'b0;
        mem_read = 1'b0;
        alu_op = 2'b00;  // Default to ADD
        pc_enable = 1'b1;  // Default to enable PC increment

        case (opcode)
            2'b00: begin  // R-type instructions (ADD)
                reg_write = 1'b1;
                alu_op = 2'b00;  // ADD
            end
            2'b01: begin  // R-type instructions (AND)
                reg_write = 1'b1;
                alu_op = 2'b10;  // AND
            end
            2'b10: begin  // I-type instruction (ADDI)
                reg_write = 1'b1;
                alu_op = 2'b00;  // ADD for immediate
            end
            2'b11: begin  // Memory operations (LW, SW)
                case (instruction_lower)  // Using lower 2 bits to differentiate LW vs SW
                    2'b00: begin  // LW (Load Word)
                        reg_write = 1'b1;
                        mem_read = 1'b1;
                        alu_op = 2'b00;  // ADD for address calculation
                    end
                    2'b01: begin  // SW (Store Word)
                        mem_write = 1'b1;
                        alu_op = 2'b00;  // ADD for address calculation
                    end
                    default: begin  // Default to LW
                        reg_write = 1'b1;
                        mem_read = 1'b1;
                        alu_op = 2'b00;  // ADD for address calculation
                    end
                endcase
            end
        endcase
    end

endmodule