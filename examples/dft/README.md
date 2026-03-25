# DFT Test

## Information

Makes dft transform of input array (given in format `N a[0] a[1] ... a[N-1]`), returns sum of all elements as exit code.

This test will compare speed of hive program, and same c program.

## Build

Build example.c
```
clang example.c -o a.exe
```

Build hive program: in root of project
```
./a.exe --input-file=./examples/dft/example.hive --optimize_query_var=true
```

create test: (1e6 numbers)
```
"r">test.txt;($n=1MB)>>test.txt;(1..$n-join" ")>>"test.txt"
```

run c program
```
gc -Raw "test.txt" | ./a.exe
```

run hive program: in `runtimes/0.3/win`
```
gc -Raw "../../../examples/dft/test.txt" | ./a.exe p10000000
```

on my machine i have
* c: 200ms
* hive: 1500ms

BUT! this is becouse of universal object system, used in hive program.
Now, let's build another version - the only diff is usage of `loc` provider for array allocation

Build hive program: in root of project
```
./a.exe --input-file=./examples/dft/optimized.hive --optimize_query_var=true
```

run hive program: in `runtimes/0.3/win`
```
gc -Raw "../../../examples/dft/test.txt" | ./a.exe p10000000
```

and i have 173 ms on my pc (!! THIS includes compying from slow @x64 array to local !!)

fully optimized version from clang gives `50ms` - so hive code is only 3 times slower than optimized c! (with copy to local array)


## Conclusion

This shows that hive code isn't very much slower than result of best C compilers.

To see how you can try to overcome them with more easy parallelism, see examples from topics gpu and parallelism.
