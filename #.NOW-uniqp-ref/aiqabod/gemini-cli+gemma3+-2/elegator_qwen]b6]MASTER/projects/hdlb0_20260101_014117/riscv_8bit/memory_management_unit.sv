// 8-bit Memory Management Unit
// Compatible with kernel requirements for the 8-bit RISC-V processor
module memory_management_unit (
    input clk,
    input rst,
    // Data memory interface
    input mem_write_enable,
    input mem_read_enable,
    input [7:0] address,
    input [7:0] write_data,
    output [7:0] read_data,
    // Memory management signals
    input [7:0] vaddr,        // Virtual address
    output [7:0] paddr,       // Physical address
    input [7:0] kernel_base,  // Base address for kernel space
    input [7:0] user_base,    // Base address for user space
    output valid_access,      // Whether the access is valid
    output is_kernel_space,   // Whether the access is in kernel space
    // Interrupt signals
    output mem_fault         // Memory fault interrupt
);

    // Internal memory - 256 bytes for 8-bit address space
    reg [7:0] memory [0:255];

    // Initialize memory on reset
    integer i;
    initial begin
        for (i = 0; i < 256; i = i + 1) begin
            memory[i] = 8'b00000000;
        end
    end

    // Address translation (simplified for this implementation)
    // In a real system, this would involve page tables
    assign paddr = vaddr;  // For now, identity mapping

    // Determine if address is in kernel space
    assign is_kernel_space = (vaddr < user_base) ? 1'b1 : 1'b0;

    // Check if access is valid (not in restricted areas)
    assign valid_access = (vaddr >= kernel_base || !is_kernel_space) ? 1'b1 : 1'b0;

    // Memory fault if access is invalid
    assign mem_fault = !valid_access;

    // Data memory access
    always @(posedge clk) begin
        if (rst) begin
            // Initialize memory during reset if needed
        end
        else if (mem_write_enable && valid_access) begin
            memory[address] <= write_data;
        end
    end

    // Read data output
    assign read_data = (mem_read_enable && valid_access) ? memory[address] : 8'b00000000;

endmodule

// 8-bit Data Memory (simplified version)
module memory_8bit (
    input clk,
    input rst,
    input mem_write_enable,
    input [7:0] address,
    input [7:0] write_data,
    output [7:0] read_data
);

    reg [7:0] memory [0:255];

    // Initialize memory on reset
    integer i;
    initial begin
        for (i = 0; i < 256; i = i + 1) begin
            memory[i] = 8'b00000000;
        end
    end

    // Write operation
    always @(posedge clk) begin
        if (rst) begin
            // Initialize during reset
        end
        else if (mem_write_enable) begin
            memory[address] <= write_data;
        end
    end

    // Read operation (combinational)
    assign read_data = memory[address];

endmodule