// Testbench for Control Unit
module control_unit_tb;

    reg [1:0] opcode;
    reg [1:0] funct3;
    wire reg_write;
    wire mem_write;
    wire mem_read;
    wire [1:0] alu_op;
    wire pc_enable;

    // Instantiate the control unit
    control_unit uut (
        .opcode(opcode),
        .funct3(funct3),
        .reg_write(reg_write),
        .mem_write(mem_write),
        .mem_read(mem_read),
        .alu_op(alu_op),
        .pc_enable(pc_enable)
    );

    // Test sequence
    initial begin
        // Test ADD instruction (R-type)
        opcode = 2'b00;
        funct3 = 2'b00;
        #10;
        $display("Time %0t: ADD instruction - reg_write=%b, mem_write=%b, mem_read=%b, alu_op=%b, pc_enable=%b", 
                 $time, reg_write, mem_write, mem_read, alu_op, pc_enable);
        
        // Test SUB instruction (R-type)
        opcode = 2'b00;
        funct3 = 2'b01;
        #10;
        $display("Time %0t: SUB instruction - reg_write=%b, mem_write=%b, mem_read=%b, alu_op=%b, pc_enable=%b", 
                 $time, reg_write, mem_write, mem_read, alu_op, pc_enable);
        
        // Test AND instruction (R-type)
        opcode = 2'b01;
        funct3 = 2'b00;
        #10;
        $display("Time %0t: AND instruction - reg_write=%b, mem_write=%b, mem_read=%b, alu_op=%b, pc_enable=%b", 
                 $time, reg_write, mem_write, mem_read, alu_op, pc_enable);
        
        // Test OR instruction (R-type)
        opcode = 2'b01;
        funct3 = 2'b01;
        #10;
        $display("Time %0t: OR instruction - reg_write=%b, mem_write=%b, mem_read=%b, alu_op=%b, pc_enable=%b", 
                 $time, reg_write, mem_write, mem_read, alu_op, pc_enable);
        
        // Test XOR instruction (R-type)
        opcode = 2'b01;
        funct3 = 2'b10;
        #10;
        $display("Time %0t: XOR instruction - reg_write=%b, mem_write=%b, mem_read=%b, alu_op=%b, pc_enable=%b", 
                 $time, reg_write, mem_write, mem_read, alu_op, pc_enable);
        
        // Test ADDI instruction (I-type)
        opcode = 2'b10;
        funct3 = 2'b00;  // Not used for I-type, but set to something
        #10;
        $display("Time %0t: ADDI instruction - reg_write=%b, mem_write=%b, mem_read=%b, alu_op=%b, pc_enable=%b", 
                 $time, reg_write, mem_write, mem_read, alu_op, pc_enable);
        
        // Test LW instruction (I-type)
        opcode = 2'b11;
        funct3 = 2'b00;
        #10;
        $display("Time %0t: LW instruction - reg_write=%b, mem_write=%b, mem_read=%b, alu_op=%b, pc_enable=%b", 
                 $time, reg_write, mem_write, mem_read, alu_op, pc_enable);
        
        // Test SW instruction (S-type)
        opcode = 2'b11;
        funct3 = 2'b01;
        #10;
        $display("Time %0t: SW instruction - reg_write=%b, mem_write=%b, mem_read=%b, alu_op=%b, pc_enable=%b", 
                 $time, reg_write, mem_write, mem_read, alu_op, pc_enable);
        
        // End simulation
        #20;
        $finish;
    end

endmodule