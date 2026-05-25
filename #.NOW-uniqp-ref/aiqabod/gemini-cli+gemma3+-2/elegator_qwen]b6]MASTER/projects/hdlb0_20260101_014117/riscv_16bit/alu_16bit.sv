// 16-bit ALU for RISC-V processor
// Supports various arithmetic and logical operations
module alu_16bit (
    input [15:0] operand_a,
    input [15:0] operand_b,
    input [2:0] alu_op,      // Operation control
    output [15:0] result,
    output zero_flag,
    output overflow_flag
);

    // Internal signals
    wire [15:0] add_result, sub_result;
    wire [15:0] and_result, or_result, xor_result;
    wire [15:0] sll_result, srl_result, sra_result;
    wire [15:0] slt_result, sltu_result;
    
    // Arithmetic operations
    assign add_result = operand_a + operand_b;
    assign sub_result = operand_a - operand_b;
    
    // Overflow detection for addition and subtraction
    wire overflow_add = (operand_a[15] == operand_b[15]) && (operand_a[15] != add_result[15]);
    wire overflow_sub = (operand_a[15] != operand_b[15]) && (operand_a[15] != sub_result[15]);
    
    // Logical operations
    assign and_result = operand_a & operand_b;
    assign or_result  = operand_a | operand_b;
    assign xor_result = operand_a ^ operand_b;
    
    // Shift operations (using lower 4 bits of operand_b as shift amount)
    wire [3:0] shift_amount = operand_b[3:0];
    assign sll_result = operand_a << shift_amount;
    assign srl_result = operand_a >> shift_amount;
    assign sra_result = $signed(operand_a) >>> shift_amount;  // Arithmetic right shift
    
    // Set less than operations
    assign slt_result = ($signed(operand_a) < $signed(operand_b)) ? 16'h0001 : 16'h0000;
    assign sltu_result = (operand_a < operand_b) ? 16'h0001 : 16'h0000;

    // Result multiplexer based on alu_op
    always @(*) begin
        case (alu_op)
            3'b000: result = add_result;      // ADD
            3'b100: result = sub_result;      // SUB
            3'b001: result = and_result;      // AND
            3'b010: result = or_result;       // OR
            3'b011: result = xor_result;      // XOR
            3'b101: result = sll_result;      // SLL (Shift Left Logical)
            3'b110: result = srl_result;      // SRL (Shift Right Logical)
            3'b111: result = sra_result;      // SRA (Shift Right Arithmetic)
            3'b1000: result = slt_result;     // SLT (Set Less Than)
            3'b1001: result = sltu_result;    // SLTU (Set Less Than Unsigned)
            default: result = 16'b0;          // Default case
        endcase
    end

    // Zero flag - set if result is zero
    assign zero_flag = (result == 16'b0);

    // Overflow flag - depends on operation
    always @(*) begin
        case (alu_op)
            3'b000: overflow_flag = overflow_add;  // ADD
            3'b100: overflow_flag = overflow_sub;  // SUB
            default: overflow_flag = 1'b0;          // No overflow for other operations
        endcase
    end

endmodule