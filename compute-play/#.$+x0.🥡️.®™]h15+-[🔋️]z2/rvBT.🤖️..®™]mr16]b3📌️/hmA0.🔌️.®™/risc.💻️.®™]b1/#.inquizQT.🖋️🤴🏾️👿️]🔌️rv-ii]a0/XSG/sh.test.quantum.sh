#!/bin/bash

# Create netlist_quantum.txt with corrected signal names and quantum.+x
cat > netlist/netlist_quantum.txt << EOL
./+x/quantum.+x toffoli INPUT_A INPUT_B INPUT_C OUTPUT_A OUTPUT_B OUTPUT_C
./+x/quantum.+x hadamard ANCILLA_1 TMP_ANCILLA_1
./+x/quantum.+x cnot TMP_ANCILLA_1 ANCILLA_2 OUTPUT_QUBIT_1 OUTPUT_QUBIT_2
EOL

# Test Case 1: Toffoli (A=1, B=1, C=0) -> (A=1, B=1, C=1)
printf "1\n1\n0\n0\n0\n" > tmp/input.txt  # INPUT_A=1, INPUT_B=1, INPUT_C=0, ANCILLA_1=0, ANCILLA_2=0
./+x/main.+x netlist/netlist_quantum.txt test_output.txt 3
echo "Test 1 (Toffoli: A=1, B=1, C=0):"
cat tmp/OUTPUT_A.*.txt  # Expect: 1
cat tmp/OUTPUT_B.*.txt  # Expect: 1
cat tmp/OUTPUT_C.*.txt  # Expect: 1

# Test Case 2: Bell State (H on ANCILLA_1, CNOT on ANCILLA_1, ANCILLA_2)
printf "0\n0\n0\n0\n0\n" > tmp/input.txt  # INPUT_A=0, INPUT_B=0, INPUT_C=0, ANCILLA_1=0, ANCILLA_2=0
./+x/main.+x netlist/netlist_quantum.txt test_output.txt 3
echo "Test 2 (Bell State):"
cat tmp/OUTPUT_QUBIT_1.*.txt  # Expect: 0 or 1 (superposition, classical measurement)
cat tmp/OUTPUT_QUBIT_2.*.txt  # Expect: 0 or 1 (entangled)

# Clean up
rm -f tmp/*.txt test_output.txt
