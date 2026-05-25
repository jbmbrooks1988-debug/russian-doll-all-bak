// 8-bit Program Counter
// Manages the instruction address for the 8-bit RISC-V processor
module program_counter (
    input clk,
    input rst,
    input enable,
    input [7:0] next_pc,
    output [7:0] current_pc
);

    reg [7:0] pc_reg;

    always @(posedge clk) begin
        if (rst) begin
            pc_reg <= 8'b00000000;  // Reset to start of memory
        end
        else if (enable) begin
            pc_reg <= next_pc;
        end
    end

    assign current_pc = pc_reg;

endmodule

// 8-bit Instruction Memory
// Stores instructions for the 8-bit RISC-V processor
module instruction_memory (
    input clk,
    input rst,
    input [7:0] address,
    output [15:0] instruction
);

    // 256 bytes of instruction memory (for 8-bit address space)
    // Each instruction is 16 bits (2 bytes)
    reg [15:0] mem [0:127];  // 128 16-bit instructions

    // Initialize with NOP (no operation) instructions
    integer i;
    initial begin
        for (i = 0; i < 128; i = i + 1) begin
            mem[i] = 16'b0000000000000000;  // NOP instruction
        end
    end

    assign instruction = mem[address[7:1]];  // Use upper 7 bits to index, since each instruction is 2 bytes

endmodule

// Instruction Fetch Unit
module instruction_fetch_unit (
    input clk,
    input rst,
    input [7:0] pc,
    output [15:0] instruction,
    output [7:0] next_pc
);

    // Instruction memory
    instruction_memory instr_mem (
        .clk(clk),
        .rst(rst),
        .address(pc),
        .instruction(instruction)
    );

    // Calculate next PC (increment by 2 for 16-bit instructions)
    assign next_pc = pc + 8'd2;

endmodule