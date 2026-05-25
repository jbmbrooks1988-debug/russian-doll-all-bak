// OR gate implemented using NAND gates
// Using De Morgan's law: A OR B = NOT((NOT A) AND (NOT B))
// Using NAND gates: OR gate = NAND(NAND(A,A), NAND(B,B))
module or_gate(
    input a,
    input b,
    output y
);
    wire not_a, not_b;
    
    // Create NOT A using NAND gate with inputs tied together
    nand_gate nand_not_a (.a(a), .b(a), .y(not_a));
    
    // Create NOT B using NAND gate with inputs tied together
    nand_gate nand_not_b (.a(b), .b(b), .y(not_b));
    
    // NAND the inverted inputs to get OR result
    nand_gate nand_final (.a(not_a), .b(not_b), .y(y));
endmodule