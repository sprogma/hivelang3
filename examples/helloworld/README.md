# Hello world program

## Simplest program
[example.hive](./example.hive)
```hive

@x64 # select default provider, (more info in providers example)

# set some tags of main function
[export:yes] # this function is external, it's symbol will be visible from inside
[entry:yes] # this function is entry, program will start from here
# function declaration
(i64[] input)main(i32? res) 
# declaration use style (<inputs>)<name>(<outputs>);
# in this case we declare i64[] input -> array of signed 64 bit integers.
# result of main function is promise (kind of pointer) of type signed 32 bit integer, named res
{ # open code block
    res <- 179; # set into res value 179. (<- is set operator)
} # end code block

```

This code declares main function which return 179.

Build program: in root of project
```
./a.exe --input-file=./examples/helloworld/example.hive
```

run it: in `runtimes/0.3/win` (`n` means no input is need for program)
```
./a.exe n
```

you will see big log, which will end on `Program exited with code 00000000000000B3`.
this means program worked. (0xB3 is 179).


## But this don't print "Hello World!"

Actually, language doesn't have stdlib for now, so have no "print" or "write" function.

You must interact with system using dllimport provider, if you simply want code which will print text here is is: (with a bit of comments)
[helloworld.hive](./helloworld.hive)
```hive
@x64

[export:yes]
[entry:yes]
(i64[] input)main(i32? res) 
{
    i64 handle;
    handle <- ?(-11)GetStdHandle@dll(*); # get stdout handle
    i16[] msg;
    msg <- new i16[]"Hello World!\n"; # create message
    res <- ?(handle, msg, `i32`?msg, `i64`0, `i64`0)WriteConsole@dll(*); # print it
}

# load winapi function from dll:

[dllimport:kernel32.dll]
[dllimport.entry:GetStdHandle]
(i32 nStdHandle)GetStdHandle(i64? hHandle);

[dllimport:kernel32.dll]
[dllimport.entry:WriteConsoleW]
(
    i64     hConsoleOutput,
    i16[]   lpBuffer,
    i32     nNumberOfCharsToWrite,
    i32?    lpNumberOfCharsWritten,
    i64     lpReserved
)WriteConsole(i32? result);
```
Build program: in root of project
```
./a.exe --input-file=./examples/helloworld/helloworld.hive
```
run it: in `runtimes/0.3/win` (`n` means no input is need for program)
```
./a.exe n
```

now you can see something like this:
```
|       thread 00      |  Wait | Queue | RPmiss | ROreq | RIreq |
|  exec / done / stall |       |       |        |       |       |
Hello World!
ShedulerInstance completed
Program finished in 15 ms
Program exited with code 0000000000000001
```
\- so this really print hello world, it is really don't hard.
