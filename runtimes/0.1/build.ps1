fasm runtime.asm asm.o
clang runtime.c -c -o runtime.o
clang runtime.o asm.o -o a.exe
rm asm.o, runtime.o
