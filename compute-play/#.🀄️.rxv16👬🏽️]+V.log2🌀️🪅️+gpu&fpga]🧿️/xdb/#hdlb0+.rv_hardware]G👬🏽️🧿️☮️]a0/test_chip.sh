#!/bin/bash

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <program_file> <num_cycles> [switch_inputs] [initial_ram_values]"
    echo "  initial_ram_values: Comma-separated list of address=value pairs (e.g., 43=1,36=0)"
    exit 1
fi

PROGRAM_FILE=$1
NUM_CYCLES=$2
SWITCH_INPUTS=$3
INITIAL_RAM_VALUES=$4
EMULATOR="./+x/0.hdlb0.☮️16]pr3]HOLY.+x"
RAM_FILE="ram_output_address.txt"
PC_ADDRESS=37 # PC is at RAM[36], so line 37 in 1-indexed file

# Initialize RAM
rm -f "$RAM_FILE"
for (( i=0; i<256; i++ )); do
    echo "0" >> "$RAM_FILE"
done

# Apply initial RAM values
if [ -n "$INITIAL_RAM_VALUES" ]; then
    IFS=',' read -ra ADDR_VAL_PAIRS <<< "$INITIAL_RAM_VALUES"
    for pair in "${ADDR_VAL_PAIRS[@]}"; do
        IFS='=' read -ra ADDR_VAL <<< "$pair"
        ADDR=${ADDR_VAL[0]}
        VAL=${ADDR_VAL[1]}
        sed -i "$((ADDR + 1))s/.*/$VAL/" "$RAM_FILE"
    done
fi

# Read program into an array
mapfile -t instructions < "$PROGRAM_FILE"

if [ ! -f "$EMULATOR" ]; then
    echo "Emulator not found at $EMULATOR"
    exit 1
fi

if [ ! -f "$PROGRAM_FILE" ]; then
    echo "Program file not found at $PROGRAM_FILE"
    exit 1
fi

# Main loop
for (( i=0; i<NUM_CYCLES; i++ )); do
    # Read PC
    PC=$(sed -n "${PC_ADDRESS}p" "$RAM_FILE")

    # Get instruction
    INSTRUCTION=${instructions[$PC]}

    # Create temporary program file
    echo "$INSTRUCTION" > temp_program.txt

    # Prepare input for the emulator
    INPUT=""
    if [ -n "$SWITCH_INPUTS" ]; then
        for (( j=0; j<${#SWITCH_INPUTS}; j++ )); do
            INPUT+="${SWITCH_INPUTS:$j:1}\n"
        done
    fi
    INPUT+="s\n q\n"

    # Run emulator with single instruction
    echo -e "$INPUT" | "$EMULATOR" temp_program.txt

    # Increment PC if no jump occurred
    NEW_PC=$(sed -n "${PC_ADDRESS}p" "$RAM_FILE")
    if [ "$PC" -eq "$NEW_PC" ]; then
        ((PC++))
        sed -i "${PC_ADDRESS}s/.*/$PC/" "$RAM_FILE"
    fi
done

rm temp_program.txt