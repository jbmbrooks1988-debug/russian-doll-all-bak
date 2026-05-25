// Memory Management Unit for 16-bit RISC-V processor
// Handles memory access and protection for kernel compatibility
module memory_management_unit (
    input clk,
    input rst,
    // Data memory interface
    input mem_write_enable,
    input [15:0] mem_address,
    input [15:0] mem_write_data,
    output [15:0] mem_read_data,
    // Control signals
    input [15:0] current_privilege_level,  // 0=user, 1=supervisor, 2=reserved, 3=kernel
    input [15:0] kernel_base_address,      // Base address of kernel space
    input [15:0] user_base_address,        // Base address of user space
    // Status
    output valid_access,
    output page_fault,
    // Memory interface to external memory
    output [15:0] ext_address,
    input [15:0] ext_read_data,
    output [15:0] ext_write_data,
    output ext_write_enable
);

    // Internal signals
    reg [15:0] memory [0:65535];  // 64KB of memory (16-bit address space)
    reg [15:0] read_data_reg;
    
    // Initialize memory on reset
    integer i;
    initial begin
        if (rst) begin
            for (i = 0; i < 65536; i = i + 1) begin
                memory[i] = 16'h0000;
            end
        end
    end

    // Check if access is valid based on privilege level
    wire is_kernel_access = (mem_address >= kernel_base_address);
    wire is_user_access = (mem_address >= user_base_address && mem_address < kernel_base_address);
    wire access_allowed = (current_privilege_level == 16'h0003) ||  // Kernel mode can access all
                          (current_privilege_level == 16'h0001 && is_user_access);  // Supervisor can access user

    // Assign outputs
    assign valid_access = access_allowed;
    assign page_fault = ~access_allowed & (mem_write_enable | 1'b1);  // Simplified page fault detection
    
    // Memory operations
    always @(posedge clk) begin
        if (rst) begin
            read_data_reg <= 16'b0;
        end
        else if (mem_write_enable && valid_access) begin
            // Write operation
            memory[mem_address] <= mem_write_data;
            read_data_reg <= mem_write_data;  // For read-after-write
        end
        else if (valid_access) begin
            // Read operation
            read_data_reg <= memory[mem_address];
        end
        else begin
            // Invalid access - return 0 or handle fault
            read_data_reg <= 16'b0;
        end
    end

    assign mem_read_data = read_data_reg;
    
    // External memory interface (pass-through in this simplified implementation)
    assign ext_address = mem_address;
    assign ext_write_data = mem_write_data;
    assign ext_write_enable = mem_write_enable & valid_access;

endmodule