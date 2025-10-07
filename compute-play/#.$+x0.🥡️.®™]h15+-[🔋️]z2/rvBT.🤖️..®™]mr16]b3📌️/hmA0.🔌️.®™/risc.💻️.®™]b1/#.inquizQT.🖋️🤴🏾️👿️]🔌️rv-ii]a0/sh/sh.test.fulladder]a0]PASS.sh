#!/bin/sh

# Clean and create directories
rm -rf tmp netlist visual_mem.txt
mkdir -p netlist tmp

# Test emulator with Full Adder netlist
echo "Testing Full Adder netlist..."

# Create input files
echo "1" > tmp/input_a_0.txt
echo "1" > tmp/input_b_0.txt
echo "0" > tmp/input_cin_0.txt
printf "1\n1\n" > tmp/input_ab_0.txt

# Create intermediate input files for XOR
./+x/nand.+x tmp/input_ab_0.txt tmp/tmp1_xor1_0.txt
printf "1\n%s\n" "$(cat tmp/tmp1_xor1_0.txt)" > tmp/input_a_tmp1_xor1_0.txt
printf "1\n%s\n" "$(cat tmp/tmp1_xor1_0.txt)" > tmp/input_b_tmp1_xor1_0.txt
./+x/nand.+x tmp/input_a_tmp1_xor1_0.txt tmp/tmp2_xor1_0.txt
./+x/nand.+x tmp/input_b_tmp1_xor1_0.txt tmp/tmp3_xor1_0.txt
printf "%s\n%s\n" "$(cat tmp/tmp2_xor1_0.txt)" "$(cat tmp/tmp3_xor1_0.txt)" > tmp/tmp2_3_xor1_0.txt
./+x/nand.+x tmp/tmp2_3_xor1_0.txt tmp/tmp_xor_ab_0.txt
printf "0\n%s\n" "$(cat tmp/tmp_xor_ab_0.txt)" > tmp/input_cin_xor_ab_0.txt
./+x/nand.+x tmp/input_cin_xor_ab_0.txt tmp/tmp1_xor2_0.txt
printf "%s\n%s\n" "$(cat tmp/tmp_xor_ab_0.txt)" "$(cat tmp/tmp1_xor2_0.txt)" > tmp/tmp_xor_ab_tmp1_xor2_0.txt
printf "0\n%s\n" "$(cat tmp/tmp1_xor2_0.txt)" > tmp/input_cin_tmp1_xor2_0.txt
./+x/nand.+x tmp/tmp_xor_ab_tmp1_xor2_0.txt tmp/tmp2_xor2_0.txt
./+x/nand.+x tmp/input_cin_tmp1_xor2_0.txt tmp/tmp3_xor2_0.txt
printf "%s\n%s\n" "$(cat tmp/tmp2_xor2_0.txt)" "$(cat tmp/tmp3_xor2_0.txt)" > tmp/tmp2_3_xor2_0.txt

# Create intermediate input files for carry
./+x/nand.+x tmp/input_ab_0.txt tmp/tmp_nand_ab_0.txt
printf "%s\n%s\n" "$(cat tmp/tmp_nand_ab_0.txt)" "$(cat tmp/tmp_nand_ab_0.txt)" > tmp/tmp_nand_ab_0_duplicate.txt
./+x/nand.+x tmp/tmp_nand_ab_0_duplicate.txt tmp/tmp_and_ab_0.txt
./+x/nand.+x tmp/input_cin_xor_ab_0.txt tmp/tmp_nand_xc_0.txt
printf "%s\n%s\n" "$(cat tmp/tmp_nand_xc_0.txt)" "$(cat tmp/tmp_nand_xc_0.txt)" > tmp/tmp_nand_xc_0_duplicate.txt
./+x/nand.+x tmp/tmp_nand_xc_0_duplicate.txt tmp/tmp_and_xc_0.txt
printf "%s\n%s\n" "$(cat tmp/tmp_and_ab_0.txt)" "$(cat tmp/tmp_and_ab_0.txt)" > tmp/tmp_and_ab_0_duplicate.txt
./+x/nand.+x tmp/tmp_and_ab_0_duplicate.txt tmp/tmp_not_and_ab_0.txt
printf "%s\n%s\n" "$(cat tmp/tmp_and_xc_0.txt)" "$(cat tmp/tmp_and_xc_0.txt)" > tmp/tmp_and_xc_0_duplicate.txt
./+x/nand.+x tmp/tmp_and_xc_0_duplicate.txt tmp/tmp_not_and_xc_0.txt
printf "%s\n%s\n" "$(cat tmp/tmp_not_and_ab_0.txt)" "$(cat tmp/tmp_not_and_xc_0.txt)" > tmp/tmp_not_and_ab_xc_0.txt

# Create netlist
cat > netlist/netlist_fulladder.txt << EOL
./+x/nand.+x tmp/input_ab_0.txt tmp/tmp1_xor1_0.txt
./+x/nand.+x tmp/input_a_tmp1_xor1_0.txt tmp/tmp2_xor1_0.txt
./+x/nand.+x tmp/input_b_tmp1_xor1_0.txt tmp/tmp3_xor1_0.txt
./+x/nand.+x tmp/tmp2_3_xor1_0.txt tmp/tmp_xor_ab_0.txt
./+x/nand.+x tmp/input_cin_xor_ab_0.txt tmp/tmp1_xor2_0.txt
./+x/nand.+x tmp/tmp_xor_ab_tmp1_xor2_0.txt tmp/tmp2_xor2_0.txt
./+x/nand.+x tmp/input_cin_tmp1_xor2_0.txt tmp/tmp3_xor2_0.txt
./+x/nand.+x tmp/tmp2_3_xor2_0.txt tmp/sum_0.txt
./+x/nand.+x tmp/input_ab_0.txt tmp/tmp_nand_ab_0.txt
./+x/nand.+x tmp/tmp_nand_ab_0_duplicate.txt tmp/tmp_and_ab_0.txt
./+x/nand.+x tmp/input_cin_xor_ab_0.txt tmp/tmp_nand_xc_0.txt
./+x/nand.+x tmp/tmp_nand_xc_0_duplicate.txt tmp/tmp_and_xc_0.txt
./+x/nand.+x tmp/tmp_and_ab_0_duplicate.txt tmp/tmp_not_and_ab_0.txt
./+x/nand.+x tmp/tmp_and_xc_0_duplicate.txt tmp/tmp_not_and_xc_0.txt
./+x/nand.+x tmp/tmp_not_and_ab_xc_0.txt tmp/carry_0.txt
EOL

# Run emulator
./+x/main]a0.+x netlist/netlist_fulladder.txt tmp/output.txt 1 > tmp/emulator_fulladder.log 2>&1

# Check results
sum=$(cat tmp/sum_0.txt 2>/dev/null | tr -d '\n')
carry=$(cat tmp/carry_0.txt 2>/dev/null | tr -d '\n')
if [ "$sum" = "0" ] && [ "$carry" = "1" ]; then
    echo "Full Adder(1,1,0) = sum:0, carry:1: PASS"
else
    echo "Full Adder(1,1,0) = sum:0, carry:1: FAIL (got sum:$sum, carry:$carry)"
    echo "DEBUG: Contents of visual_mem.txt:"
    cat visual_mem.txt 2>/dev/null || echo "visual_mem.txt not found"
    echo "DEBUG: Contents of tmp/emulator_fulladder.log:"
    cat tmp/emulator_fulladder.log 2>/dev/null
    exit 1
fi

echo "Test passed!"
exit 0
