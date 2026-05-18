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
Start-Process "a.exe" -ArgumentList 'p10000000', 'l' -RedirectStandardInput "../../../examples/qsort/test.txt" -NoNewWindow -Wait
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
Start-Process "a.exe" -ArgumentList 'p10000000', 'l', 'j8' -RedirectStandardInput "../../../examples/qsort/test.txt" -NoNewWindow -Wait
```

and i have 173 ms.

Here is some table:

j1 = 500ms
j2 = 400ms
j4 = 300ms
j8 = 200ms
j16 = 173ms

fully optimized version from clang gives `70ms` - so hive code is only 3 times slower than optimized c!


The main problem of this test, is that my functions doesn't have any stack, so they allocated large amount of memory on each
call (may be I will add arena allocator later)


## Conclusion

This shows that hive code isn't very much slower than result of best C compilers.

To see how you can try to overcome them with more easy parallelism, see examples from topics gpu and parallelism.

