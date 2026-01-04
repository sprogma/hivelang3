fasm runtime.asm asm.o
clang runtime.c -c -o runtime.o -g -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE
clang runtime.o asm.o -o a.exe -g
rm asm.o, runtime.o
