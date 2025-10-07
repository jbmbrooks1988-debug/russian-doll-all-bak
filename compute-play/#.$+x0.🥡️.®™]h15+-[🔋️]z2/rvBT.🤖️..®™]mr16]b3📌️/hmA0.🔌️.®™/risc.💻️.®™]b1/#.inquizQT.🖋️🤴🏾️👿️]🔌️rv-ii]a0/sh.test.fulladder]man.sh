#!/bin/bash

# Create netlist_full_adder.txt
cat > netlist/netlist_full_adder.txt << EOL
INCLUDE netlist/netlist_xor.txt
./+x/nand.+x TMP_XOR INPUT_C TMP4
./+x/nand.+x INPUT_A INPUT_B TMP5
./+x/nand.+x INPUT_B INPUT_C TMP6
./+x/nand.+x INPUT_A INPUT_C TMP7
./+x/nand.+x TMP5 TMP5 TMP8
./+x/nand.+x TMP6 TMP6 TMP9
./+x/nand.+x TMP7 TMP7 TMP10
./+x/nand.+x TMP8 TMP9 TMP11
./+x/nand.+x TMP11 TMP10 TMP12
./+x/nand.+x TMP12 TMP12 OUTPUT_COUT
./+x/nand.+x TMP_XOR TMP4 TMP13
./+x/nand.+x TMP4 TMP4 TMP14
./+x/nand.+x TMP_XOR TMP13 TMP15
./+x/nand.+x TMP14 TMP15 TMP16
./+x/nand.+x TMP16 TMP16 OUTPUT_SUM
EOL

# Test Case 1: A=0, B=0, C=0
printf "0\n0\n0\n" > tmp/input.txt
./+x/main.+x netlist/netlist_full_adder.txt test_sum.txt 1
echo "Test 1 (A=0, B=0, C=0):"
cat test_sum.txt    # Expect: 0
cat test_cout.txt   # Expect: 0

# Test Case 2: A=1, B=0, C=0
printf "1\n0\n0\n" > tmp/input.txt
./+x/main.+x netlist/netlist_full_adder.txt test_sum.txt 1
echo "Test 2 (A=1, B=0, C=0):"
cat test_sum.txt    # Expect: 1
cat test_cout.txt   # Expect: 0

# Test Case 3: A=1, B=1, C=0
printf "1\n1\n0\n" > tmp/input.txt
./+x/main.+x netlist/netlist_full_adder.txt test_sum.txt 1
echo "Test 3 (A=1, B=1, C=0):"
cat test_sum.txt    # Expect: 0
cat test_cout.txt   # Expect: 1

# Test Case 4: A=1, B=1, C=1
printf "1\n1\n1\n" > tmp/input.txt
./+x/main.+x netlist/netlist_full_adder.txt test_sum.txt 1
echo "Test 4 (A=1, B=1, C=1):"
cat test_sum.txt    # Expect: 1
cat test_cout.txt   # Expect: 1

# Clean up
rm -f tmp/*.txt test_sum.txt test_cout.txt
