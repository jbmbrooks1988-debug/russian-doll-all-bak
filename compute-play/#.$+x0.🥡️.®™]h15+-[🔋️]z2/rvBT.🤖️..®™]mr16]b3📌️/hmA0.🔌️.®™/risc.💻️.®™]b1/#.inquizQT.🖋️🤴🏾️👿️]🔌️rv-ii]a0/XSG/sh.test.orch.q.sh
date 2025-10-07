#!/bin/bash

# Create netlist_quantum.txt with quantum.+x
cat > netlist/netlist_quantum.txt << EOL
./+x/quantum.+x toffoli INPUT_A INPUT_B INPUT_C OUTPUT_A OUTPUT_B OUTPUT_C
./+x/quantum.+x hadamard ANCILLA_1 TMP_ANCILLA_1
./+x/quantum.+x cnot TMP_ANCILLA_1 ANCILLA_2 OUTPUT_QUBIT_1 OUTPUT_QUBIT_2
EOL

# Test Case 1: Run orchestrator for 2 cycles (Toffoli and Bell state)
echo "Test 1: 2 cycles with CLOCK signal (automated)"
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

# Test Case 2: Run orchestrator in manual mode (2 cycles)
echo "Test 2: 2 cycles with CLOCK signal (manual mode)"
# Run in background to simulate user input
./+x/orchestrator.+x netlist/netlist_quantum.txt test_output.txt 3 2 manual &
ORCH_PID=$!
sleep 1
echo "Simulating Enter press for cycle 1"
echo | kill -SIGCONT $ORCH_PID
sleep 1
echo "Simulating Enter press for cycle 2"
echo | kill -SIGCONT $ORCH PID
wait $ORCH_PID
echo "Cycle 0 (Toffoli: A=1, B=1, C=0, CLOCK=0):"
cat tmp/OUTPUT_A.*.txt  # Expect: 1
cat tmp/OUTPUT_B.*.txt  # Expect: 1
cat tmp/OUTPUT_C.*.txt  # Expect: 1
cat tmp/CLOCK.*.txt     # Expect: 0
echo "Cycle 1 (Bell State, CLOCK=1):"
cat tmp/OUTPUT_QUBIT_1.*.txt  # Expect: 0 or 1
cat tmp/OUTPUT_QUBIT_2.*.txt  # Expect: 0 or 1
cat tmp/CLOCK.*.txt           # Expect: 1

# Clean up
rm -f tmp/*.txt test_output.txt
