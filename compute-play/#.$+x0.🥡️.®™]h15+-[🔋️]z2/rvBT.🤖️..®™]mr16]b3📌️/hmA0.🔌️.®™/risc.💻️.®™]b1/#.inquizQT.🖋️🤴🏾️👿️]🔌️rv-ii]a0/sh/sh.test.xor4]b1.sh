#!/bin/sh

echo "Testing XOR netlist..."
rm -f test.txt tmp/*

# Test cases: A B demorgan, B Expected (XOR truth table)
tests="0 0 0 0 1 1 1 0 1 1 1 0"

# Process tests using while loop
echo "$tests" | while read a b expected; do
    # Skip empty lines
    [ -z "$a" ] && continue
    
    # Create input.txt with exactly two lines
    printf "%s\n%s\n" "$a" "$b" > tmp/input.txt
    # Debug: Show input.txt content
    echo "DEBUG: tmp/input.txt contents:"
    cat tmp/input.txt
    
    ./+x/main.+x netlist/netlist_xor.txt test.txt 1 > test.log 2>&1
    if [ ! -f test.txt ]; then
        echo "Test failed: test.txt not created for XOR($a,$b)"
        echo "DEBUG: main.+x output:"
        cat test.log
        rm -f tmp/* test.txt test.log
        exit 1
    fi
    result=$(cat test.txt)
    
    if [ "$result" = "$expected" ]; then
        echo "Test passed: XOR($a,$b)=$result"
    else
        echo "Test failed: XOR($a,$b)=$result, expected $expected"
        rm -f tmp/* test.txt test.log
        exit 1
    fi
    rm -f tmp/* test.txt test.log
done

echo "All XOR tests passed!"
