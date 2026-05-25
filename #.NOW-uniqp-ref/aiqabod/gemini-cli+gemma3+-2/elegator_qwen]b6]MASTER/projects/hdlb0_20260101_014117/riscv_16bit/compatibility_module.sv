// 16-bit to 2-bit and 8-bit Compatibility Module
// Ensures the 16-bit processor can run 2-bit and 8-bit compatible programs
module compatibility_module (
    // 2-bit compatibility interface
    input clk_2bit,
    input rst_2bit,
    output [1:0] pc_out_2bit,
    // 8-bit compatibility interface
    input clk_8bit,
    input rst_8bit,
    output [7:0] pc_out_8bit,
    // 16-bit processor interface
    input clk_16bit,
    input rst_16bit,
    output [15:0] pc_out_16bit,
    // Compatibility control
    input mode_2bit,  // When 1, operate in 2-bit compatibility mode
    input mode_8bit,  // When 1, operate in 8-bit compatibility mode
    // Memory interface for all modes
    input [15:0] data_in,
    output [15:0] data_out_16bit,
    output [7:0] data_out_8bit,
    output [1:0] data_out_2bit,
    output [15:0] addr_16bit,
    output [7:0] addr_8bit,
    output [1:0] addr_2bit,
    output mem_write_16bit,
    output mem_write_8bit,
    output mem_write_2bit,
    output mem_read_16bit,
    output mem_read_8bit,
    output mem_read_2bit
);

    // Internal signals for compatibility modes
    reg [1:0] pc_2bit_internal;
    reg [7:0] pc_8bit_internal;
    reg [15:0] pc_16bit_internal;
    
    // 2-bit compatibility mode
    always @(posedge clk_2bit) begin
        if (rst_2bit) begin
            pc_2bit_internal <= 2'b00;
        end
        else if (mode_2bit) begin
            pc_2bit_internal <= pc_2bit_internal + 1;
        end
    end
    
    // 8-bit compatibility mode
    always @(posedge clk_8bit) begin
        if (rst_8bit) begin
            pc_8bit_internal <= 8'b00000000;
        end
        else if (mode_8bit) begin
            pc_8bit_internal <= pc_8bit_internal + 1;
        end
    end
    
    // 16-bit mode
    always @(posedge clk_16bit) begin
        if (rst_16bit) begin
            pc_16bit_internal <= 16'b0000000000000000;
        end
        else if (!mode_2bit && !mode_8bit) begin
            pc_16bit_internal <= pc_16bit_internal + 1;
        end
    end
    
    // Output assignments
    assign pc_out_2bit = pc_2bit_internal;
    assign pc_out_8bit = pc_8bit_internal;
    assign pc_out_16bit = pc_16bit_internal;
    
    // Address mapping for compatibility
    assign addr_2bit = mode_2bit ? pc_out_2bit : 2'b00;
    assign addr_8bit = mode_8bit ? {1'b0, pc_out_8bit[7:1]} : 8'b00000000;  // Map to lower 8-bit space
    assign addr_16bit = mode_2bit ? {14'b0, addr_2bit} : 
                       mode_8bit ? {8'b0, addr_8bit} : 
                       pc_16bit_internal;  // Normal 16-bit addressing
    
    // Memory control signals
    assign mem_write_2bit = mode_2bit ? 1'b0 : 1'b0;  // 2-bit mode not fully implemented
    assign mem_read_2bit = mode_2bit ? 1'b1 : 1'b0;   // 2-bit mode not fully implemented
    assign mem_write_8bit = mode_8bit ? 1'b0 : 1'b0;  // 8-bit mode not fully implemented
    assign mem_read_8bit = mode_8bit ? 1'b1 : 1'b0;   // 8-bit mode not fully implemented
    assign mem_write_16bit = mode_2bit ? 1'b0 : 
                            mode_8bit ? 1'b0 : 
                            1'b1;  // Normal 16-bit operation
    assign mem_read_16bit = mode_2bit ? 1'b0 : 
                           mode_8bit ? 1'b0 : 
                           1'b1;  // Normal 16-bit operation
    
    // Data outputs with compatibility mapping
    assign data_out_2bit = mode_2bit ? {6'b000000, data_in[1:0]} : 8'b00000000;
    assign data_out_8bit = mode_8bit ? {1'b0, data_in[7:1]} : 8'b00000000;
    assign data_out_16bit = mode_2bit ? {14'b00000000000000, data_in[1:0]} :
                           mode_8bit ? {8'b00000000, data_in[7:0]} :
                           data_in;  // Normal 16-bit data

endmodule

// Wrapper for the 16-bit processor that maintains 2-bit and 8-bit compatibility
module riscv_16bit_processor_with_compat (
    input clk,
    input rst,
    // Memory interface
    input [15:0] data_in,
    output [15:0] data_out,
    output [15:0] addr,
    output mem_write,
    output mem_read,
    // Interrupt lines
    input timer_interrupt,
    input external_interrupt,
    input software_interrupt,
    // Compatibility mode
    input compat_mode_2bit,  // When high, operate with 2-bit compatibility
    input compat_mode_8bit,  // When high, operate with 8-bit compatibility
    // Status output
    output [15:0] pc_out
);

    // Internal signals
    wire [15:0] pc_16bit;
    wire [15:0] data_out_16bit;
    wire [15:0] addr_16bit;
    wire mem_write_16bit;
    wire mem_read_16bit;

    // Compatibility signals
    wire [1:0] pc_2bit;
    wire [7:0] pc_8bit;
    wire [7:0] data_out_8bit;
    wire [1:0] data_out_2bit;
    wire [7:0] addr_8bit;
    wire [1:0] addr_2bit;
    wire mem_write_8bit;
    wire mem_read_8bit;
    wire mem_write_2bit;
    wire mem_read_2bit;

    // Compatibility module instance
    compatibility_module compat (
        .clk_2bit(clk),
        .rst_2bit(rst),
        .pc_out_2bit(pc_2bit),
        .clk_8bit(clk),
        .rst_8bit(rst),
        .pc_out_8bit(pc_8bit),
        .clk_16bit(clk),
        .rst_16bit(rst),
        .pc_out_16bit(pc_16bit),
        .mode_2bit(compat_mode_2bit),
        .mode_8bit(compat_mode_8bit),
        .data_in(data_in),
        .data_out_16bit(data_out_16bit),
        .data_out_8bit(data_out_8bit),
        .data_out_2bit(data_out_2bit),
        .addr_16bit(addr_16bit),
        .addr_8bit(addr_8bit),
        .addr_2bit(addr_2bit),
        .mem_write_16bit(mem_write_16bit),
        .mem_write_8bit(mem_write_8bit),
        .mem_write_2bit(mem_write_2bit),
        .mem_read_16bit(mem_read_16bit),
        .mem_read_8bit(mem_read_8bit),
        .mem_read_2bit(mem_read_2bit)
    );

    // 16-bit processor instance
    riscv_16bit_processor processor (
        .clk(clk),
        .rst(rst),
        .data_in(data_in),
        .data_out(data_out_16bit),
        .addr(addr_16bit),
        .mem_write(mem_write_16bit),
        .mem_read(mem_read_16bit),
        .timer_interrupt(timer_interrupt),
        .external_interrupt(external_interrupt),
        .software_interrupt(software_interrupt),
        .pc_out(pc_16bit)
    );

    // In compatibility mode, we might need to adapt certain behaviors
    // For now, just pass through the appropriate processor signals based on mode
    assign pc_out = compat_mode_2bit ? {14'b0, pc_2bit} :
                   compat_mode_8bit ? {8'b0, pc_8bit} :
                   pc_16bit;
    assign data_out = compat_mode_2bit ? {14'b0, data_out_2bit} :
                     compat_mode_8bit ? {8'b0, data_out_8bit} :
                     data_out_16bit;
    assign addr = compat_mode_2bit ? {14'b0, addr_2bit} :
                 compat_mode_8bit ? {8'b0, addr_8bit} :
                 addr_16bit;
    assign mem_write = compat_mode_2bit ? mem_write_2bit :
                      compat_mode_8bit ? mem_write_8bit :
                      mem_write_16bit;
    assign mem_read = compat_mode_2bit ? mem_read_2bit :
                     compat_mode_8bit ? mem_read_8bit :
                     mem_read_16bit;

endmodule