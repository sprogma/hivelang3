clang -std=gnu++2c (ls *.cpp) -o a.exe -g -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -fsanitize=address
