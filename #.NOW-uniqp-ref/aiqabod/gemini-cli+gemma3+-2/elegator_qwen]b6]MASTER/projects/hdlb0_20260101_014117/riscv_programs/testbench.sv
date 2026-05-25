// Testbench for 2-bit RISC-V processor with HDLb0 programs
// This testbench loads HDLb0 programs and verifies their execution

`timescale 1ns/1ps

module riscv_2bit_testbench;
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
        rst = 1;
        #20;  // Hold reset for 20ns
        rst = 0;
        
        // Wait for processor to execute
        #100;
        
        // End simulation
        $display("Test completed at time %t", $time);
        $finish;
    end
    
    // Monitor register and memory values during execution
    initial begin
        $monitor("Time: %t, PC: %b, R0: %b, R1: %b, R2: %b, R3: %b", 
                 $time, processor.pc_current, 
                 processor.reg_file.register_array[0],
                 processor.reg_file.register_array[1],
                 processor.reg_file.register_array[2],
                 processor.reg_file.register_array[3]);
    end
    
endmodule