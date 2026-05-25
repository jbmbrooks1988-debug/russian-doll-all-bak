// XOR gate implemented using NAND gates
// XOR = (A AND NOT B) OR (NOT A AND B)
// Using NAND gates, we need to implement this expression with multiple NAND gates
module xor_gate(
    input a,
    input b,
    output y
);
    wire not_a, not_b, a_and_not_b_nand, not_a_and_b_nand, a_and_not_b, not_a_and_b, or_input1, or_input2;

    // Create NOT A and NOT B
    nand_gate nand_not_a (.a(a), .b(a), .y(not_a));
    nand_gate nand_not_b (.a(b), .b(b), .y(not_b));

    // Create A NAND NOT B (first step for A AND NOT B)
    nand_gate nand_a_and_not_b (.a(a), .b(not_b), .y(a_and_not_b_nand));

    // Create NOT A NAND B (first step for NOT A AND B)
    nand_gate nand_not_a_and_b (.a(not_a), .b(b), .y(not_a_and_b_nand));

    // Invert the NAND outputs to get A AND NOT B and NOT A AND B
    nand_gate nand_a_and_not_b_inv (.a(a_and_not_b_nand), .b(a_and_not_b_nand), .y(a_and_not_b));
    nand_gate nand_not_a_and_b_inv (.a(not_a_and_b_nand), .b(not_a_and_b_nand), .y(not_a_and_b));

    // To implement OR using NAND gates: OR(X,Y) = NAND(NAND(X,X), NAND(Y,Y))
    nand_gate nand_or_input1 (.a(a_and_not_b), .b(a_and_not_b), .y(or_input1));
    nand_gate nand_or_input2 (.a(not_a_and_b), .b(not_a_and_b), .y(or_input2));
    nand_gate nand_final_or (.a(or_input1), .b(or_input2), .y(y));
endmodule