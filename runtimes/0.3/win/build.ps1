param([string]$CC="clang", [switch]$Sanitize)
$pseudoRelease = $true
$FLAGS = ,"-fno-stack-protector", "-DUNICODE", "-D_UNICODE", "-DFREESTANDING", "-municode", "-ffreestanding", "-nostdlib", "-mno-stack-arg-probe", "-fms-extensions", "-Wno-microsoft"
if ($Sanitize)
{
    $FLAGS = $FLAGS, "-fsanitize=address" | % {$_} | ? {$_-notmatch"nostdlib|freestanding"}
    $LF = "-lshell32", "-lkernel32", "-lbcrypt", "-lws2_32", "-Wl,-dynamicbase:no" #, "-Wl,-entry:entry" 
}
else
{
    $LF = "-lshell32", "-lkernel32", "-lbcrypt", "-lws2_32", "-Wl,-dynamicbase:no", "-Wl,-entry:entry" 
}
$dbgLF = , "-Wl,/debug"
$rlsLF = ,"-flto", "-fuse-ld=lld"
$rlsFF = ,"-fno-unwind-tables", "-fno-asynchronous-unwind-tables"
$files = (ls *.c)
$jobs = @()
$jobs += Start-ThreadJob {   
    fasm runtime.asm obj/asm.o
    $z = @()
    $Speed = $null # "-O3"
    $using:files | % {
        $o = $_.Name-replace"\.c$",".o"
        $o = "obj/$o"
        $z += $o
        & $using:CC $_ -c -o $o $Speed -g -DNDEBUG -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE $using:FLAGS $using:rlsFF || Write-Host "Error in compilation"
    }
    & $using:CC $z obj/asm.o -g -o a.exe $Speed "-Wl,/subsystem:console" "-Wl,/MAP:release.map" $using:LF $using:FLAGS $using:rlsLF $using:rlsFF || Write-Host "Error in compilation"
}
$jobs += Start-ThreadJob {
    fasm runtime_dbg.asm obj/asm_dbg.o
    $z = @()
    $using:files | % {
        $o = $_.Name-replace"\.c$","_dbg.o"
        $o = "obj/$o"
        $z += $o
        & $using:CC $_ -c -o $o -g -D_DEBUG -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE $using:FLAGS  || Write-Host "Error in compilation" # -fsanitize=address
    }
    & $using:CC $z obj/asm_dbg.o -g -o d.exe "-Wl,/subsystem:console" "-Wl,/MAP:debug.map" $using:LF $using:FLAGS $using:dbgLF  || Write-Host "Error in compilation" # -fsanitize=address
}
$jobs | Wait-Job | Receive-Job
