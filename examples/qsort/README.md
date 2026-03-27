# Qsort Test

## Information

Sorts array using quicksort.

To don't print array, program calculates value:

$\sum_{i=0}^{n} i \cdot a_i $

This test will compare speed of hive program, and same c program.

## Build

Build example.c
```
clang example.c -o a.exe
```

Build hive program: in root of project
```
./a.exe --input-file=./examples/qsort/example.hive --optimize_query_var=true
```

create test: (1e8 numbers, descending) (script is big to make it fast) (waring: it creates file of size ~1GB)
```
$code = @"
using System;
using System.IO;
public class FastWriter {
    public static void WriteNumbers(string path, long n) {
        using (StreamWriter sw = new StreamWriter(path)) {
            sw.WriteLine('r');
            sw.WriteLine(n);
            for (long i = n; i >= 1; i--) {
                sw.WriteLine(i);
            }
        }
    }
}
"@
Add-Type -TypeDefinition $code
[FastWriter]::WriteNumbers("test.txt", 100000000)
```

run c program
```
Start-Process "a.exe" -RedirectStandardInput "test.txt" -NoNewWindow -Wait
```

run hive program: in `runtimes/0.3/win`
```
Start-Process "a.exe" -ArgumentList 'p10000000' -RedirectStandardInput "../../../examples/qsort/test.txt" -NoNewWindow -Wait
```

on my machine i have
* c: 200ms
* hive: 1500ms

BUT! this is becouse of universal object system, used in hive program.
Now, let's build another version - the only diff is usage of `loc` provider for array allocation

Build hive program: in root of project
```
./a.exe --input-file=./examples/qsort/optimized.hive --optimize_query_var=true
```

run hive program: in `runtimes/0.3/win`
```
gc -Raw "../../../examples/qsort/test.txt" | ./a.exe p10000000
```

and i have 173 ms on my pc (!! THIS includes compying from slow @x64 array to local !!)

fully optimized version from clang gives `50ms` - so hive code is only 3 times slower than optimized c! (with copy to local array)


## Conclusion

This shows that hive code isn't very much slower than result of best C compilers.

To see how you can try to overcome them with more easy parallelism, see examples from topics gpu and parallelism.

