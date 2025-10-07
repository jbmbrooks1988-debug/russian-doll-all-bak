#!/bin/sh

# Clean and create directories
rm -rf tmp netlist
mkdir -p netlist tmp

# Test nand.+x
echo "Testing nand.+x..."

# Test NAND (1,1) = 0
echo "1" > tmp/input.txt
echo "1" >> tmp/input.txt
./+x/nand.+x tmp/input.txt tmp/output.txt
result=$(cat tmp/output.txt | tr -d '\n')
if [ "$result" = "0" ]; then
    echo "NAND(1,1) = 0: PASS"
else
    echo "NAND(1,1) = 0: FAIL (got $result)"
    exit 1
fi

# Test emulator with netlist for AND (1,1) = 1 using NAND
echo "Testing emulator with AND netlist..."
echo "./+x/nand.+x tmp/input.txt tmp/tmp_nand.txt" > netlist/netlist_and.txt
echo "./+x/nand.+x tmp/tmp_nand_duplicate.txt tmp/tmp_output.txt" >> netlist/netlist_and.txt
echo "1" > tmp/input.txt
echo "1" >> tmp/input.txt
cat tmp/input.txt > tmp/debug_input.txt
# Create tmp_nand.txt and tmp_nand_duplicate.txt
./+x/nand.+x tmp/input.txt tmp/tmp_nand.txt
cat tmp/tmp_nand.txt tmp/tmp_nand.txt > tmp/tmp_nand_duplicate.txt
cat tmp/tmp_nand.txt > tmp/debug_tmp_nand.txt
cat tmp/tmp_nand_duplicate.txt > tmp/debug_tmp_nand_duplicate.txt
./+x/main.+x netlist/netlist_and.txt tmp/output.txt 1 > tmp/emulator.log 2>&1
cat tmp/tmp_output.txt > tmp/debug_tmp_output.txt 2>/dev/null || echo "DEBUG: tmp/tmp_output.txt not found" > tmp/debug_tmp_output.txt
cat tmp/output.txt > tmp/debug_output.txt 2>/dev/null || echo "DEBUG: tmp/output.txt not found" > tmp/debug_output.txt
echo "DEBUG: Emulator log:"
cat tmp/emulator.log
echo "DEBUG: Listing tmp/ contents after emulator:"
ls -l tmp/
echo "DEBUG: Contents of tmp/debug_input.txt:"
cat tmp/debug_input.txt
echo "DEBUG: Contents of tmp/debug_tmp_nand.txt:"
cat tmp/debug_tmp_nand.txt 2>/dev/null || echo "DEBUG: tmp/debug_tmp_nand.txt not found"
echo "DEBUG: Contents of tmp/debug_tmp_nand_duplicate.txt:"
cat tmp/debug_tmp_nand_duplicate.txt 2>/dev/null || echo "DEBUG: tmp/debug_tmp_nand_duplicate.txt not found"
echo "DEBUG: Contents of tmp/debug_tmp_output.txt:"
cat tmp/debug_tmp_output.txt
echo "DEBUG: Contents of tmp/debug_output.txt:"
cat tmp/debug_output.txt
result=$(cat tmp/output.txt | tr -d '\n')
if [ "$result" = "1" ]; then
    echo "Emulator AND(1,1) = 1: PASS"
else
    echo "Emulator AND(1,1) = 1: FAIL (got $result)"
    exit 1
fi

