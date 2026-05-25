// 2-bit Memory subsystem with 4 words (2 bits each)
module memory_2bit (
    input clk,
    input rst,
    input mem_write_enable,
    input [1:0] address,
    input [1:0] write_data,
    output [1:0] read_data
);

    // 4 memory locations of 2 bits each
    reg [1:0] memory [0:3];
    
    // Initialize memory on reset
    integer i;
    initial begin
        for (i = 0; i < 4; i = i + 1) begin
            memory[i] = 2'b00;
        end
    end
    
    // Write operation (positive edge triggered)
    always @(posedge clk) begin
        if (rst) begin
            // Reset all memory locations
            memory[0] <= 2'b00;
            memory[1] <= 2'b00;
            memory[2] <= 2'b00;
            memory[3] <= 2'b00;
        end
        else if (mem_write_enable) begin
            memory[address] <= write_data;
        end
    end
    
    // Read operation (combinational)
    assign read_data = memory[address];

endmodule