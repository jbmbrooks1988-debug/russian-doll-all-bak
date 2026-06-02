#!/bin/bash
# Compile script for AIDA monolith
gcc -std=c11 -Wall -Wextra -O2 -D_DEFAULT_SOURCE -pthread -o monolith monolith.c
echo "Build complete: monolith"
