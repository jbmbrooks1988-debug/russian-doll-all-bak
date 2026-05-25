// 8-bit Register File with 8 registers (R0-R7)
// R0 is hardwired to 0
module register_file_8bit (
    input clk,
    input rst,
    input reg_write_enable,
    input [2:0] read_addr_1,  // 3 bits to address 8 registers
    input [2:0] read_addr_2,  // 3 bits to address 8 registers
    input [2:0] write_addr,   // 3 bits to address 8 registers
    input [7:0] write_data,   // 8-bit data
    output [7:0] read_data_1, // 8-bit data
    output [7:0] read_data_2  // 8-bit data
);

    // 8 registers of 8 bits each
    reg [7:0] registers [0:7];

    // Initialize registers on reset
    integer i;
    initial begin
        for (i = 0; i < 8; i = i + 1) begin
            registers[i] = 8'b00000000;
        end
    end

    // Write operation (positive edge triggered)
    always @(posedge clk) begin
        if (rst) begin
            // Reset all registers except R0 (which stays 0)
            registers[1] <= 8'b00000000;
            registers[2] <= 8'b00000000;
            registers[3] <= 8'b00000000;
            registers[4] <= 8'b00000000;
            registers[5] <= 8'b00000000;
            registers[6] <= 8'b00000000;
            registers[7] <= 8'b00000000;
        end
        else if (reg_write_enable && write_addr != 3'b000) begin
            // Only write if write address is not R0 (R0 is always 0)
            registers[write_addr] <= write_data;
        end
    end

    // Read operations (combinational)
    assign read_data_1 = (read_addr_1 == 3'b000) ? 8'b00000000 : registers[read_addr_1];
    assign read_data_2 = (read_addr_2 == 3'b000) ? 8'b00000000 : registers[read_addr_2];

endmodule