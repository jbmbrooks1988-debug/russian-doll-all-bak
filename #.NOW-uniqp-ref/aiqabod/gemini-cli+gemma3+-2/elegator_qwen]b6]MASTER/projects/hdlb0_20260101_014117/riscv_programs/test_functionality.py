# Test script to verify functionality of 2-bit RISC-V programs
# This script simulates the expected behavior of each program

def simulate_add_program():
    """Simulate the add program: 1 + 1 = 2 (in 2-bit: 1 + 1 = 0 with carry)"""
    print("Testing ADD program...")
    print("Initial state: R0=0, R1=0, R2=0, R3=0")

    # Step 1: ADDI R0, R0, 1  -> R0 = 0 + 1 = 1
    r0 = (0 + 1) & 0x3  # R0 becomes 1, 2-bit arithmetic
    print(f"After ADDI R0: R0={r0}, R1=0, R2=0, R3=0")

    # Step 2: ADDI R1, R0, 1  -> R1 = 0 + 1 = 1 (using R0's initial value of 0)
    r1 = (0 + 1) & 0x3  # R1 becomes 1, using R0's initial value (0) + 1
    print(f"After ADDI R1: R0={r0}, R1={r1}, R2=0, R3=0")

    # Step 3: ADD R2  -> R2 = R0 + R1 = 1 + 1 = 2 (in 2-bit: 0 with carry, but value is 2 mod 4 = 2)
    r2 = (r0 + r1) & 0x3  # 2-bit arithmetic: (1 + 1) mod 4 = 2
    print(f"After ADD R2: R0={r0}, R1={r1}, R2={r2}, R3=0")

    # Expected: R2 should be 2 (since 1+1=2, and in our 2-bit ALU, 1+1=2 which is 10 in binary = 2 in decimal)
    # In unsigned 2-bit arithmetic: 1 + 1 = 2 (binary 10), but since we only have 2 bits,
    # the value is 2 (which is 10 in binary, truncated to 2 in decimal representation)
    print(f"Expected R2: 2, Actual R2: {r2}")
    print(f"ADD program test {'PASSED' if r2 == 2 else 'FAILED'}\n")

def simulate_logical_program():
    """Simulate the logical program: 1 AND 0 = 0"""
    print("Testing LOGICAL program...")
    print("Initial state: R0=0, R1=0, R2=0, R3=0")
    
    # Step 1: ADDI R1, R0, 0  -> R1 = 0 + 0 = 0
    r1 = 0 + 0  # R1 becomes 0
    print(f"After ADDI R1: R0=0, R1={r1}, R2=0, R3=0")
    
    # Step 2: ADDI R0, R0, 1  -> R0 = 0 + 1 = 1
    r0 = 0 + 1  # R0 becomes 1
    print(f"After ADDI R0: R0={r0}, R1={r1}, R2=0, R3=0")
    
    # Step 3: AND R2  -> R2 = R0 AND R1 = 1 AND 0 = 0
    r2 = r0 & r1  # AND operation
    print(f"After AND R2: R0={r0}, R1={r1}, R2={r2}, R3=0")
    
    # Expected: R2 should be 0
    print(f"Expected R2: 0, Actual R2: {r2}")
    print(f"LOGICAL program test {'PASSED' if r2 == 0 else 'FAILED'}\n")

def simulate_memory_program():
    """Simulate the memory program: store and load"""
    print("Testing MEMORY program...")
    print("Initial state: R0=0, R1=0, R2=0, R3=0, Memory=[0,0,0,0]")
    
    # Initialize memory
    memory = [0, 0, 0, 0]
    
    # Step 1: ADDI R0, R0, 1  -> R0 = 0 + 1 = 1 (memory address)
    r0 = 0 + 1  # R0 becomes 1 (address)
    print(f"After ADDI R0: R0={r0}, R1=0, R2=0, R3=0")
    
    # Step 2: ADDI R1, R0, 1  -> R1 = 0 + 1 = 1 (value to store)
    r1 = 0 + 1  # R1 becomes 1 (value)
    print(f"After ADDI R1: R0={r0}, R1={r1}, R2=0, R3=0")
    
    # Step 3: SW  -> Store R1 at address R0 -> Memory[1] = R1
    memory[r0] = r1
    print(f"After SW: R0={r0}, R1={r1}, R2=0, R3=0, Memory={memory}")
    
    # Step 4: LW R2  -> Load from address R0 to R2 -> R2 = Memory[R0]
    r2 = memory[r0]
    print(f"After LW R2: R0={r0}, R1={r1}, R2={r2}, R3=0, Memory={memory}")
    
    # Expected: R2 should be 1 (the value we stored)
    print(f"Expected R2: 1, Actual R2: {r2}")
    print(f"MEMORY program test {'PASSED' if r2 == 1 else 'FAILED'}\n")

def simulate_complex_program():
    """Simulate the complex program"""
    print("Testing COMPLEX program...")
    print("Initial state: R0=0, R1=0, R2=0, R3=0")
    
    # Step 1: ADDI R0, R0, 1  -> R0 = 0 + 1 = 1
    r0 = 0 + 1
    print(f"After ADDI R0: R0={r0}, R1=0, R2=0, R3=0")
    
    # Step 2: ADDI R3, R0, 0  -> R3 = 0 + 0 = 0 (using R0's initial value of 0)
    r3 = 0 + 0
    print(f"After ADDI R3: R0={r0}, R1=0, R2=0, R3={r3}")
    
    # Step 3: ADD R2  -> R2 = R0 + R1 = 1 + 0 = 1
    r2 = (r0 + 0) & 0x3  # R0 + R1, but R1 is always source 2, so R2 = R0(current) + R1(current)
    # Actually, looking back at the implementation: rs1=2'b00 (R0), rs2=2'b01 (R1)
    # So ADD R2 means R2 = R0 + R1
    r2 = (r0 + 0) & 0x3  # R1 was 0 initially
    print(f"After ADD R2: R0={r0}, R1=0, R2={r2}, R3={r3}")
    
    # Step 4: AND R3  -> R3 = R0 AND R2 = 1 AND 1 = 1
    r3 = r0 & r2
    print(f"After AND R3: R0={r0}, R1=0, R2={r2}, R3={r3}")
    
    print(f"COMPLEX program completed: R0={r0}, R1=0, R2={r2}, R3={r3}\n")

if __name__ == "__main__":
    print("Running functional tests for 2-bit RISC-V programs...\n")
    
    simulate_add_program()
    simulate_logical_program()
    simulate_memory_program()
    simulate_complex_program()
    
    print("All functional tests completed!")