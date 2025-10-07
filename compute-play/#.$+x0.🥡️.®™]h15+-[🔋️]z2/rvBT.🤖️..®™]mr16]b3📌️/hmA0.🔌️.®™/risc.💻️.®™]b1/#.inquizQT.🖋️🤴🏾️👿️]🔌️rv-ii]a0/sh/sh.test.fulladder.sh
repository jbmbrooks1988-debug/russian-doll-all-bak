#!/bin/sh
rm -rf tmp visual_mem.txt
mkdir -p netlist tmp

echo "Testing Full Adder netlist..."

# Test case: A=1, B=1, Cin=1 (Sum=1, Cout=1)
printf "1\n1\n1\n" > tmp/input.txt
./+x/main.+x netlist/netlist_fulladder.txt tmp/output.txt 1 > tmp/emulator_fulladder.log 2>&1
sum=$(cat tmp/output_sum.*.txt 2>/dev/null | tr -d '\n')
cout=$(cat tmp/output.*.txt 2>/dev/null | tr -d '\n')
if [ "$sum" = "1" ] && [ "$cout" = "1" ]; then
    echo "FullAdder(1,1,1) = Sum:1, Cout:1: PASS"
else
    echo "FullAdder(1,1,1) = Sum:1, Cout:1: FAIL (got Sum:$sum, Cout:$cout)"
    cat visual_mem.txt tmp/emulator_fulladder.log
    exit 1
fi

echo "Full Adder test passed!"
exit 0
