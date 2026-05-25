// Testbench for 16-bit RISC-V Processor
module riscv_16bit_processor_tb;

    // Testbench signals
    reg clk;
    reg rst;
    reg [15:0] data_in;
    wire [15:0] data_out;
    wire [15:0] addr;
    wire mem_write;
    wire mem_read;
    reg timer_interrupt;
    reg external_interrupt;
    reg software_interrupt;
    wire [15:0] pc_out;
    
    // Memory for testing
    reg [15:0] test_memory [0:65535];
    
    // Instantiate the 16-bit processor
    riscv_16bit_processor uut (
        .clk(clk),
        .rst(rst),
        .data_in(data_in),
        .data_out(data_out),
        .addr(addr),
        .mem_write(mem_write),
        .mem_read(mem_read),
        .timer_interrupt(timer_interrupt),
        .external_interrupt(external_interrupt),
        .software_interrupt(software_interrupt),
        .pc_out(pc_out)
    );
    
    // Clock generation
    initial begin
        clk = 0;
        forever #5 clk = ~clk;  // 10 time unit period
    end
    
    // Test program - simple addition
    initial begin
        integer i;
        
        // Initialize test memory with a simple program
        // This is a placeholder - in a real test, we'd load actual instructions
        test_memory[0] = 16'b0000000000000000;  // NOP
        test_memory[1] = 16'b0000000000000000;  // NOP
        test_memory[2] = 16'b0000000000000000;  // NOP
        test_memory[3] = 16'b0000000000000000;  // NOP
        
        // Initialize inputs
        rst = 1;
        data_in = 16'b0000000000000000;
        timer_interrupt = 0;
        external_interrupt = 0;
        software_interrupt = 0;
        
        // Apply reset
        #15;
        rst = 0;
        
        // Load first instruction
        data_in = test_memory[0];
        
        // Run for several cycles
        #100;
        
        // Apply a timer interrupt
        timer_interrupt = 1;
        #10;
        timer_interrupt = 0;
        
        // Continue execution
        #100;
        
        // Apply an external interrupt
        external_interrupt = 1;
        #10;
        external_interrupt = 0;
        
        // Continue execution
        #100;
        
        // Finish simulation
        $display("Simulation completed");
        $display("Final PC: %h", pc_out);
        $display("Final addr: %h", addr);
        $display("Final data_out: %h", data_out);
        $finish;
    end
    
    // Monitor important signals
    initial begin
        $monitor("Time: %t, PC: %h, addr: %h, data_in: %h, data_out: %h, mem_write: %b, mem_read: %b", 
                 $time, pc_out, addr, data_in, data_out, mem_write, mem_read);
    end

endmodule