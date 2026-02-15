param([string]$CC="clang", [switch]$Sanitize)
$pseudoRelease = $true
$FLAGS = "-I.","-fno-stack-protector", "-DUNICODE", "-D_UNICODE", "-DFREESTANDING", "-municode", "-ffreestanding", "-nostdlib", "-mno-stack-arg-probe", "-fms-extensions", "-Wno-microsoft"
$LF = "-lgdi32", "-lshell32", "-lkernel32", "-lbcrypt", "-lws2_32", "-lOpenCL", "-Wl,-dynamicbase:no", "-Wl,-entry:entry" 
if ($Sanitize)
{
    $FLAGS = ($FLAGS, "-fsanitize=address" | % {$_})-notmatch"nostdlib|freestanding"
    $LF = $LF-notmatch"entry:entry"
}
$dbgLF = , "-Wl,/debug"
$rlsLF = ,"-flto", "-fuse-ld=lld"
$rlsFF = ,"-fno-unwind-tables", "-fno-asynchronous-unwind-tables"
$rlsDef = , "-DNDEBUG"
$dbgDef = , "-D_DEBUG"
$files = (ls -r *.c)
$jobs = @()
$jobs += Start-ThreadJob {   
    fasm runtime.asm obj/asm.o
    $z = @()
    $Speed = $null # "-O3"
    $using:files | % {
        $o = (rvpa -Path $_ -Relative -RelativeBasePath $PSScriptRoot)-replace"\.c$",".o"-replace"\\|/","-"
        $o = "obj/$o"
        $z += $o
        & $using:CC $_ -c -o $o $Speed -g $using:rlsDef -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE $using:FLAGS $using:rlsFF || Write-Host "Error in compilation"
    }
    & $using:CC $z obj/asm.o -g -o a.exe $Speed "-Wl,/subsystem:console" "-Wl,/MAP:release.map" $using:LF $using:FLAGS $using:rlsLF $using:rlsFF || Write-Host "Error in compilation"
}
$jobs += Start-ThreadJob {
    fasm runtime_dbg.asm obj/asm_dbg.o
    $z = @()
    $using:files | % {
        $o = (rvpa -Path $_ -Relative -RelativeBasePath $PSScriptRoot)-replace"\.c$",".o"-replace"\\|/","-"
        $o = "obj/dbg_$o"
        $z += $o
        & $using:CC $_ -c -o $o -g $using:dbgDef -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE $using:FLAGS  || Write-Host "Error in compilation" # -fsanitize=address
    }
    & $using:CC $z obj/asm_dbg.o -g -o d.exe "-Wl,/subsystem:console" "-Wl,/MAP:debug.map" $using:LF $using:FLAGS $using:dbgLF  || Write-Host "Error in compilation" # -fsanitize=address
}
$jobs | Wait-Job | Receive-Job
