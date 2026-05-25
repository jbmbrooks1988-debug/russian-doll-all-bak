// Testbench for 16-bit ALU
module alu_16bit_tb;

    // Testbench signals
    reg [15:0] operand_a;
    reg [15:0] operand_b;
    reg [2:0] alu_op;
    wire [15:0] result;
    wire zero_flag;
    wire overflow_flag;
    
    // Instantiate the 16-bit ALU
    alu_16bit uut (
        .operand_a(operand_a),
        .operand_b(operand_b),
        .alu_op(alu_op),
        .result(result),
        .zero_flag(zero_flag),
        .overflow_flag(overflow_flag)
    );
    
    // Test program
    initial begin
        // Initialize inputs
        operand_a = 16'h0000;
        operand_b = 16'h0000;
        alu_op = 3'b000;  // ADD
        
        // Test 1: Addition
        #10;
        operand_a = 16'h0005;
        operand_b = 16'h0003;
        alu_op = 3'b000;  // ADD
        $display("Time: %t, ADD: %h + %h = %h, zero: %b, overflow: %b", 
                 $time, operand_a, operand_b, result, zero_flag, overflow_flag);
        
        // Test 2: Subtraction
        #10;
        operand_a = 16'h000A;
        operand_b = 16'h0003;
        alu_op = 3'b100;  // SUB
        $display("Time: %t, SUB: %h - %h = %h, zero: %b, overflow: %b", 
                 $time, operand_a, operand_b, result, zero_flag, overflow_flag);
        
        // Test 3: AND operation
        #10;
        operand_a = 16'h000F;
        operand_b = 16'h0003;
        alu_op = 3'b001;  // AND
        $display("Time: %t, AND: %h & %h = %h, zero: %b, overflow: %b", 
                 $time, operand_a, operand_b, result, zero_flag, overflow_flag);
        
        // Test 4: OR operation
        #10;
        operand_a = 16'h000F;
        operand_b = 16'h0003;
        alu_op = 3'b010;  // OR
        $display("Time: %t, OR: %h | %h = %h, zero: %b, overflow: %b", 
                 $time, operand_a, operand_b, result, zero_flag, overflow_flag);
        
        // Test 5: XOR operation
        #10;
        operand_a = 16'h000F;
        operand_b = 16'h0003;
        alu_op = 3'b011;  // XOR
        $display("Time: %t, XOR: %h ^ %h = %h, zero: %b, overflow: %b", 
                 $time, operand_a, operand_b, result, zero_flag, overflow_flag);
        
        // Test 6: Shift Left
        #10;
        operand_a = 16'h0005;
        operand_b = 16'h0002;  // Use lower 4 bits as shift amount
        alu_op = 3'b101;  // SLL
        $display("Time: %t, SLL: %h << %h = %h, zero: %b, overflow: %b", 
                 $time, operand_a, operand_b[3:0], result, zero_flag, overflow_flag);
        
        // Test 7: Set Less Than
        #10;
        operand_a = 16'h0003;
        operand_b = 16'h0005;
        alu_op = 3'b1000;  // SLT
        $display("Time: %t, SLT: %h < %h = %h, zero: %b, overflow: %b", 
                 $time, operand_a, operand_b, result, zero_flag, overflow_flag);
        
        // Test 8: Zero result
        #10;
        operand_a = 16'h0005;
        operand_b = 16'h0005;
        alu_op = 3'b100;  // SUB (5-5=0)
        $display("Time: %t, SUB: %h - %h = %h, zero: %b, overflow: %b", 
                 $time, operand_a, operand_b, result, zero_flag, overflow_flag);
        
        // Finish simulation
        #10;
        $display("ALU testbench completed");
        $finish;
    end
    
    // Monitor important signals
    initial begin
        $monitor("Time: %t, A: %h, B: %h, Op: %b, Result: %h, Zero: %b, Overflow: %b", 
                 $time, operand_a, operand_b, alu_op, result, zero_flag, overflow_flag);
    end

endmodule