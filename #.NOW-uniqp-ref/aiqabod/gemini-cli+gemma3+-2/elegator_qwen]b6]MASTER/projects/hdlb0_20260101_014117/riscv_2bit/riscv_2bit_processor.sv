// Top-level 2-bit RISC-V Processor
module riscv_2bit_processor (
    input clk,
    input rst,
    output [1:0] pc_out
);

    // Internal signals
    wire [1:0] opcode, rs1, rs2, rd, funct3, immediate;
    wire reg_write, mem_write, mem_read, pc_enable;
    wire [1:0] alu_op;
    wire [1:0] pc_current, pc_next;
    wire [3:0] instruction_from_mem;
    wire [1:0] read_data_1, read_data_2;
    wire [1:0] alu_result;
    wire [1:0] write_data;
    wire [1:0] mem_read_data;
    wire zero_flag;

    // Program Counter
    program_counter pc (
        .clk(clk),
        .rst(rst),
        .enable(pc_enable),
        .next_pc(pc_next),
        .current_pc(pc_current)
    );

    // Instruction Memory (4 instructions, 4 bits each)
    // Since our memory is 2-bit wide and instruction is 4-bit, we need two memory modules
    memory_2bit instr_mem_lower (
        .clk(clk),
        .rst(rst),
        .mem_write_enable(1'b0),  // Instructions are read-only
        .address(pc_current),  // Use PC as address
        .write_data(2'b00),  // Not used for read-only
        .read_data(instruction_from_mem[1:0])  // Lower 2 bits
    );

    // A second memory module for upper 2 bits of instruction
    memory_2bit instr_mem_upper (
        .clk(clk),
        .rst(rst),
        .mem_write_enable(1'b0),  // Instructions are read-only
        .address(pc_current),  // Use PC as address
        .write_data(2'b00),  // Not used for read-only
        .read_data(instruction_from_mem[3:2])  // Upper 2 bits
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
        .immediate(immediate),
        .reg_write(reg_write),
        .mem_write(mem_write),
        .mem_read(mem_read),
        .alu_op(alu_op)
    );

    // Control Unit
    control_unit ctrl (
        .opcode(opcode),
        .instruction_lower(instruction_from_mem[1:0]),  // Lower 2 bits of instruction
        .reg_write(reg_write),
        .mem_write(mem_write),
        .mem_read(mem_read),
        .alu_op(alu_op),
        .pc_enable(pc_enable)
    );

    // Register File
    register_file_2bit reg_file (
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
    alu_2bit alu (
        .operand_a(read_data_1),
        .operand_b((opcode == 2'b10) ? {immediate} : read_data_2),  // Use immediate for I-type
        .alu_op(alu_op),
        .result(alu_result),
        .zero_flag(zero_flag)
    );

    // Data Memory
    memory_2bit data_mem (
        .clk(clk),
        .rst(rst),
        .mem_write_enable(mem_write),
        .address(alu_result),  // Use ALU result as address for memory operations
        .write_data(read_data_2),  // Write data from rs2 for store operations
        .read_data(mem_read_data)
    );

    // Select write data: ALU result for R-type/I-type, memory data for load operations
    assign write_data = mem_read ? mem_read_data : alu_result;

    // Calculate next PC (simplified - just increment for now)
    assign pc_next = pc_current + 1;

endmodule