#!/bin/bash🤴🏾️

# Create netlist_toffoli.txt
cat > netlist/netlist_toffoli.txt << EOL
./+x/nand.+x INPUT_A INPUT_B TMP1
./+x/nand.+x TMP1 TMP1 TMP2
./+x/nand.+x TMP2 INPUT_C OUTPUT_C
EOL

# Test Case 1: A=1, B=1, C=1 (NAND-like: expect OUTPUT_C=0)
printf "1\n1\n1\n" > tmp/input.txt
./+x/main.+x netlist/netlist_toffoli.txt test_output.txt 1
echo "Test 1 (A=1, B=1, C=1):"
cat test_output.txt  # Expect: 0

# Test Case 2: A=1, B=0, C=1 (NAND-like: expect OUTPUT_C=1)
printf "1\n0\n1\n" > tmp/input.txt
./+x/main.+x netlist/netlist_toffoli.txt test_output.txt 1
echo "Test 2 (A=1, B=0, C=1):"
cat test_output.txt  # Expect: 1

# Test Case 3: A=0, B=1, C=1 (NAND-like: expect OUTPUT_C=1)
printf "0\n1\n1\n" > tmp/input.txt
./+x/main.+x netlist/netlist_toffoli.txt test_output.txt 1
echo "Test 3 (A=0, B=1, C=1):"
cat test_output.txt  # Expect: 1

# Test Case 4: A=0, B=0, C=1 (NAND-like: expect OUTPUT_C=1)
printf "0\n0\n1\n" > tmp/input.txt
./+x/main.+x netlist/netlist_toffoli.txt test_output.txt 1
echo "Test 4 (A=0, B=0, C=1):"
cat test_output.txt  # Expect: 1

# Clean up
rm -f tmp/*.txt test_output.txt
