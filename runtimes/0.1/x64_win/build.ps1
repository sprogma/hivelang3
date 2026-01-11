param([string]$CC="clang")
$FLAGS = ,"-fno-stack-protector", "-DUNICODE", "-D_UNICODE"
# $LF = ,"-luser32"
$jobs = @()
$jobs += Start-ThreadJob {   
    fasm runtime.asm asm.o
    & $using:CC runtime.c -c -o runtime.o -O3 -DNDEBUG -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE $FLAGS
    & $using:CC runtime.o asm.o -o a.exe -O3 "-Wl,/subsystem:console" $FLAGS $LF
}
$jobs += Start-ThreadJob {
    fasm runtime_dbg.asm asm_dbg.o
    & $using:CC runtime.c -c -o runtime_dbg.o -g -D_DEBUG -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE $FLAGS # -fsanitize=address
    & $using:CC runtime_dbg.o asm_dbg.o -g -o d.exe "-Wl,/subsystem:console" $FLAGS $LF # -fsanitize=address
}
$jobs | Wait-Job | Receive-Job
