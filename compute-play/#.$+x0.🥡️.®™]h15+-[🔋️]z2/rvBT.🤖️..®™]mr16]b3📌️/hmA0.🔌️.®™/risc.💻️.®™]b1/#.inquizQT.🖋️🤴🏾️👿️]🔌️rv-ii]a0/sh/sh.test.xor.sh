#!/bin/sh

# Clean tmp but preserve netlist
rm -rf tmp visual_mem.txt
mkdir -p netlist tmp

# Test emulator with XOR netlist
echo "Testing XOR netlist..."

# Create input file
printf "1\n1\n" > tmp/input.txt

# Create netlist
cat > netlist/netlist_xor.txt << EOL
./+x/nand.+x INPUT TMP1
./+x/nand.+x TMP_A_TMP1 TMP2
./+x/nand.+x TMP_B_TMP1 TMP3
./+x/nand.+x TMP2_3 OUTPUT
EOL

# Run emulator
./+x/main.+x netlist/netlist_xor.txt tmp/output.txt 1 > tmp/emulator_xor.log 2>&1

# Check results
result=$(cat tmp/output.*.txt 2>/dev/null | tr -d '\n')
if [ "$result" = "0" ]; then
    echo "XOR(1,1) = 0: PASS"
else
    echo "XOR(1,1) = 0: FAIL (got $result)"
    echo "DEBUG: Contents of visual_mem.txt:"
    cat visual_mem.txt 2>/dev/null || echo "visual_mem.txt not found"
    echo "DEBUG: Contents of tmp/emulator_xor.log:"
    cat tmp/emulator_xor.log 2>/dev/null
    exit 1
fi

echo "Test passed!"
exit 0
