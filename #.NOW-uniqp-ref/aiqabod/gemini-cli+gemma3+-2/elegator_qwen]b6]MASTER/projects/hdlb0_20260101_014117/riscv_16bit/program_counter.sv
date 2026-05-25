// 16-bit Program Counter for RISC-V processor
module program_counter (
    input clk,
    input rst,
    input enable,
    input [15:0] next_pc,
    output [15:0] current_pc
);

    reg [15:0] pc_reg;

    // Update PC on positive clock edge
    always @(posedge clk) begin
        if (rst) begin
            pc_reg <= 16'b0;  // Reset to 0
        end
        else if (enable) begin
            pc_reg <= next_pc;  // Update to next PC value
        end
    end

    assign current_pc = pc_reg;

endmodule