echo "./+x/nand.+x INPUT_A INPUT_B OUTPUT" > netlist/netlist_nand.txt
printf "1\n1\n" > tmp/input.txt
./+x/main.+x netlist/netlist_nand.txt test_output.txt 1
cat test_output.txt  # Expect: 0
