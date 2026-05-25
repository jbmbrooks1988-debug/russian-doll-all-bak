// 2-bit Program Counter
module program_counter (
    input clk,
    input rst,
    input enable,
    input [1:0] next_pc,
    output reg [1:0] current_pc
);

    // Update PC on positive clock edge if enabled
    always @(posedge clk) begin
        if (rst) begin
            current_pc <= 2'b00;  // Reset to 0
        end
        else if (enable) begin
            current_pc <= next_pc;
        end
    end

endmodule