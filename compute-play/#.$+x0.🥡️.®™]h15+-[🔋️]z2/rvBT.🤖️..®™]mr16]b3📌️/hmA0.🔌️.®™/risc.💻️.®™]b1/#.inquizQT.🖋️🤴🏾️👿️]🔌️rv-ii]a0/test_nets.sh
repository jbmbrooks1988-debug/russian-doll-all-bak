#!/bin/bash

# Test script for XOR and Full Adder netlists
# Validates outputs without deleting netlist files
# Usage: ./test_nets.sh

# Exit on error
set -e

# Ensure tmp/ exists
mkdir -p tmp

# Compile executables (if needed)
echo "Compiling main.c and io_manager.c..."
gcc main.c -o +x/main.+x
gcc io_manager.c -o +x/io_manager.+x

# Test XOR netlist
echo "Testing XOR netlist..."
echo "A=0, B=0"
printf "0\n0\n" > tmp/input.txt
./+x/main.+x netlist/netlist_xor.txt test_output.txt 1 > test_xor.log 2>&1
echo "Expected: 0, Got: $(cat test_output.txt)"
if [ "$(cat test_output.txt)" != "0" ]; then
    echo "ERROR: XOR failed for A=0, B=0"
    exit 1
fi

echo "A=1, B=0"
printf "1\n0\n" > tmp/input.txt
./+x/main.+x netlist/netlist_xor.txt test_output.txt 1 >> test_xor.log 2>&1
echo "Expected: 1, Got: $(cat test_output.txt)"
if [ "$(cat test_output.txt)" != "1" ]; then
    echo "ERROR: XOR failed for A=1, B=0"
    exit 1
fi

echo "A=0, B=1"
printf "0\n1\n" > tmp/input.txt
./+x/main.+x netlist/netlist_xor.txt test_output.txt 1 >> test_xor.log 2>&1
echo "Expected: 1, Got: $(cat test_output.txt)"
if [ "$(cat test_output.txt)" != "1" ]; then
    echo "ERROR: XOR failed for A=0, B=1"
    exit 1
fi

echo "A=1, B=1"
printf "1\n1\n" > tmp/input.txt
./+x/main.+x netlist/netlist_xor.txt test_output.txt 1 >> test_xor.log 2>&1
echo "Expected: 0, Got: $(cat test_output.txt)"
if [ "$(cat test_output.txt)" != "0" ]; then
    echo "ERROR: XOR failed for A=1, B=1"
    exit 1
fi

# Test Full Adder netlist
echo "Testing Full Adder netlist..."
echo "A=0, B=0, Cin=0"
printf "0\n0\n0\n" > tmp/input.txt
./+x/main.+x netlist/netlist_full_adder.txt test_sum.txt 1 > test_full_adder.log 2>&1
echo "Expected S=0, Cout=0, Got S=$(cat test_sum.txt), Cout=$(cat test_cout.txt)"
if [ "$(cat test_sum.txt)" != "0" ] || [ "$(cat test_cout.txt)" != "0" ]; then
    echo "ERROR: Full Adder failed for A=0, B=0, Cin=0"
    exit 1
fi

echo "A=1, B=0, Cin=0"
printf "1\n0\n0\n" > tmp/input.txt
./+x/main.+x netlist/netlist_full_adder.txt test_sum.txt 1 >> test_full_adder.log 2>&1
echo "Expected S=1, Cout=0, Got S=$(cat test_sum.txt), Cout=$(cat test_cout.txt)"
if [ "$(cat test_sum.txt)" != "1" ] || [ "$(cat test_cout.txt)" != "0" ]; then
    echo "ERROR: Full Adder failed for A=1, B=0, Cin=0"
    exit 1
fi

echo "A=0, B=1, Cin=1"
printf "0\n1\n1\n" > tmp/input.txt
./+x/main.+x netlist/netlist_full_adder.txt test_sum.txt 1 >> test_full_adder.log 2>&1
echo "Expected S=0, Cout=1, Got S=$(cat test_sum.txt), Cout=$(cat test_cout.txt)"
if [ "$(cat test_sum.txt)" != "0" ] || [ "$(cat test_cout.txt)" != "1" ]; then
    echo "ERROR: Full Adder failed for A=0, B=1, Cin=1"
    exit 1
fi

echo "A=1, B=1, Cin=1"
printf "1\n1\n1\n" > tmp/input.txt
./+x/main.+x netlist/netlist_full_adder.txt test_sum.txt 1 >> test_full_adder.log 2>&1
echo "Expected S=1, Cout=1, Got S=$(cat test_sum.txt), Cout=$(cat test_cout.txt)"
if [ "$(cat test_sum.txt)" != "1" ] || [ "$(cat test_cout.txt)" != "1" ]; then
    echo "ERROR: Full Adder failed for A=1, B=1, Cin=1"
    exit 1
fi

# Clean up (preserve netlists)
echo "Cleaning up temporary files..."
rm -f tmp/*.txt test_output.txt test_sum.txt test_cout.txt
echo "Preserving netlist/ directory and logs (test_xor.log, test_full_adder.log)"

echo "All tests passed!"
