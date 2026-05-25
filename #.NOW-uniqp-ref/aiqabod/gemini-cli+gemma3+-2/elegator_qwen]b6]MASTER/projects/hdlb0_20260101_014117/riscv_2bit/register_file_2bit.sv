// 2-bit Register File with 4 registers (R0-R3)
// R0 is hardwired to 0
module register_file_2bit (
    input clk,
    input rst,
    input reg_write_enable,
    input [1:0] read_addr_1,
    input [1:0] read_addr_2,
    input [1:0] write_addr,
    input [1:0] write_data,
    output [1:0] read_data_1,
    output [1:0] read_data_2
);

    // 4 registers of 2 bits each
    reg [1:0] registers [0:3];

    // Initialize registers on reset
    integer i;
    initial begin
        for (i = 0; i < 4; i = i + 1) begin
            registers[i] = 2'b00;
        end
    end

    // Write operation (positive edge triggered)
    always @(posedge clk) begin
        if (rst) begin
            // Reset all registers except R0 (which stays 0)
            registers[1] <= 2'b00;
            registers[2] <= 2'b00;
            registers[3] <= 2'b00;
        end
        else if (reg_write_enable && write_addr != 2'b00) begin
            // Only write if write address is not R0 (R0 is always 0)
            registers[write_addr] <= write_data;
        end
    end

    // Read operations (combinational)
    assign read_data_1 = (read_addr_1 == 2'b00) ? 2'b00 : registers[read_addr_1];
    assign read_data_2 = (read_addr_2 == 2'b00) ? 2'b00 : registers[read_addr_2];

endmodule