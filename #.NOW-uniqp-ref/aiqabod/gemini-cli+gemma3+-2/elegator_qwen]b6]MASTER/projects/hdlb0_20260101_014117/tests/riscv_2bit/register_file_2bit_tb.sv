// Testbench for 2-bit Register File
module register_file_2bit_tb;

    reg clk;
    reg rst;
    reg reg_write_enable;
    reg [1:0] read_addr_1;
    reg [1:0] read_addr_2;
    reg [1:0] write_addr;
    reg [1:0] write_data;
    wire [1:0] read_data_1;
    wire [1:0] read_data_2;

    // Instantiate the register file
    register_file_2bit uut (
        .clk(clk),
        .rst(rst),
        .reg_write_enable(reg_write_enable),
        .read_addr_1(read_addr_1),
        .read_addr_2(read_addr_2),
        .write_addr(write_addr),
        .write_data(write_data),
        .read_data_1(read_data_1),
        .read_data_2(read_data_2)
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
        reg_write_enable = 0;
        read_addr_1 = 0;
        read_addr_2 = 0;
        write_addr = 0;
        write_data = 0;
        
        // Apply reset
        #10;
        rst = 0;
        #10;
        
        // Test 1: Read from R0 (should always be 0)
        read_addr_1 = 2'b00;  // R0
        read_addr_2 = 2'b01;  // R1
        #10;
        $display("Time %0t: Read from R0=%b, R1=%b", $time, read_data_1, read_data_2);
        
        // Test 2: Write to R1
        reg_write_enable = 1;
        write_addr = 2'b01;  // R1
        write_data = 2'b10;  // Write value 2
        #10;
        $display("Time %0t: Wrote 2'b10 to R1", $time);
        
        // Check read after write
        read_addr_1 = 2'b01;  // R1
        #10;
        $display("Time %0t: Read from R1=%b", $time, read_data_1);
        
        // Test 3: Write to R2
        write_addr = 2'b10;  // R2
        write_data = 2'b11;  // Write value 3
        #10;
        $display("Time %0t: Wrote 2'b11 to R2", $time);
        
        // Check read from R2
        read_addr_1 = 2'b10;  // R2
        #10;
        $display("Time %0t: Read from R2=%b", $time, read_data_1);
        
        // Test 4: Try to write to R0 (should not change)
        write_addr = 2'b00;  // R0
        write_data = 2'b11;  // Try to write value 3 to R0
        #10;
        $display("Time %0t: Attempted to write 2'b11 to R0", $time);
        
        // Check that R0 is still 0
        read_addr_1 = 2'b00;  // R0
        #10;
        $display("Time %0t: Read from R0=%b (should still be 0)", $time, read_data_1);
        
        // End simulation
        #20;
        $finish;
    end

endmodule