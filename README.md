# Hive Lang

![bees](./logo.png)

it contains

* Base compiler
* Providers:
    * x64 provider
    * gpu provider (opencl)
    * local provider (x64)
    * dllimport provider (win/x64)
* Runtime
    * Network object manager
    * Providers:
        * x64 provider
        * gpu provider (opencl)
        * local provider (x64)
        * dllimport provider (win/x64)


(last runtime version is located at `runtimes/0.3/win` and `runtimes/0.3/lin`)


## Build compiler

To build compiler, you need clang, supporting c++ of version `gnu++2c`.

On windows prefer running `.\build.ps1` in root:
```pwsh
.\build.ps1
```
(you need powershell of version 7+, default installed doesn't works).

Or, you can use linux way using make analogues? (don't officially supported)

----
On linux, use makefile in root.

----

Both variants uses precompiled grammar (see info in `grammar` directory).
And also source code uses `utils/quoter.ps1` (usually in runtime).


## Build runtime

To build runtime you need clang, supporting c of version `gnu2y`, targeting msvc stdlib, and fasm.

use pwsh of latest version, run in runtimes/0.3/win directory:
```pwsh
./build.ps1
```
It is incremental build, to make clean build delete `obj` directory.

If you don't want to use good shell, you can simply build all .cpp files in project together, to make compiler.
To build runtime use build.ps1.

## Build

to build hive program use
```
./a.exe [--input-file=filename] [--syntax-only=true] [--optimize_query_var=true]
```
if filename is missing, it will build `./example.hive` file (relative to current position).
Destination file is always `./res.bin` file (relative to current position).

use `--syntax-only=true` for only syntax check (can find not all logic errors wich can occur during full compilation).

use `--optimize_query_var=true` for some speed gain, and cose shrink.

## Running

to run program, use `runtime/0.3/win/a.exe` for stable programs, and `runtime/0.3/win/d.exe` for debug.
Your code is compiled to native, so you can use gdb for debugging it (but there is also many errors in runtime)

runtime will use `../../../res.bin` file, so if you run runtime from it's directory it will load `res.bin` 
produced by compiler.

Runtime starts from server console. Use `r` (`R`) command to run, `c` (`C`) to connect another hive server.
network connections are full of bugs for now, but can be used.

Commandline args
* use `jN` to set threads in one worker, all possible tasks will be run in parallel. (default 1)
* use `pN` to set time interval (in ms) of thread run timeout (for multiprocessing). (default )
* use `n` to not read data from console (in 'a' mode)
* use `h` to not end server after program end
* use `c` to set hive as protectorate (won't read and run main function, will wait for another server's connection.)
* use `--` to end arg parsing to pass input data (in 'd' mode)
