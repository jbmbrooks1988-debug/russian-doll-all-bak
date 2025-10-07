#!/bin/bash

# Create netlist_quantum.txt
cat > netlist/netlist_quantum.txt << EOL
./+x/quantum.+x toffoli INPUT_A INPUT_B INPUT_C OUTPUT_A OUTPUT_B OUTPUT_C
./+x/quantum.+x hadamard ANCILLA_1 TMP_ANCILLA_1
./+x/quantum.+x cnot TMP_ANCILLA_1 ANCILLA_2 OUTPUT_QUBIT_1 OUTPUT_QUBIT_2
./+x/quantum.+x t ANCILLA_1 OUTPUT_T
EOL

# Test Case 1: Classical mode (2 cycles)
echo "Test 1: 2 cycles in classical mode"
./+x/orchestrator.+x netlist/netlist_quantum.txt test_output.txt 4 2
echo "Cycle 0 (Toffoli: A=1, B=1, C=0, CLOCK=0):"
cat tmp/OUTPUT_A.*.txt  # Expect: 1
cat tmp/OUTPUT_B.*.txt  # Expect: 1
cat tmp/OUTPUT_C.*.txt  # Expect: 1
cat tmp/OUTPUT_T.*.txt  # Expect: 0
cat tmp/CLOCK.*.txt     # Expect: 0
echo "Cycle 1 (Bell State, CLOCK=1):"
cat tmp/OUTPUT_QUBIT_1.*.txt  # Expect: 0 or 1
cat tmp/OUTPUT_QUBIT_2.*.txt  # Expect: 0 or 1
cat tmp/OUTPUT_T.*.txt        # Expect: 0
cat tmp/CLOCK.*.txt           # Expect: 1

# Test Case 2: Quantum mode (2 cycles)
echo "Test 2: 2 cycles in quantum mode"
./+x/orchestrator.+x netlist/netlist_quantum.txt test_output.txt 4 2 --quantum
echo "Cycle 0 (Toffoli, CLOCK=0):"
cat tmp/STATE_OUTPUT_A.*.txt  # Expect: 0.0 0.0\n1.0 0.0 (|1⟩)
cat tmp/STATE_OUTPUT_B.*.txt  # Expect: 0.0 0.0\n1.0 0.0 (|1⟩)
cat tmp/STATE_OUTPUT_C.*.txt  # Expect: 0.0 0.0\n1.0 0.0 (|1⟩)
cat tmp/STATE_OUTPUT_T.*.txt  # Expect: 1.0 0.0\n0.0 0.0 (|0⟩)
cat tmp/OUTPUT_A.*.txt        # Expect: 0 (dummy)
cat tmp/OUTPUT_T.*.txt        # Expect: 0 (dummy)
cat tmp/CLOCK.*.txt           # Expect: 0
echo "Cycle 1 (Bell State, CLOCK=1):"
cat tmp/STATE_OUTPUT_QUBIT_1.*.txt  # Expect: 0.707 0.0\n0.707 0.0 ((|0⟩ + |1⟩)/√2)
cat tmp/STATE_OUTPUT_QUBIT_2.*.txt  # Expect: 0.707 0.0\n0.707 0.0 ((|0⟩ + |1⟩)/√2)
cat tmp/STATE_OUTPUT_T.*.txt        # Expect: 1.0 0.0\n0.0 0.0 (|0⟩)
cat tmp/OUTPUT_QUBIT_1.*.txt        # Expect: 0 (dummy)
cat tmp/OUTPUT_T.*.txt              # Expect: 0 (dummy)
cat tmp/CLOCK.*.txt                 # Expect: 1
echo "Feedback Loop Test (Cycle 1 INPUT_A from Cycle 0 OUTPUT_A):"
cat tmp/INPUT_A.*.txt               # Expect: 1 (from STATE_OUTPUT_A)

# Test Case 3: T gate reversibility in quantum mode
echo "Test 3: T gate reversibility in quantum mode"
cat > netlist/netlist_t_reversibility.txt << EOL
./+x/quantum.+x t ANCILLA_1 TMP_T
./+x/quantum.+x t_dagger TMP_T OUTPUT_T
EOL
./+x/orchestrator.+x netlist/netlist_t_reversibility.txt test_output.txt 2 1 --quantum
echo "T then T_dagger (ANCILLA_1=1):"
cat tmp/STATE_OUTPUT_T.*.txt  # Expect: 0.0 0.0\n1.0 0.0 (|1⟩)
cat tmp/OUTPUT_T.*.txt        # Expect: 0 (dummy)

# Test Case 4: Reverse mode in quantum mode (1 cycle)
echo "Test 4: Reverse mode in quantum mode"
./+x/orchestrator.+x netlist/netlist_quantum.txt test_output.txt 4 1 --quantum --reverse
echo "Reverse Toffoli (OUTPUT_A=1, OUTPUT_B=1, OUTPUT_C=1):"
cat tmp/STATE_INPUT_A.*.txt  # Expect: 0.0 0.0\n1.0 0.0 (|1⟩)
cat tmp/STATE_INPUT_B.*.txt  # Expect: 0.0 0.0\n1.0 0.0 (|1⟩)
cat tmp/STATE_INPUT_C.*.txt  # Expect: 0.0 0.0\n1.0 0.0 (|1⟩)
cat tmp/INPUT_A.*.txt        # Expect: 0 (dummy)
cat tmp/CLOCK.*.txt          # Expect: 0

# Clean up
rm -f tmp/*.txt test_output.txt netlist/netlist_t_reversibility.txt tmp/netlist_reverse.txt
