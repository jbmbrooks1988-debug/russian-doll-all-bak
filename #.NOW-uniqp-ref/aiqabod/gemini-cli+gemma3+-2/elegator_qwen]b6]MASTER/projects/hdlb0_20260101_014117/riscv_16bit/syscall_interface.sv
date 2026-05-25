// System Call Interface for 16-bit RISC-V Processor
// Provides kernel interface for system calls
module syscall_interface (
    input clk,
    input rst,
    // Processor interface
    input [15:0] instruction,      // Current instruction
    input [15:0] rs1_data,         // Data from rs1 register
    input [15:0] rs2_data,         // Data from rs2 register
    input [4:0] rd_addr,           // Destination register address
    input ecall_enable,            // ECALL instruction detected
    // Kernel interface
    output [15:0] syscall_num,     // System call number
    output [15:0] syscall_arg1,    // First argument
    output [15:0] syscall_arg2,    // Second argument
    output [15:0] syscall_arg3,    // Third argument
    output [15:0] syscall_arg4,    // Fourth argument
    output syscall_request,        // System call request
    input syscall_complete,        // System call completion signal
    input [15:0] syscall_result,   // System call result
    output [15:0] write_data,      // Data to write to destination register
    output reg_write_enable        // Enable register write for result
);

    // Internal signals
    reg [15:0] syscall_num_reg;
    reg [15:0] syscall_arg1_reg, syscall_arg2_reg, syscall_arg3_reg, syscall_arg4_reg;
    reg syscall_request_reg;
    reg reg_write_enable_reg;
    reg [15:0] write_data_reg;
    
    // Detect ECALL instruction (typically opcode 1110011 for system calls)
    wire is_ecall = (instruction[6:0] == 7'b1110011) && (instruction[14:12] == 3'b000) && 
                    (instruction[31:25] == 7'b0000000) && (instruction[11:7] == 5'b00000);
    
    // System call number is typically in register a7 (x17) which would be rs1
    assign syscall_num = rs1_data;  // For ECALL, syscall number is in a7 (x17)
    assign syscall_arg1 = rs2_data; // First argument typically in a0 (x10)
    // Additional arguments would come from other registers (a1, a2, a3)
    
    // Process system call on ECALL
    always @(posedge clk) begin
        if (rst) begin
            syscall_request_reg <= 1'b0;
            reg_write_enable_reg <= 1'b0;
            write_data_reg <= 16'b0;
        end
        else if (ecall_enable || is_ecall) begin
            // Initiate system call
            syscall_num_reg <= rs1_data;
            syscall_arg1_reg <= rs2_data;  // This would be a0
            // In a real implementation, we'd read other argument registers
            syscall_request_reg <= 1'b1;
            reg_write_enable_reg <= 1'b0;  // Don't write until syscall completes
        end
        else if (syscall_complete && syscall_request_reg) begin
            // Complete system call
            syscall_request_reg <= 1'b0;
            write_data_reg <= syscall_result;
            reg_write_enable_reg <= 1'b1;  // Enable write of result
        end
        else begin
            reg_write_enable_reg <= 1'b0;
        end
    end
    
    // Assign outputs
    assign syscall_num = syscall_num_reg;
    assign syscall_arg1 = syscall_arg1_reg;
    assign syscall_arg2 = syscall_arg2_reg;
    assign syscall_arg3 = syscall_arg3_reg;
    assign syscall_arg4 = syscall_arg4_reg;
    assign syscall_request = syscall_request_reg;
    assign write_data = write_data_reg;
    assign reg_write_enable = reg_write_enable_reg;

endmodule

