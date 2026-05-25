// NOR gate implemented using NAND gates
// A NOR gate is an OR gate followed by a NOT gate
// Since we already have an OR gate implementation using NAND gates, 
// we just need to add a NOT gate (which is a NAND gate with inputs tied together) at the output
module nor_gate(
    input a,
    input b,
    output y
);
    wire or_output;
    
    // OR gate using NAND gates (from our or_gate implementation)
    wire not_a, not_b;
    
    // Create NOT A using NAND gate with inputs tied together
    nand_gate nand_not_a (.a(a), .b(a), .y(not_a));
    
    // Create NOT B using NAND gate with inputs tied together
    nand_gate nand_not_b (.a(b), .b(b), .y(not_b));
    
    // NAND the inverted inputs to get OR result
    nand_gate nand_final_or (.a(not_a), .b(not_b), .y(or_output));
    
    // NOT gate to invert OR output (NOR = NOT(OR))
    nand_gate nand_not_or (.a(or_output), .b(or_output), .y(y));
endmodule