#!/bin/sh

echo "Testing INCLUDE netlist..."
rm -f test.txt tmp/*

# Ensure netlist_xor.txt exists
if [ ! -f netlist/netlist_xor.txt ]; then
    echo "ERROR: netlist/netlist_xor.txt not found"
    exit 1
fi

# Create netlist_include_test.txt
cat > netlist/netlist_include_test.txt << EOF
INCLUDE netlist/netlist_xor.txt
./+x/nand.+x TMP3 OUTPUT
EOF

# Test cases: A B Expected (for NAND(TMP3, OUTPUT))
tests="0 0 1 0 1 0 1 0 0 1 1 1"

# Process tests using while loop
echo "$tests" | while read a b expected; do
    # Skip empty lines
    [ -z "$a" ] && continue
    
    # Create input.txt with exactly two lines
    printf "%s\n%s\n" "$a" "$b" > tmp/input.txt
    # Debug: Show input.txt content
    echo "DEBUG: tmp/input.txt contents:"
    cat tmp/input.txt
    
    ./+x/main.+x netlist/netlist_include_test.txt test.txt 1 > test.log 2>&1
    if [ ! -f test.txt ]; then
        echo "Test failed: test.txt not created for INCLUDE XOR($a,$b)"
        echo "DEBUG: main.+x output:"
        cat test.log
        rm -f tmp/* test.txt test.log netlist/netlist_include_test.txt
        exit 1
    fi
    result=$(cat test.txt)
    
    if [ "$result" = "$expected" ]; then
        echo "Test passed: INCLUDE XOR($a,$b) -> NAND(TMP3,OUTPUT)=$result"
    else
        echo "Test failed: INCLUDE XOR($a,$b) -> NAND(TMP3,OUTPUT)=$result, expected $expected"
        rm -f tmp/* test.txt test.log netlist/netlist_include_test.txt
        exit 1
    fi
    rm -f tmp/* test.txt test.log
done

echo "All INCLUDE tests passed!"
rm -f netlist/netlist_include_test.txt
