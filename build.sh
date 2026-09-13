#!/bin/bash
fasm src/context_switch.asm ./build/context_switch.o
FLAGS_DIR="-I./include"
WARNINGS="-Wall -Wextra -Werror"
gcc -ggdb $WARNINGS -c src/main.c $FLAGS_DIR -o ./build/main.o
gcc -ggdb $WARNINGS -c src/task.c $FLAGS_DIR -o ./build/task.o    
gcc -ggdb $WARNINGS -c src/parser.c $FLAGS_DIR -o ./build/parser.o
gcc -ggdb $WARNINGS -c src/term.c $FLAGS_DIR -o ./build/term.o    
gcc -ggdb $WARNINGS -c src/trie.c $FLAGS_DIR -o ./build/trie.o    
gcc -ggdb $WARNINGS -c src/cmd.c $FLAGS_DIR -o ./build/cmd.o      
gcc -ggdb $WARNINGS -g3 -O0 -fno-omit-frame-pointer ./build/context_switch.o ./build/task.o ./build/trie.o ./build/term.o ./build/parser.o ./build/cmd.o ./build/main.o -luring -o ./build/main
