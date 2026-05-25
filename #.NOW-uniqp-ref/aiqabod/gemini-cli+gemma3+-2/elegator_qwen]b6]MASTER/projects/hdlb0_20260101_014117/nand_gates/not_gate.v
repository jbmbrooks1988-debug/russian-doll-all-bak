// NOT gate implemented using NAND gate
// A NOT gate is created by connecting both inputs of a NAND gate together
module not_gate(
    input a,
    output y
);
    // Connect both inputs of NAND gate to the same input
    nand_gate nand_inst (.a(a), .b(a), .y(y));
endmodule