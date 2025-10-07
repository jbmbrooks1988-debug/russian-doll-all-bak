printf "1\n0\n" > tmp/input.txt
./+x/main.+x netlist/netlist_xor.txt test_output.txt 1
cat test_output.txt  # Expect: 1
