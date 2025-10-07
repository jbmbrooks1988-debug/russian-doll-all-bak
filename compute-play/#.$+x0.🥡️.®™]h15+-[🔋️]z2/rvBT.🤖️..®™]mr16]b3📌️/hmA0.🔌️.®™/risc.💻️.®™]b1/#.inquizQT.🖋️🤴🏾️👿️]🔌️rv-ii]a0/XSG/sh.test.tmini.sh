
#https://x.com/i/grok?conversation=1922219753822916690

#.t gate works just fine. its the script that was bad. 
# editing other aspects of the code is crazy
#. and nand works like a dream, were not gonna dig into this anymore ritenow🤡️🥶️
#!/bin/bash

# Create a minimal netlist for T gate
cat > netlist/netlist_t_gate.txt << EOL
./+x/quantum.+x t INPUT_T OUTPUT_T
EOL

# Test: Basic T gate in quantum mode (1 cycle)
echo "Test: Basic T gate in quantum mode"
mkdir -p tmp
# Initialize INPUT_T with |1⟩ state (classical bit 1 for simplicity, quantum state in STATE file)
echo "1" > tmp/INPUT_T.289383.txt
echo -e "0.000000 0.000000\n1.000000 0.000000" > tmp/STATE_INPUT_T.289383.txt
echo "INPUT_T tmp/INPUT_T.289383.txt" > tmp/input.txt
echo "quantum" > tmp/quantum_mode.txt
echo "0" > tmp/CLOCK.289383.txt

# Run the orchestrator with 1 cycle, quantum mode
./+x/orchestrator.+x netlist/netlist_t_gate.txt test_output.txt 2 1 --quantum

# Display results
echo "T gate output (INPUT_T=|1⟩):"
if [ -f tmp/STATE_OUTPUT_T.289383.txt ]; then
    cat tmp/STATE_OUTPUT_T.289383.txt  # Expect: ~0.0 0.0\n~0.707107 0.707107
else
    echo "Error: Output state file not found"
fi
if [ -f tmp/OUTPUT_T.289383.txt ]; then
    cat tmp/OUTPUT_T.289383.txt  # Expect: 0 (dummy)
else
    echo "Error: Dummy output file not found"
fi

# Clean up
rm -rf tmp/*.txt test_output.txt netlist/netlist_t_gate.txt