# Test emulator with netlist for Full Adder (1,1,0) = sum:0, carry:1
echo "Testing emulator with Full Adder netlist..."
echo "./+x/nand.+x tmp/input_ab_0.txt tmp/tmp1_xor1_0.txt" > netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/input_a_0_tmp1_xor1_0.txt tmp/tmp2_xor1_0.txt" >> netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/input_b_0_tmp1_xor1_0.txt tmp/tmp3_xor1_0.txt" >> netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/tmp2_xor1_0.txt tmp/tmp3_xor1_0.txt tmp/tmp_xor_ab_0.txt" >> netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/input_cin_xor_ab_0.txt tmp/tmp1_xor2_0.txt" >> netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/tmp_xor_ab_0_tmp1_xor2_0.txt tmp/tmp2_xor2_0.txt" >> netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/input_cin_0_tmp1_xor2_0.txt tmp/tmp3_xor2_0.txt" >> netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/tmp2_xor2_0.txt tmp/tmp3_xor2_0.txt tmp/sum_0.txt" >> netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/input_ab_0.txt tmp/tmp_nand_ab_0.txt" >> netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/tmp_nand_ab_0_duplicate.txt tmp/tmp_and_ab_0.txt" >> netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/input_cin_xor_ab_0.txt tmp/tmp_nand_xc_0.txt" >> netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/tmp_nand_xc_0_duplicate.txt tmp/tmp_and_xc_0.txt" >> netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/tmp_and_ab_0_duplicate.txt tmp/tmp_not_and_ab_0.txt" >> netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/tmp_and_xc_0_duplicate.txt tmp/tmp_not_and_xc_0.txt" >> netlist/netlist_fulladder.txt
echo "./+x/nand.+x tmp/tmp_not_and_ab_0.txt tmp/tmp_not_and_xc_0.txt tmp/carry_0.txt" >> netlist/netlist_fulladder.txt
echo "1" > tmp/input_a_0.txt
echo "1" > tmp/input_b_0.txt
echo "0" > tmp/input_cin_0.txt
printf "1\n1\n" > tmp/input_ab_0.txt
# Debug initial inputs
echo "DEBUG: Contents of tmp/input_ab_0.txt:"
cat tmp/input_ab_0.txt
# Step-by-step intermediate file creation with debug
./+x/nand.+x tmp/input_ab_0.txt tmp/tmp1_xor1_0.txt || echo "ERROR: Failed to create tmp/tmp1_xor1_0.txt"
echo "DEBUG: Contents of tmp/tmp1_xor1_0.txt:"
cat tmp/tmp1_xor1_0.txt 2>/dev/null || echo "Not found"
printf "1\n%s\n" "$(cat tmp/tmp1_xor1_0.txt 2>/dev/null || echo 0)" > tmp/input_a_0_tmp1_xor1_0.txt
printf "1\n%s\n" "$(cat tmp/tmp1_xor1_0.txt 2>/dev/null || echo 0)" > tmp/input_b_0_tmp1_xor1_0.txt
echo "DEBUG: Contents of tmp/input_a_0_tmp1_xor1_0.txt:"
cat tmp/input_a_0_tmp1_xor1_0.txt
echo "DEBUG: Contents of tmp/input_b_0_tmp1_xor1_0.txt:"
cat tmp/input_b_0_tmp1_xor1_0.txt
./+x/nand.+x tmp/input_a_0_tmp1_xor1_0.txt tmp/tmp2_xor1_0.txt || echo "ERROR: Failed to create tmp/tmp2_xor1_0.txt"
echo "DEBUG: Contents of tmp/tmp2_xor1_0.txt:"
cat tmp/tmp2_xor1_0.txt 2>/dev/null || echo "Not found"
./+x/nand.+x tmp/input_b_0_tmp1_xor1_0.txt tmp/tmp3_xor1_0.txt || echo "ERROR: Failed to create tmp/tmp3_xor1_0.txt"
echo "DEBUG: Contents of tmp/tmp3_xor1_0.txt:"
cat tmp/tmp3_xor1_0.txt 2>/dev/null || echo "Not found"
./+x/nand.+x tmp/tmp2_xor1_0.txt tmp/tmp3_xor1_0.txt tmp/tmp_xor_ab_0.txt || echo "ERROR: Failed to create tmp/tmp_xor_ab_0.txt"
echo "DEBUG: Contents of tmp/tmp_xor_ab_0.txt:"
cat tmp/tmp_xor_ab_0.txt 2>/dev/null || echo "Not found"
printf "0\n%s\n" "$(cat tmp/tmp_xor_ab_0.txt 2>/dev/null || echo 0)" > tmp/input_cin_xor_ab_0.txt
echo "DEBUG: Contents of tmp/input_cin_xor_ab_0.txt:"
cat tmp/input_cin_xor_ab_0.txt
./+x/nand.+x tmp/input_ab_0.txt tmp/tmp_nand_ab_0.txt || echo "ERROR: Failed to create tmp/tmp_nand_ab_0.txt"
printf "%s\n%s\n" "$(cat tmp/tmp_nand_ab_0.txt 2>/dev/null || echo 0)" "$(cat tmp/tmp_nand_ab_0.txt 2>/dev/null || echo 0)" > tmp/tmp_nand_ab_0_duplicate.txt
./+x/nand.+x tmp/input_cin_xor_ab_0.txt tmp/tmp_nand_xc_0.txt || echo "ERROR: Failed to create tmp/tmp_nand_xc_0.txt"
printf "%s\n%s\n" "$(cat tmp/tmp_nand_xc_0.txt 2>/dev/null || echo 0)" "$(cat tmp/tmp_nand_xc_0.txt 2>/dev/null || echo 0)" > tmp/tmp_nand_xc_0_duplicate.txt
printf "%s\n%s\n" "$(cat tmp/tmp_and_ab_0.txt 2>/dev/null || echo 0)" "$(cat tmp/tmp_and_ab_0.txt 2>/dev/null || echo 0)" > tmp/tmp_and_ab_0_duplicate.txt
printf "%s\n%s\n" "$(cat tmp/tmp_and_xc_0.txt 2>/dev/null || echo 0)" "$(cat tmp/tmp_and_xc_0.txt 2>/dev/null || echo 0)" > tmp/tmp_and_xc_0_duplicate.txt
cat tmp/input_ab_0.txt > tmp/debug_input_ab_0.txt
cat tmp/input_cin_xor_ab_0.txt > tmp/debug_input_cin_xor_ab_0.txt
./+x/main.+x netlist/netlist_fulladder.txt tmp/output.txt 1 > tmp/emulator_fulladder.log 2>&1
cat tmp/sum_0.txt > tmp/debug_sum_0.txt 2>/dev/null || echo "DEBUG: tmp/sum_0.txt not found" > tmp/debug_sum_0.txt
cat tmp/carry_0.txt > tmp/debug_carry_0.txt 2>/dev/null || echo "DEBUG: tmp/carry_0.txt not found" > tmp/debug_carry_0.txt
echo "DEBUG: Full Adder emulator log:"
cat tmp/emulator_fulladder.log
sum=$(cat tmp/sum_0.txt | tr -d '\n')
carry=$(cat tmp/carry_0.txt | tr -d '\n')
if [ "$sum" = "0" ] && [ "$carry" = "1" ]; then
    echo "Emulator Full Adder(1,1,0) = sum:0, carry:1: PASS"
else
    echo "Emulator Full Adder(1,1,0) = sum:0, carry:1: FAIL (got sum:$sum, carry:$carry)"
    exit 1
fi

echo "All tests passed!"
exit 0
