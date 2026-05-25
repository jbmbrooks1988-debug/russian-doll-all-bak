// 8-bit ALU supporting ADD, SUB, AND, OR, XOR, SLL, SRL, SLT operations
module alu_8bit (
    input [7:0] operand_a,
    input [7:0] operand_b,
    input [2:0] alu_op,  // 000: ADD, 001: SUB, 010: AND, 011: OR, 100: XOR, 101: SLL, 110: SRL, 111: SLT
    output [7:0] result,
    output zero_flag  // Set to 1 if result is zero
);

    wire [8:0] temp_result;  // 9 bits to handle potential overflow for addition/subtraction
    wire [7:0] shift_result;
    wire [7:0] slt_result;   // Result for set less than operation

    // Shift operations
    reg [7:0] temp_shift;
    integer shift_amount;
    assign shift_amount = operand_b[2:0];  // Use lower 3 bits of operand_b as shift amount for 8-bit
    
    always @(*) begin
        case (alu_op)
            3'b101: begin  // SLL (Shift Left Logical)
                temp_shift = operand_a << shift_amount;
            end
            3'b110: begin  // SRL (Shift Right Logical)
                temp_shift = operand_a >> shift_amount;
            end
            default: begin
                temp_shift = 8'b00000000;
            end
        endcase
    end

    // SLT (Set Less Than) operation - signed comparison
    assign slt_result = ($signed(operand_a) < $signed(operand_b)) ? 8'b00000001 : 8'b00000000;

    // Perform operations based on alu_op
    assign temp_result = (alu_op == 3'b000) ? {1'b0, operand_a} + {1'b0, operand_b} :  // ADD
                        (alu_op == 3'b001) ? {1'b0, operand_a} - {1'b0, operand_b} :  // SUB
                        (alu_op == 3'b010) ? {1'b0, operand_a & operand_b} :          // AND
                        (alu_op == 3'b011) ? {1'b0, operand_a | operand_b} :          // OR
                        (alu_op == 3'b100) ? {1'b0, operand_a ^ operand_b} :          // XOR
                        {1'b0, 8'b00000000};                                          // Default

    // Select final result based on alu_op
    assign result = (alu_op == 3'b101) ? temp_shift :      // SLL
                    (alu_op == 3'b110) ? temp_shift :      // SRL
                    (alu_op == 3'b111) ? slt_result :      // SLT
                    temp_result[7:0];                      // All other operations

    // Zero flag is set when result is 0
    assign zero_flag = (result == 8'b00000000);

endmodule