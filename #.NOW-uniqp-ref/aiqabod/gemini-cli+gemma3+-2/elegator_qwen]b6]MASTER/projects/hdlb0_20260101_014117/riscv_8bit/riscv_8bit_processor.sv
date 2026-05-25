// Top-level 8-bit RISC-V Processor
// Integrates all components of the 8-bit RISC-V processor
module riscv_8bit_processor (
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
    // Status output
    output [7:0] pc_out
);

    // Internal signals
    wire [6:0] opcode, funct3;
    wire [6:0] funct7;
    wire [2:0] rs1, rs2, rd;
    wire [11:0] immediate;
    wire reg_write, mem_write_ctrl, mem_read_ctrl, branch, jump, pc_src;
    wire [2:0] alu_op;
    wire [1:0] alu_src, mem_to_reg;
    wire [7:0] pc_current, pc_next, next_pc_logic;
    wire [15:0] instruction_from_mem;
    wire [7:0] read_data_1, read_data_2;
    wire [7:0] alu_result;
    wire [7:0] write_data;
    wire [7:0] mem_read_data;
    wire zero_flag;
    // Interrupt signals
    wire interrupt_req;
    wire [2:0] interrupt_cause;
    wire [7:0] interrupt_handler_addr;
    wire take_interrupt;
    wire [7:0] next_pc_after_interrupt;

    // Program Counter
    program_counter pc (
        .clk(clk),
        .rst(rst),
        .enable(1'b1),  // Always enabled for now
        .next_pc(take_interrupt ? interrupt_handler_addr : next_pc_logic),
        .current_pc(pc_current)
    );

    // Instruction Fetch
    instruction_fetch_unit if_unit (
        .clk(clk),
        .rst(rst),
        .pc(pc_current),
        .instruction(instruction_from_mem),
        .next_pc(next_pc_logic)
    );

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
        .pc_src(pc_src),
        .alu_op(alu_op),
        .alu_src(alu_src),
        .mem_to_reg(mem_to_reg)
    );

    // Register File
    register_file_8bit reg_file (
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
    alu_8bit alu (
        .operand_a(read_data_1),
        .operand_b(alu_src == 2'b01 ? {4'b0000, immediate[7:0]} : read_data_2),  // Use immediate for I-type
        .alu_op(alu_op),
        .result(alu_result),
        .zero_flag(zero_flag)
    );

    // Data Memory
    memory_8bit data_mem (
        .clk(clk),
        .rst(rst),
        .mem_write_enable(mem_write_ctrl),
        .address(alu_result),  // Use ALU result as address for memory operations
        .write_data(read_data_2),  // Write data from rs2 for store operations
        .read_data(mem_read_data)
    );

    // Select write data: ALU result for R-type/I-type, memory data for load operations
    assign write_data = (mem_to_reg == 2'b01) ? mem_read_data : alu_result;

    // Calculate next PC based on control signals
    wire [7:0] branch_target = pc_current + {4{immediate[7]}, immediate[7:0]};  // Sign-extended immediate
    assign next_pc_logic = jump ? {4{immediate[7]}, immediate[7:0]} :  // Jump target
                           branch & zero_flag ? branch_target :  // Branch if zero
                           pc_next;  // Default increment

    // Connect memory interface
    assign addr = alu_result;
    assign data_out = read_data_2;
    assign mem_write = mem_write_ctrl;
    assign mem_read = mem_read_ctrl;

    // Interrupt Controller
    interrupt_controller int_ctrl (
        .clk(clk),
        .rst(rst),
        .timer_irq(timer_interrupt),
        .keyboard_irq(keyboard_interrupt),
        .external_irq(external_interrupt),
        .syscall_irq(1'b0),  // System call interrupt would come from instruction decoder
        .mstatus_mie(1'b1),  // Interrupts enabled
        .interrupt_req(interrupt_req),
        .interrupt_cause(interrupt_cause),
        .interrupt_handler_addr(interrupt_handler_addr)
    );

    // Interrupt handling
    assign take_interrupt = interrupt_req;
    assign next_pc_after_interrupt = interrupt_handler_addr;

endmodule