// 8-bit to 2-bit Compatibility Module
// Ensures the 8-bit processor can run 2-bit compatible programs
module compatibility_module (
    // 2-bit compatibility interface
    input clk_2bit,
    input rst_2bit,
    output [1:0] pc_out_2bit,
    // 8-bit processor interface
    input clk_8bit,
    input rst_8bit,
    output [7:0] pc_out_8bit,
    // Compatibility control
    input mode_2bit,  // When 1, operate in 2-bit compatibility mode
    // Memory interface for both modes
    input [7:0] data_in,
    output [7:0] data_out_8bit,
    output [7:0] data_out_2bit,
    output [7:0] addr_8bit,
    output [1:0] addr_2bit,
    output mem_write_8bit,
    output mem_write_2bit,
    output mem_read_8bit,
    output mem_read_2bit
);

    // 2-bit processor instance (for compatibility)
    // This would be a simplified version of the 2-bit processor
    wire [1:0] pc_2bit_internal;
    wire [3:0] instruction_2bit;
    
    // 8-bit processor instance
    wire [7:0] pc_8bit_internal;
    wire [7:0] data_out_8bit_internal;
    wire [7:0] addr_8bit_internal;
    wire mem_write_8bit_internal;
    wire mem_read_8bit_internal;
    
    // 2-bit processor signals
    reg [1:0] pc_2bit_reg;
    reg [3:0] instr_mem_2bit [0:3];  // 4 instructions of 4 bits each
    
    // Initialize 2-bit instruction memory
    initial begin
        instr_mem_2bit[0] = 4'b0000;  // NOP
        instr_mem_2bit[1] = 4'b0000;  // NOP
        instr_mem_2bit[2] = 4'b0000;  // NOP
        instr_mem_2bit[3] = 4'b0000;  // NOP
    end
    
    // 2-bit processor simulation
    always @(posedge clk_2bit) begin
        if (rst_2bit) begin
            pc_2bit_reg <= 2'b00;
        end
        else begin
            pc_2bit_reg <= pc_2bit_reg + 1;
        end
    end
    
    assign pc_out_2bit = pc_2bit_reg;
    assign instruction_2bit = instr_mem_2bit[pc_2bit_reg];
    
    // 8-bit processor instance
    riscv_8bit_processor processor_8bit (
        .clk(clk_8bit),
        .rst(rst_8bit),
        .data_in(data_in),
        .data_out(data_out_8bit_internal),
        .addr(addr_8bit_internal),
        .mem_write(mem_write_8bit_internal),
        .mem_read(mem_read_8bit_internal),
        .timer_interrupt(1'b0),
        .keyboard_interrupt(1'b0),
        .external_interrupt(1'b0),
        .pc_out(pc_8bit_internal)
    );
    
    // Output assignment based on mode
    assign pc_out_8bit = pc_8bit_internal;
    
    // When in 2-bit mode, we use simplified addressing
    assign addr_2bit = mode_2bit ? pc_out_2bit : 2'b00;
    assign addr_8bit = mode_2bit ? {6'b000000, addr_2bit} : addr_8bit_internal;
    
    // Memory control signals
    assign mem_write_2bit = mode_2bit ? 1'b0 : 1'b0;  // 2-bit mode not fully implemented
    assign mem_read_2bit = mode_2bit ? 1'b1 : 1'b0;   // 2-bit mode not fully implemented
    assign mem_write_8bit = mode_2bit ? 1'b0 : mem_write_8bit_internal;
    assign mem_read_8bit = mode_2bit ? 1'b0 : mem_read_8bit_internal;
    
    // Data outputs
    assign data_out_2bit = mode_2bit ? {6'b000000, instruction_2bit} : 8'b00000000;
    assign data_out_8bit = mode_2bit ? data_out_2bit : data_out_8bit_internal;

endmodule

// Wrapper for the 8-bit processor that maintains 2-bit compatibility
module riscv_8bit_processor_with_compat (
    input clk,
    input rst,
    // Memory interface
    input [7:0] data_in,
    output [7:0] data_out,
    output [7:0] addr,
    output mem_write,
    output mem_read,
    // Interrupt lines
    input timer_interrupt,
    input keyboard_interrupt,
    input external_interrupt,
    // Compatibility mode
    input compat_mode_2bit,  // When high, operate with 2-bit compatibility
    // Status output
    output [7:0] pc_out
);

    // Internal signals
    wire [7:0] pc_8bit;
    wire [7:0] data_out_8bit;
    wire [7:0] addr_8bit;
    wire mem_write_8bit;
    wire mem_read_8bit;
    
    // 8-bit processor instance
    riscv_8bit_processor processor (
        .clk(clk),
        .rst(rst),
        .data_in(data_in),
        .data_out(data_out_8bit),
        .addr(addr_8bit),
        .mem_write(mem_write_8bit),
        .mem_read(mem_read_8bit),
        .timer_interrupt(timer_interrupt),
        .keyboard_interrupt(keyboard_interrupt),
        .external_interrupt(external_interrupt),
        .pc_out(pc_8bit)
    );
    
    // In compatibility mode, we might need to adapt certain behaviors
    // For now, just pass through the 8-bit processor signals
    assign pc_out = pc_8bit;
    assign data_out = data_out_8bit;
    assign addr = addr_8bit;
    assign mem_write = mem_write_8bit;
    assign mem_read = mem_read_8bit;

endmodule