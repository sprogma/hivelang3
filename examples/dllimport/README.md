# Dllimport



Build program: in root of project
```
./a.exe --input-file=./examples/dllimport/example.hive
```

run it: in `runtimes/0.3/win` (`n` means no input is need for program)
```
./a.exe n
```

you will see text box.

## Creating

To load dll function you must

1. Create it's prototype matching types (see Types section)
2. Add `[dllimport:]` attribute and select library to load
3. If there is need, add optional `[dllimport.entry:]` or `[dllimport.out]` attributes (see info in comments in example.hive)
4. Call it __with @dll provider!__

## Types

conversion is very simple.

* T[] -> pointer (structure is packed)
* T? -> pointer
* class T -> pointer
* struct T -> scalar
* scalar -> scalar

Return type is T + promise. For example

```
int foo(int *, int, char)
```
->
```
(i32? a, i32 b, i8 c)foo(i32? res) # pointer as pointer to one int
# or
(i32[] a, i32 b, i8 c)foo(i32? res) # pointer as array
# or
(i64 a, i32 b, i8 c)foo(i32? res) # pointer as int
```

