# Minimal C Compiler for 16-bit RISC-V Processor

## Overview
This is a minimal C compiler designed for the 16-bit RISC-V processor in the HDLB0 project. It translates a subset of C to 16-bit RISC-V assembly code.

## Features
- Lexical analysis (tokenization)
- Parsing of simple expressions
- Variable assignment
- Basic arithmetic operations (+, -, *, /)
- Generation of 16-bit RISC-V assembly code

## Supported C Constructs
- Variable declarations and assignments
- Integer literals
- Basic arithmetic expressions
- Parentheses for grouping

## Example Usage
The compiler can handle simple C code like:
```c
a = 5;
b = 3;
c = a + b;
d = c * 2;
```

## Architecture
- **Lexer**: Tokenizes the input C source code
- **Parser**: Parses statements and expressions
- **Code Generator**: Generates 16-bit RISC-V assembly
- **Symbol Table**: Tracks variables and their register assignments

## Limitations
- No function support
- No control flow (if/else, loops)
- No data types beyond integers
- No arrays or pointers
- No standard library functions
- Simple register allocation (assigns registers sequentially)

## Output Format
The compiler generates assembly code in the format expected by the 16-bit RISC-V assembler, with:
- Register names (x0-x7 for our simplified implementation)
- Basic RISC-V instructions (add, sub, li, etc.)
- System calls for program termination

## Integration
The generated assembly code can be assembled using the existing assembler in the riscv_16bit directory.