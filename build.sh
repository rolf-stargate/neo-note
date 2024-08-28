#!/bin/bash

gcc -g src/main.c -o build/main -O0  -std=c99 -Wno-missing-braces -L ./lib/ -lraylib
