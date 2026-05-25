// Testbench for 2-bit ALU
module alu_2bit_tb;

    reg [1:0] operand_a;
    reg [1:0] operand_b;
    reg [1:0] alu_op;
    wire [1:0] result;
    wire zero_flag;

    // Instantiate the ALU
    alu_2bit uut (
        .operand_a(operand_a),
        .operand_b(operand_b),
        .alu_op(alu_op),
        .result(result),
        .zero_flag(zero_flag)
    );

    // Test sequence
    initial begin
        // Test ADD operation (00)
        alu_op = 2'b00;
        operand_a = 2'b01;
        operand_b = 2'b10;
        #10;
        $display("Time %0t: ADD %b + %b = %b, zero_flag=%b", $time, operand_a, operand_b, result, zero_flag);
        
        operand_a = 2'b11;
        operand_b = 2'b01;
        #10;
        $display("Time %0t: ADD %b + %b = %b, zero_flag=%b", $time, operand_a, operand_b, result, zero_flag);
        
        // Test SUB operation (01)
        alu_op = 2'b01;
        operand_a = 2'b11;
        operand_b = 2'b01;
        #10;
        $display("Time %0t: SUB %b - %b = %b, zero_flag=%b", $time, operand_a, operand_b, result, zero_flag);
        
        operand_a = 2'b01;
        operand_b = 2'b11;
        #10;
        $display("Time %0t: SUB %b - %b = %b, zero_flag=%b", $time, operand_a, operand_b, result, zero_flag);
        
        // Test AND operation (10)
        alu_op = 2'b10;
        operand_a = 2'b11;
        operand_b = 2'b10;
        #10;
        $display("Time %0t: AND %b & %b = %b, zero_flag=%b", $time, operand_a, operand_b, result, zero_flag);
        
        operand_a = 2'b01;
        operand_b = 2'b10;
        #10;
        $display("Time %0t: AND %b & %b = %b, zero_flag=%b", $time, operand_a, operand_b, result, zero_flag);
        
        // Test OR operation (11)
        alu_op = 2'b11;
        operand_a = 2'b01;
        operand_b = 2'b10;
        #10;
        $display("Time %0t: OR %b | %b = %b, zero_flag=%b", $time, operand_a, operand_b, result, zero_flag);
        
        operand_a = 2'b00;
        operand_b = 2'b00;
        #10;
        $display("Time %0t: OR %b | %b = %b, zero_flag=%b", $time, operand_a, operand_b, result, zero_flag);
        
        // Test XOR operation (when alu_op is not 00, 01, 10, or 11)
        alu_op = 2'b11;  // Actually XOR since it's not 00, 01, or 10
        operand_a = 2'b11;
        operand_b = 2'b10;
        #10;
        $display("Time %0t: XOR %b ^ %b = %b, zero_flag=%b", $time, operand_a, operand_b, result, zero_flag);
        
        operand_a = 2'b01;
        operand_b = 2'b01;
        #10;
        $display("Time %0t: XOR %b ^ %b = %b, zero_flag=%b", $time, operand_a, operand_b, result, zero_flag);
        
        // End simulation
        #20;
        $finish;
    end

endmodule