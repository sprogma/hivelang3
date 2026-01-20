param([string]$CC="clang")
$FLAGS = ,"-fno-stack-protector", "-DUNICODE", "-D_UNICODE", "-municode", "-ffreestanding", "-nostdlib", "-mno-stack-arg-probe"
$LF = ,"-lws2_32", "-Wl,-entry:entry", "-Wl,-dynamicbase:no" # used flags
# flags without them LoadLibrary doent works?
$LF = $LF, "-luser32", "-lkernel32", "-lwinmm", "-lgdi32" | %{$_}
$dbgLF = ,"-lshell32", "-Wl,/debug"
$rlsLF = ,"-flto", "-fuse-ld=lld"
$rlsFF = ,"-fno-unwind-tables", "-fno-asynchronous-unwind-tables"
$files = (ls *.c)
$jobs = @()
$jobs += Start-ThreadJob {   
    fasm runtime.asm asm.o
    $z = @()
    $using:files | % {
        $o = $_-replace"\.c$",".o"
        $z += $o
        & $using:CC $_ -c -o $o -O3 -DNDEBUG -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE $using:FLAGS $using:rlsFF
    }
    & $using:CC $z asm.o -o a.exe -O3 "-Wl,/subsystem:console" "-Wl,/MAP:release.map" $using:FLAGS $using:LF $using:rlsLF $using:rlsFF
}
$jobs += Start-ThreadJob {
    fasm runtime_dbg.asm asm_dbg.o
    $z = @()
    $using:files | % {
        $o = $_-replace"\.c$","_dbg.o"
        $z += $o
        & $using:CC $_ -c -o $o -g -D_DEBUG -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE $using:FLAGS # -fsanitize=address
    }
    & $using:CC $z asm_dbg.o -g -o d.exe "-Wl,/subsystem:console" "-Wl,/MAP:debug.map" $using:FLAGS $using:LF $using:dbgLF # -fsanitize=address
}
$jobs | Wait-Job | Receive-Job
