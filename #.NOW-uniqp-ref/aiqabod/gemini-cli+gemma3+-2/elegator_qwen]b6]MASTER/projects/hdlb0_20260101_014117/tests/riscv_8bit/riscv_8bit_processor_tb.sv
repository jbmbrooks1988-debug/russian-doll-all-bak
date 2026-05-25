// Testbench for 8-bit RISC-V Processor
`timescale 1ns/1ps

module riscv_8bit_processor_tb;

    // Clock and reset
    reg clk;
    reg rst;
    
    // Memory interface
    reg [7:0] data_in;
    wire [7:0] data_out;
    wire [7:0] addr;
    wire mem_write;
    wire mem_read;
    
    // Interrupt lines
    reg timer_interrupt;
    reg keyboard_interrupt;
    reg external_interrupt;
    
    // Status output
    wire [7:0] pc_out;
    
    // Instantiate the 8-bit processor
    riscv_8bit_processor uut (
        .clk(clk),
        .rst(rst),
        .data_in(data_in),
        .data_out(data_out),
        .addr(addr),
        .mem_write(mem_write),
        .mem_read(mem_read),
        .timer_interrupt(timer_interrupt),
        .keyboard_interrupt(keyboard_interrupt),
        .external_interrupt(external_interrupt),
        .pc_out(pc_out)
    );
    
    // Memory for the processor
    reg [7:0] memory [0:255];
    
    // Initialize memory
    integer i;
    initial begin
        for (i = 0; i < 256; i = i + 1) begin
            memory[i] = 8'b00000000;
        end
        
        // Load a simple test program into memory
        // This is a simple program that adds two numbers
        // Instruction: add r3, r1, r2 (r3 = r1 + r2)
        // This would need to be encoded in the actual RISC-V instruction format
        // For simplicity, we'll just load some test data
        memory[0] = 8'h13;  // First byte of instruction
        memory[1] = 8'h00;  // Second byte of instruction
        memory[2] = 8'h01;  // Next instruction
        memory[3] = 8'h02;  // Next instruction
    end
    
    // Memory read/write logic
    always @(*) begin
        if (mem_read) begin
            data_in = memory[addr];
        end
        else begin
            data_in = 8'bZZZZZZZZ;  // High impedance when not reading
        end
    end
    
    // Memory write logic
    always @(posedge clk) begin
        if (mem_write) begin
            memory[addr] <= data_out;
        end
    end
    
    // Clock generation
    initial begin
        clk = 0;
        forever #5 clk = ~clk;  // 10ns period, 100MHz clock
    end
    
    // Test sequence
    initial begin
        // Initialize signals
        rst = 1;
        timer_interrupt = 0;
        keyboard_interrupt = 0;
        external_interrupt = 0;
        
        // Apply reset
        #15;
        rst = 0;
        
        // Run simulation for some time
        #1000;
        
        // Apply an interrupt
        timer_interrupt = 1;
        #10;
        timer_interrupt = 0;
        
        // Continue simulation
        #500;
        
        // Finish simulation
        $display("Simulation completed");
        $display("Final PC: %h", pc_out);
        $finish;
    end
    
    // Monitor important signals
    initial begin
        $monitor("Time: %t, PC: %h, Addr: %h, Data Out: %h, Mem Write: %b, Mem Read: %b", 
                 $time, pc_out, addr, data_out, mem_write, mem_read);
    end

endmodule