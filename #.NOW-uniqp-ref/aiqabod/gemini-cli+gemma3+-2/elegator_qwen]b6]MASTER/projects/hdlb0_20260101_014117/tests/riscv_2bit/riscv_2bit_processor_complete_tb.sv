// Testbench for 2-bit RISC-V Processor with a complete test program
module riscv_2bit_processor_complete_tb;

    reg clk;
    reg rst;
    wire [1:0] pc_out;

    // Memory to initialize instruction memory
    reg [1:0] instr_mem_lower_init [0:3];
    reg [1:0] instr_mem_upper_init [0:3];
    
    // Internal signals to access instruction memory
    reg [1:0] init_addr;
    reg [1:0] init_data_lower;
    reg [1:0] init_data_upper;
    reg init_enable;
    
    // Instantiate the processor
    riscv_2bit_processor uut (
        .clk(clk),
        .rst(rst),
        .pc_out(pc_out)
    );

    // Clock generation
    initial begin
        clk = 0;
        forever #5 clk = ~clk;  // 10 time unit clock period
    end

    // Initialize instruction memory with test program
    initial begin
        // Initialize instruction memory with a simple test program
        // This program will:
        // 1. ADDI R1, R0, 1  (R1 = R0 + 1 = 0 + 1 = 1) - instruction: 10 01 -> 0x9
        // 2. ADDI R2, R0, 2  (R2 = R0 + 2 = 0 + 2 = 2) - instruction: 10 10 -> 0xA
        // 3. ADD  R3, R1, R2 (R3 = R1 + R2 = 1 + 2 = 3) - instruction: 00 11 -> 0x3 (simplified)
        // 4. SW   R3, 0(R1)  (Store R3 at address R1+0)  - instruction: 11 01 -> 0xD
        
        // Instruction format: [3:2] opcode | [1:0] data
        // 00: ADD (simplified - for this example, we'll use destination register in lower bits)
        // 01: AND (not used in this example)
        // 10: ADDI
        // 11: Memory operations (00=LW, 01=SW)
        
        // Instruction 0: ADDI R1, R0, 1  (instruction: 10 01)
        instr_mem_upper_init[0] = 2'b10;  // opcode = 10 (ADDI)
        instr_mem_lower_init[0] = 2'b01;  // data = 01 (R1 destination)
        
        // Instruction 1: ADDI R2, R0, 2  (instruction: 10 10)
        instr_mem_upper_init[1] = 2'b10;  // opcode = 10 (ADDI)
        instr_mem_lower_init[1] = 2'b10;  // data = 10 (R2 destination)
        
        // Instruction 2: ADD R3, R1, R2  (instruction: 00 11) - simplified
        instr_mem_upper_init[2] = 2'b00;  // opcode = 00 (ADD)
        instr_mem_lower_init[2] = 2'b11;  // data = 11 (R3 destination)
        
        // Instruction 3: SW R3, 0(R1)    (instruction: 11 01)
        instr_mem_upper_init[3] = 2'b11;  // opcode = 11 (Store)
        instr_mem_lower_init[3] = 2'b01;  // data = 01 (SW operation)
        
        rst = 1;
        #20;
        rst = 0;
        #20;
        $display("Time %0t: After reset, PC = %b", $time, pc_out);
        
        // Run for several clock cycles to execute the test program
        #400;
        $display("Time %0t: PC after executing test program = %b", $time, pc_out);
        
        // End simulation
        #50;
        $finish;
    end

endmodule