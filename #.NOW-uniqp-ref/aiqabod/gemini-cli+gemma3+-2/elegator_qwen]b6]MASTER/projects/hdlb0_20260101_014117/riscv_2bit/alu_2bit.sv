// 2-bit ALU supporting ADD, SUB, AND, OR, XOR operations
module alu_2bit (
    input [1:0] operand_a,
    input [1:0] operand_b,
    input [1:0] alu_op,  // 00: ADD, 01: SUB, 10: AND, 11: OR/XOR (depending on context)
    output [1:0] result,
    output zero_flag  // Set to 1 if result is zero
);

    wire [2:0] temp_result;  // 3 bits to handle potential overflow for addition/subtraction
    wire [1:0] xor_result;

    // XOR operation
    assign xor_result = operand_a ^ operand_b;

    // Perform operations based on alu_op
    assign temp_result = (alu_op == 2'b00) ? {1'b0, operand_a} + {1'b0, operand_b} :  // ADD
                        (alu_op == 2'b01) ? {1'b0, operand_a} - {1'b0, operand_b} :  // SUB
                        (alu_op == 2'b10) ? {1'b0, operand_a & operand_b} :          // AND
                        (alu_op == 2'b11) ? {1'b0, operand_a ^ operand_b} :          // XOR (changed from OR)
                        {1'b0, operand_a | operand_b};                               // OR (fallback)

    // Take only the lower 2 bits as the result
    assign result = temp_result[1:0];

    // Zero flag is set when result is 0
    assign zero_flag = (result == 2'b00);

endmodule