param([string]$CC="clang")
$jobs = @()
$jobs += Start-ThreadJob {   
    fasm runtime.asm asm.o
    & $using:CC runtime.c -c -o runtime.o -g -DNDEBUG -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE
    & $using:CC runtime.o asm.o -o a.exe -g
}
$jobs += Start-ThreadJob {
    fasm runtime_dbg.asm asm_dbg.o
    & $using:CC runtime.c -c -o runtime_dbg.o -O3 -D_DEBUG -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE
    & $using:CC runtime_dbg.o asm_dbg.o -O3 -o d.exe
}
$jobs | Wait-Job | Receive-Job
