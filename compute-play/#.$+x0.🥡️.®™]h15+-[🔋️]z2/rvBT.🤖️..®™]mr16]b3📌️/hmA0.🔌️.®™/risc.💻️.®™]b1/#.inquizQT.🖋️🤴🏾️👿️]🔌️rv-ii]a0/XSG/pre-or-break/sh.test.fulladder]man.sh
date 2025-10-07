#!/bin/sh

echo "Testing Full Adder netlist (manual style)..."
rm -f test_sum.txt test_cout.txt tmp/* test.log

# Ensure netlist_xor.txt exists
if [ ! -f netlist/netlist_xor.txt ]; then
    echo "ERROR: netlist/netlist_xor.txt not found"
    exit 1
fi

# Create netlist_full_adder.txt
cat > netlist/netlist_full_adder.txt << EOF
INCLUDE netlist/netlist_xor.txt
./+x/nand.+x TMP3 INPUT TMP4
./+x/nand.+x TMP_A_TMP1 TMP_B_TMP1 TMP5
./+x/nand.+x TMP_B_TMP1 INPUT TMP6
./+x/nand.+x TMP_A_TMP1 INPUT TMP7
./+x/nand.+x TMP5 TMP5 TMP8
./+x/nand.+x TMP6 TMP6 TMP9
./+x/nand.+x TMP7 TMP7 TMP10
./+x/nand.+x TMP8 TMP9 TMP11
./+x/nand.+x TMP10 TMP11 OUTPUT_COUT
./+x/nand.+x TMP4 TMP4 OUTPUT_SUM
EOF

# Test cases: A B Cin Expected_Sum Expected_Cout
tests="0 0 0 0 0
       0 0 1 1 0
       0 1 0 1 0
       0 1 1 0 1
       1 0 0 1 0
       1 0 1 0 1
       1 1 0 0 1
       1 1 1 1 1"

success=1
echo "$tests" | while read a b cin expected_sum expected_cout; do
    # Skip empty lines
    [ -z "$a" ] && continue
    
    # Validate inputs
    case "$a$b$cin" in
        000|001|010|011|100|101|110|111) ;;
        *) echo "Invalid input: a=$a, b=$b, cin=$cin"; success=0; exit 1 ;;
    esac
    
    # Create input.txt exactly like manual run
    printf "%s\n%s\n%s\n" "$a" "$b" "$cin" > tmp/input.txt
    echo "DEBUG: tmp/input.txt contents:"
    cat tmp/input.txt
    
    # Run main.+x directly
    ./+x/main.+x netlist/netlist_full_adder.txt test_sum.txt 1 > test.log 2>&1
    if [ $? -ne 0 ]; then
        echo "Test failed: main.+x failed for Full Adder($a,$b,$cin)"
        echo "DEBUG: main.+x output:"
        cat test.log
        rm -f tmp/* test_sum.txt test_cout.txt test.log netlist/netlist_full_adder.txt
        success=0
        exit 1
    fi
    
    # Check outputs
    if [ ! -f test_sum.txt ] || [ ! -f test_cout.txt ]; then
        echo "Test failed: Output files missing for Full Adder($a,$b,$cin)"
        echo "DEBUG: main.+x output:"
        cat test.log
        rm -f tmp/* test_sum.txt test_cout.txt test.log netlist/netlist_full_adder.txt
        success=0
        exit 1
    fi
    
    result_sum=$(cat test_sum.txt)
    result_cout=$(cat test_cout.txt)
    
    if [ "$result_sum" = "$expected_sum" ] && [ "$result_cout" = "$expected_cout" ]; then
        echo "Test passed: Full Adder($a,$b,$cin) = Sum=$result_sum, Cout=$result_cout"
    else
        echo "Test failed: Full Adder($a,$b,$cin) = Sum=$result_sum, Cout=$result_cout, expected Sum=$expected_sum, Cout=$expected_cout"
        echo "DEBUG: main.+x output:"
        cat test.log
        rm -f tmp/* test_sum.txt test_cout.txt test.log netlist/netlist_full_adder.txt
        success=0
        exit 1
    fi
    rm -f tmp/* test_sum.txt test_cout.txt test.log
done

if [ $success -eq 1 ]; then
    echo "All Full Adder tests passed!"
else
    echo "Some Full Adder tests failed."
    exit 1
fi
rm -f netlist/netlist_full_adder.txt
