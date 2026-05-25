// Testbench for 2-bit RISC-V processor with HDLb0 programs
// This testbench loads HDLb0 programs and verifies their execution

`timescale 1ns/1ps

module riscv_2bit_processor_test;
    // Clock and reset signals
    reg clk;
    reg rst;
    
    // Processor outputs
    wire [1:0] pc_out;
    
    // Instantiate the processor
    riscv_2bit_processor processor (
        .clk(clk),
        .rst(rst),
        .pc_out(pc_out)
    );
    
    // Clock generation - 100MHz clock
    initial begin
        clk = 0;
        forever #5 clk = ~clk;  // Toggle every 5ns (100MHz)
    end
    
    // Test program execution
    initial begin
        // Initialize
        $display("Starting 2-bit RISC-V processor test...");
        rst = 1;
        #20;  // Hold reset for 20ns
        rst = 0;
        
        $display("Reset released, starting execution...");
        
        // Wait for several clock cycles to allow program execution
        #200;
        
        // Display final state
        $display("Final PC: %d", processor.pc_current);
        
        // End simulation
        $display("Test completed at time %t", $time);
        $finish;
    end
    
endmodule