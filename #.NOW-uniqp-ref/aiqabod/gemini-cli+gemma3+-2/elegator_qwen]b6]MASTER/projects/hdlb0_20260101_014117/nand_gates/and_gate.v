// AND gate implemented using NAND gates
// An AND gate is created by using a NAND gate followed by a NOT gate (which is also a NAND gate with inputs tied together)
module and_gate(
    input a,
    input b,
    output y
);
    wire nand_out;
    
    // First NAND gate
    nand_gate nand1 (.a(a), .b(b), .y(nand_out));
    
    // NOT gate (using NAND gate with inputs tied together) to invert the output
    nand_gate nand2 (.a(nand_out), .b(nand_out), .y(y));
endmodule