// Enhanced 16-bit processor with kernel compatibility
module riscv_16bit_processor_kernel_compat (
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
    // System call interface
    input [15:0] syscall_result,
    input syscall_complete,
    output [15:0] syscall_num,
    output [15:0] syscall_arg1,
    output [15:0] syscall_arg2,
    output [15:0] syscall_arg3,
    output [15:0] syscall_arg4,
    output syscall_request,
    // Status output
    output [15:0] pc_out
);

    // Internal signals
    wire [6:0] opcode, funct3;
    wire [6:0] funct7;
    wire [4:0] rs1, rs2, rd;
    wire [11:0] immediate;
    wire reg_write, mem_write_ctrl, mem_read_ctrl, branch, jump, pc_src;
    wire [2:0] alu_op;
    wire [1:0] alu_src, mem_to_reg;
    wire [15:0] pc_current, pc_next, next_pc_logic;
    wire [15:0] instruction_from_mem;
    wire [15:0] read_data_1, read_data_2;
    wire [15:0] alu_result;
    wire [15:0] write_data;
    wire [15:0] mem_read_data;
    wire zero_flag, overflow_flag;
    // Interrupt signals
    wire interrupt_req;
    wire [2:0] interrupt_cause;
    wire [15:0] interrupt_handler_addr;
    wire take_interrupt;
    wire [15:0] next_pc_after_interrupt;
    
    // System call signals
    wire ecall_enable;
    wire [15:0] syscall_write_data;
    wire syscall_reg_write;
    
    // Privilege and exception handling
    reg [1:0] privilege_level;  // 0=user, 1=supervisor, 2=reserved, 3=kernel
    reg [15:0] mepc, mtvec, mcause;  // Machine registers for exception handling

    // Program Counter
    program_counter pc (
        .clk(clk),
        .rst(rst),
        .enable(1'b1),  // Always enabled for now
        .next_pc(take_interrupt ? interrupt_handler_addr : next_pc_logic),
        .current_pc(pc_current)
    );

    // Instruction Fetch - simplified for this implementation
    assign instruction_from_mem = data_in;  // Simplified: instruction comes from data_in

    assign pc_out = pc_current;

    // Instruction Decoder
    instruction_decoder decoder (
        .instruction(instruction_from_mem),
        .opcode(opcode),
        .rs1(rs1),
        .rs2(rs2),
        .rd(rd),
        .funct3(funct3),
        .funct7(funct7),
        .immediate(immediate),
        .reg_write(reg_write),
        .mem_write(mem_write_ctrl),
        .mem_read(mem_read_ctrl),
        .branch(branch),
        .jump(jump),
        .alu_op(alu_op)
    );

    // Detect ECALL instruction
    assign ecall_enable = (instruction_from_mem[6:0] == 7'b1110011) && 
                          (instruction_from_mem[14:12] == 3'b000) && 
                          (instruction_from_mem[31:25] == 7'b0000000) && 
                          (instruction_from_mem[11:7] == 5'b00000);

    // Control Unit
    control_unit ctrl (
        .opcode(opcode),
        .reg_write(reg_write),
        .mem_write(mem_write_ctrl),
        .mem_read(mem_read_ctrl),
        .branch(branch),
        .jump(jump),
        .alu_op(alu_op),
        .alu_src(alu_src),
        .mem_to_reg(mem_to_reg),
        .pc_src(pc_src)
    );

    // Register File
    register_file_16bit reg_file (
        .clk(clk),
        .rst(rst),
        .reg_write_enable(syscall_reg_write ? 1'b1 : reg_write),  // Allow syscall to write
        .read_addr_1(rs1),
        .read_addr_2(rs2),
        .write_addr(syscall_reg_write ? 5'd10 : rd),  // Syscall typically writes to a0 (x10)
        .write_data(syscall_reg_write ? syscall_write_data : write_data),  // Use syscall result if available
        .read_data_1(read_data_1),
        .read_data_2(read_data_2)
    );

    // ALU
    alu_16bit alu (
        .operand_a(read_data_1),
        .operand_b(alu_src == 2'b01 ? {4'b0000, immediate[11:0]} :  // Use immediate for I-type
                   alu_src == 2'b10 ? 16'h0002 :  // PC+2 for jumps
                   read_data_2),  // Use rs2 as default
        .alu_op(alu_op),
        .result(alu_result),
        .zero_flag(zero_flag),
        .overflow_flag(overflow_flag)
    );

    // Memory Management Unit
    memory_management_unit mmu (
        .clk(clk),
        .rst(rst),
        .mem_write_enable(mem_write_ctrl),
        .mem_address(alu_result),  // Use ALU result as address for memory operations
        .mem_write_data(read_data_2),  // Write data from rs2 for store operations
        .mem_read_data(mem_read_data),
        .current_privilege_level({14'b00000000000000, privilege_level}),
        .kernel_base_address(16'h8000),  // Example kernel base address
        .user_base_address(16'h0000),    // Example user base address
        .valid_access(),  // Not used in this simplified version
        .page_fault(),    // Not used in this simplified version
        .ext_address(addr),
        .ext_read_data(data_in),
        .ext_write_data(data_out),
        .ext_write_enable(mem_write)
    );

    // System Call Interface
    syscall_interface syscall_inst (
        .clk(clk),
        .rst(rst),
        .instruction(instruction_from_mem),
        .rs1_data(read_data_1),
        .rs2_data(read_data_2),
        .rd_addr(rd),
        .ecall_enable(ecall_enable),
        .syscall_num(syscall_num),
        .syscall_arg1(syscall_arg1),
        .syscall_arg2(syscall_arg2),
        .syscall_arg3(syscall_arg3),
        .syscall_arg4(syscall_arg4),
        .syscall_request(syscall_request),
        .syscall_complete(syscall_complete),
        .syscall_result(syscall_result),
        .write_data(syscall_write_data),
        .reg_write_enable(syscall_reg_write)
    );

    // Select write data: ALU result for R-type/I-type, memory data for load operations, syscall result for ECALL
    assign write_data = (mem_to_reg == 2'b01) ? mem_read_data : 
                       (mem_to_reg == 2'b10) ? {14'b0, pc_current + 16'h0002} :  // PC+2 for jumps
                       alu_result;

    // Calculate next PC based on control signals
    wire [15:0] branch_target = pc_current + {4{immediate[11]}, immediate[11:0]};  // Sign-extended immediate
    assign next_pc_logic = jump ? (rs1 == 5'b0 ? {4{immediate[11]}, immediate[11:0]} : read_data_1 + {4{immediate[11]}, immediate[11:0]}) :  // Jump target
                           branch & zero_flag ? branch_target :  // Branch if zero
                           pc_current + 16'h0002;  // Default increment by 2 (16-bit instructions)

    // Connect memory interface
    assign mem_read = mem_read_ctrl;

    // Interrupt Controller
    interrupt_handling_unit int_ctrl (
        .clk(clk),
        .rst(rst),
        .timer_interrupt(timer_interrupt),
        .external_interrupt(external_interrupt),
        .software_interrupt(software_interrupt),
        .mstatus_mie(16'hFFFF),  // All interrupts enabled for now
        .mepc(mepc),
        .mtvec(mtvec),
        .mcause(mcause),
        .interrupt_req(interrupt_req),
        .interrupt_cause(interrupt_cause),
        .interrupt_handler_addr(interrupt_handler_addr),
        .take_interrupt(take_interrupt)
    );

    // Interrupt handling
    assign next_pc_after_interrupt = interrupt_handler_addr;

    // Exception and privilege handling
    always @(posedge clk) begin
        if (rst) begin
            privilege_level <= 2'b11;  // Start in kernel mode
            mepc <= 16'b0;
            mtvec <= 16'h0004;  // Default trap vector
            mcause <= 16'b0;
        end
        else if (take_interrupt) begin
            // Save current PC to mepc
            mepc <= pc_current;
            // Update cause register
            mcause <= {13'b0, interrupt_cause};
            // Privilege level may change during exception handling
        end
    end

endmodule