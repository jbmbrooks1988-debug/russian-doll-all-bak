// Testbench for Program Counter
module program_counter_tb;

    reg clk;
    reg rst;
    reg enable;
    reg [1:0] next_pc;
    wire [1:0] current_pc;

    // Instantiate the program counter
    program_counter uut (
        .clk(clk),
        .rst(rst),
        .enable(enable),
        .next_pc(next_pc),
        .current_pc(current_pc)
    );

    // Clock generation
    initial begin
        clk = 0;
        forever #5 clk = ~clk;  // 10 time unit clock period
    end

    // Test sequence
    initial begin
        // Initialize signals
        rst = 1;
        enable = 0;
        next_pc = 0;
        
        // Apply reset
        #10;
        rst = 0;
        #10;
        $display("Time %0t: After reset, PC = %b", $time, current_pc);
        
        // Test PC increment when enabled
        enable = 1;
        next_pc = 2'b01;
        #20;  // Wait for 2 clock cycles
        $display("Time %0t: After setting next PC to 1, current PC = %b", $time, current_pc);
        
        next_pc = 2'b10;
        #20;  // Wait for 2 clock cycles
        $display("Time %0t: After setting next PC to 2, current PC = %b", $time, current_pc);
        
        next_pc = 2'b11;
        #20;  // Wait for 2 clock cycles
        $display("Time %0t: After setting next PC to 3, current PC = %b", $time, current_pc);
        
        // Test disabling PC update
        enable = 0;
        next_pc = 2'b00;
        #20;  // Wait for 2 clock cycles
        $display("Time %0t: With enable=0, PC should remain at %b", $time, current_pc);
        
        // Re-enable and test wraparound
        enable = 1;
        next_pc = 2'b00;
        #20;  // Wait for 2 clock cycles
        $display("Time %0t: After re-enabling, PC = %b", $time, current_pc);
        
        // End simulation
        #20;
        $finish;
    end

endmodule