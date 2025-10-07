#!/bin/bash
gcc main.c -o game -lglut -lGL -lGLU -lm
gcc ai_module.c -o ai_module -lm