// Top-level 16-bit RISC-V Processor
// Integrates all components of the 16-bit RISC-V processor
module riscv_16bit_processor (
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
    // In a real implementation, this would interface with instruction memory
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
        .reg_write_enable(reg_write),
        .read_addr_1(rs1),
        .read_addr_2(rs2),
        .write_addr(rd),
        .write_data(write_data),
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

    // Select write data: ALU result for R-type/I-type, memory data for load operations
    assign write_data = (mem_to_reg == 2'b01) ? mem_read_data : 
                       (mem_to_reg == 2'b10) ? {14'b0, pc_current + 16'h0002} :  // PC+2 for jumps
                       alu_result;

    // Calculate next PC based on control signals
    wire [15:0] branch_target = pc_current + {4{immediate[11]}, immediate[11:0]};  // Sign-extended immediate
    assign next_pc_logic = jump ? (rs1 == 5'b0 ? {4{immediate[11]}, immediate[11:0]} : read_data_1 + {4{immediate[11]}, immediate[11:0]}) :  // Jump target
                           branch & zero_flag ? branch_target :  // Branch if zero
                           pc_current + 16'h0002;  // Default increment by 2 (16-bit instructions)

    // Connect memory interface
    // addr and data_out are connected through MMU
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
    // The take_interrupt signal is already assigned from int_ctrl
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