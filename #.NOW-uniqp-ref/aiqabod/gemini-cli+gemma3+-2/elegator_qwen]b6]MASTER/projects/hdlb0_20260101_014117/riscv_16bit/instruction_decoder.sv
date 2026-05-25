// Instruction Decoder for 16-bit RISC-V processor
// Decodes 16-bit RISC-V instructions and extracts fields
module instruction_decoder (
    input [15:0] instruction,
    output [6:0] opcode,
    output [4:0] rs1,
    output [4:0] rs2,
    output [4:0] rd,
    output [2:0] funct3,
    output [6:0] funct7,
    output [11:0] immediate,
    output reg_write,
    output mem_write,
    output mem_read,
    output branch,
    output jump,
    output [2:0] alu_op
);

    // Define internal signals for our 16-bit instruction format
    // We'll use a custom format that fits in 16 bits but is compatible with RISC-V concepts
    
    // For our 16-bit processor, we'll define a custom instruction format:
    // [15:13] - opcode (3 bits) - main operation type
    // [12:10] - rs1 (3 bits) - source register 1 (expanded to 5 bits with padding)
    // [9:7] - rs2 (3 bits) - source register 2 (expanded to 5 bits with padding)
    // [6:4] - rd (3 bits) - destination register (expanded to 5 bits with padding)
    // [3:1] - funct3 (3 bits) - sub-operation type
    // [0] - additional control bit
    
    // Extract fields from the 16-bit instruction
    wire [2:0] main_opcode = instruction[15:13];
    wire [2:0] reg_rs1 = instruction[12:10];
    wire [2:0] reg_rs2 = instruction[9:7];
    wire [2:0] reg_rd = instruction[6:4];
    wire [2:0] func3 = instruction[3:1];
    
    // Assign outputs with proper bit widths
    assign opcode = {4'b0001, main_opcode};  // 7-bit opcode (padded)
    assign rs1 = {2'b00, reg_rs1};           // 5-bit rs1 (padded)
    assign rs2 = {2'b00, reg_rs2};           // 5-bit rs2 (padded)
    assign rd = {2'b00, reg_rd};             // 5-bit rd (padded)
    assign funct3 = func3;                   // 3-bit funct3
    assign funct7 = 7'b0000000;              // 7-bit funct7 (set to 0 for now)
    assign immediate = {4'b0000, instruction}; // 12-bit immediate (padded)
    
    // Control signals based on main opcode
    always @(*) begin
        case (main_opcode)
            3'b000: begin  // R-type ALU operations
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 0;
                alu_op = func3;  // Use funct3 to determine ALU operation
            end
            3'b001: begin  // I-type operations (loads, immediate ops)
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;  // May be set based on specific instruction
                branch = 0;
                jump = 0;
                alu_op = func3;  // Use funct3 to determine ALU operation
            end
            3'b010: begin  // S-type store operations
                reg_write = 0;
                mem_write = 1;
                mem_read = 0;
                branch = 0;
                jump = 0;
                alu_op = 3'b000;  // ADD for address calculation
            end
            3'b011: begin  // B-type branch operations
                reg_write = 0;
                mem_write = 0;
                mem_read = 0;
                branch = 1;
                jump = 0;
                alu_op = 3'b001;  // SUB for comparison
            end
            3'b100: begin  // J-type jump operations
                reg_write = 1;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 1;
                alu_op = 3'b000;  // ADD for PC calculation
            end
            3'b101: begin  // Memory load operations
                reg_write = 1;
                mem_write = 0;
                mem_read = 1;
                branch = 0;
                jump = 0;
                alu_op = 3'b000;  // ADD for address calculation
            end
            default: begin  // Default: NOP
                reg_write = 0;
                mem_write = 0;
                mem_read = 0;
                branch = 0;
                jump = 0;
                alu_op = 3'b000;
            end
        endcase
    end

endmodule