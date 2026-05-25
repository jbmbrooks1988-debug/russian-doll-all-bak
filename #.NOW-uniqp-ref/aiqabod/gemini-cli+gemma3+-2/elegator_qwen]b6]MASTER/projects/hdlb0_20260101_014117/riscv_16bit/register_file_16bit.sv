// 16-bit Register File with 32 registers
// x0 is hardwired to 0
module register_file_16bit (
    input clk,
    input rst,
    input reg_write_enable,
    input [4:0] read_addr_1,  // 5-bit address for 32 registers
    input [4:0] read_addr_2,
    input [4:0] write_addr,
    input [15:0] write_data,
    output [15:0] read_data_1,
    output [15:0] read_data_2
);

    // 32 registers of 16 bits each
    reg [15:0] registers [0:31];
    
    // Initialize register x0 to 0 and others to 0 on reset
    integer i;
    initial begin
        registers[0] = 16'b0;
        for (i = 1; i < 32; i = i + 1) begin
            registers[i] = 16'b0;
        end
    end

    // Write operation (active on positive clock edge)
    always @(posedge clk) begin
        if (rst) begin
            // Reset all registers to 0
            for (i = 1; i < 32; i = i + 1) begin
                registers[i] <= 16'b0;
            end
        end
        else if (reg_write_enable && write_addr != 5'b00000) begin
            // Only write if write enable is high and not writing to x0
            registers[write_addr] <= write_data;
        end
    end

    // Read operations (combinational)
    assign read_data_1 = (read_addr_1 == 5'b00000) ? 16'b0 : registers[read_addr_1];
    assign read_data_2 = (read_addr_2 == 5'b00000) ? 16'b0 : registers[read_addr_2];

endmodule