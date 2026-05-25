// Testbench for 2-bit Memory Subsystem
module memory_2bit_tb;

    reg clk;
    reg rst;
    reg mem_write_enable;
    reg [1:0] address;
    reg [1:0] write_data;
    wire [1:0] read_data;

    // Instantiate the memory
    memory_2bit uut (
        .clk(clk),
        .rst(rst),
        .mem_write_enable(mem_write_enable),
        .address(address),
        .write_data(write_data),
        .read_data(read_data)
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
        mem_write_enable = 0;
        address = 0;
        write_data = 0;
        
        // Apply reset
        #10;
        rst = 0;
        #10;
        
        // Test initial values (should be 0)
        address = 2'b00;
        #10;
        $display("Time %0t: After reset, memory[0] = %b", $time, read_data);
        
        address = 2'b01;
        #10;
        $display("Time %0t: After reset, memory[1] = %b", $time, read_data);
        
        // Test writing to memory
        mem_write_enable = 1;
        address = 2'b00;
        write_data = 2'b11;
        #10;
        $display("Time %0t: Wrote 2'b11 to memory[0]", $time);
        
        // Read back the written value
        mem_write_enable = 0;
        address = 2'b00;
        #10;
        $display("Time %0t: Read from memory[0] = %b", $time, read_data);
        
        // Test writing to another location
        mem_write_enable = 1;
        address = 2'b10;
        write_data = 2'b10;
        #10;
        $display("Time %0t: Wrote 2'b10 to memory[2]", $time);
        
        // Read back the written value
        mem_write_enable = 0;
        address = 2'b10;
        #10;
        $display("Time %0t: Read from memory[2] = %b", $time, read_data);
        
        // Test reading from other locations to ensure they weren't affected
        address = 2'b01;
        #10;
        $display("Time %0t: Read from memory[1] = %b (should still be 0)", $time, read_data);
        
        address = 2'b11;
        #10;
        $display("Time %0t: Read from memory[3] = %b (should still be 0)", $time, read_data);
        
        // End simulation
        #20;
        $finish;
    end

endmodule