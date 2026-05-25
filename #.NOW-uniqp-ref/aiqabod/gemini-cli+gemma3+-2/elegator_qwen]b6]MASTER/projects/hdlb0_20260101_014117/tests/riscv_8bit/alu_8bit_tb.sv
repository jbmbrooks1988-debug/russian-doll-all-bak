// Testbench for 8-bit ALU
`timescale 1ns/1ps

module alu_8bit_tb;

    reg [7:0] operand_a;
    reg [7:0] operand_b;
    reg [2:0] alu_op;
    wire [7:0] result;
    wire zero_flag;

    alu_8bit uut (
        .operand_a(operand_a),
        .operand_b(operand_b),
        .alu_op(alu_op),
        .result(result),
        .zero_flag(zero_flag)
    );

    initial begin
        // Test ADD operation
        alu_op = 3'b000;
        operand_a = 8'd10;
        operand_b = 8'd5;
        #10;
        $display("ADD: %d + %d = %d, zero_flag = %b", operand_a, operand_b, result, zero_flag);

        // Test SUB operation
        alu_op = 3'b001;
        operand_a = 8'd10;
        operand_b = 8'd5;
        #10;
        $display("SUB: %d - %d = %d, zero_flag = %b", operand_a, operand_b, result, zero_flag);

        // Test AND operation
        alu_op = 3'b010;
        operand_a = 8'hFF;
        operand_b = 8'h0F;
        #10;
        $display("AND: %h & %h = %h, zero_flag = %b", operand_a, operand_b, result, zero_flag);

        // Test OR operation
        alu_op = 3'b011;
        operand_a = 8'hF0;
        operand_b = 8'h0F;
        #10;
        $display("OR: %h | %h = %h, zero_flag = %b", operand_a, operand_b, result, zero_flag);

        // Test XOR operation
        alu_op = 3'b100;
        operand_a = 8'hFF;
        operand_b = 8'h0F;
        #10;
        $display("XOR: %h ^ %h = %h, zero_flag = %b", operand_a, operand_b, result, zero_flag);

        // Test SLL operation
        alu_op = 3'b101;
        operand_a = 8'd1;  // 00000001
        operand_b = 8'd3;  // Shift by 3 positions
        #10;
        $display("SLL: %b << %d = %b (%d), zero_flag = %b", operand_a, operand_b[2:0], result, result, zero_flag);

        // Test SRL operation
        alu_op = 3'b110;
        operand_a = 8'd8;  // 00001000
        operand_b = 8'd2;  // Shift by 2 positions
        #10;
        $display("SRL: %b >> %d = %b (%d), zero_flag = %b", operand_a, operand_b[2:0], result, result, zero_flag);

        // Test SLT operation
        alu_op = 3'b111;
        operand_a = 8'd5;
        operand_b = 8'd10;
        #10;
        $display("SLT: %d < %d = %d, zero_flag = %b", operand_a, operand_b, result, zero_flag);

        operand_a = 8'd10;
        operand_b = 8'd5;
        #10;
        $display("SLT: %d < %d = %d, zero_flag = %b", operand_a, operand_b, result, zero_flag);

        // Test with zero result
        operand_a = 8'd5;
        operand_b = 8'd5;
        #10;
        $display("SUB: %d - %d = %d, zero_flag = %b", operand_a, operand_b, result, zero_flag);

        $finish;
    end

endmodule