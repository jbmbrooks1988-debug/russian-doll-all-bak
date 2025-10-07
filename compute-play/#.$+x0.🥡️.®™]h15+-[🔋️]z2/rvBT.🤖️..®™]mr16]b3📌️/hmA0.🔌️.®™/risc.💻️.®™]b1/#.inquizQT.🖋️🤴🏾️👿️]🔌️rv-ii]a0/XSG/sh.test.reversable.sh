#!/bin/bash

# Create netlist_quantum.txt
cat > netlist/netlist_quantum.txt << EOL
./+x/quantum.+x toffoli INPUT_A INPUT_B INPUT_C OUTPUT_A OUTPUT_B OUTPUT_C
./+x/quantum.+x hadamard ANCILLA_1 TMP_ANCILLA_1
./+x/quantum.+x cnot TMP_ANCILLA_1 ANCILLA_2 OUTPUT_QUBIT_1 OUTPUT_QUBIT_2
EOL

# Test Case 1: Classical mode (2 cycles)
echo "Test 1: 2 cycles in classical mode"
./+x/orchestrator.+x netlist/netlist_quantum.txt test_output.txt 3 2
echo "Cycle 0 (Toffoli: A=1, B=1, C=0, CLOCK=0):"
cat tmp/OUTPUT_A.*.txt  # Expect: 1
cat tmp/OUTPUT_B.*.txt  # Expect: 1
cat tmp/OUTPUT_C.*.txt  # Expect: 1
cat tmp/CLOCK.*.txt     # Expect: 0
echo "Cycle 1 (Bell State, CLOCK=1):"
cat tmp/OUTPUT_QUBIT_1.*.txt  # Expect: 0 or 1
cat tmp/OUTPUT_QUBIT_2.*.txt  # Expect: 0 or 1
cat tmp/CLOCK.*.txt           # Expect: 1

# Test Case 2: Quantum mode (2 cycles)
echo "Test 2: 2 cycles in quantum mode"
./+x/orchestrator.+x netlist/netlist_quantum.txt test_output.txt 3 2 --quantum
echo "Cycle 0 (Toffoli, CLOCK=0):"
cat tmp/STATE_OUTPUT_A.*.txt  # Expect: 0.0 0.0\n1.0 0.0 (|1⟩)
cat tmp/STATE_OUTPUT_B.*.txt  # Expect: 0.0 0.0\n1.0 0.0 (|1⟩)
cat tmp/STATE_OUTPUT_C.*.txt  # Expect: 0.0 0.0\n1.0 0.0 (|1⟩)
cat tmp/OUTPUT_A.*.txt        # Expect: 0 (dummy)
cat tmp/CLOCK.*.txt           # Expect: 0
echo "Cycle 1 (Bell State, CLOCK=1):"
cat tmp/STATE_OUTPUT_QUBIT_1.*.txt  # Expect: 0.707 0.0\n0.707 0.0 ((|0⟩ + |1⟩)/√2)
cat tmp/STATE_OUTPUT_QUBIT_2.*.txt  # Expect: 0.707 0.0\n0.707 0.0 ((|0⟩ + |1⟩)/√2)
cat tmp/OUTPUT_QUBIT_1.*.txt        # Expect: 0 (dummy)
cat tmp/CLOCK.*.txt                 # Expect: 1
echo "Feedback Loop Test (Cycle 1 INPUT_A from Cycle 0 OUTPUT_A):"
cat tmp/INPUT_A.*.txt               # Expect: 1 (from STATE_OUTPUT_A)

# Clean up
rm -f tmp/*.txt test_output.txt
