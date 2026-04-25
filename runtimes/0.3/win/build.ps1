param([string]$CC="clang", [switch]$Sanitize)
pushd $PSScriptRoot
$pseudoRelease = $true
$FLAGS = "-I.", "-I../common","-fno-stack-protector", "-DUNICODE", "-D_UNICODE", "-DFREESTANDING", "-municode", "-ffreestanding", "-nostdlib", "-mno-stack-arg-probe", "-fms-extensions", "-Wno-microsoft"
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
$files = @(ls -r *.c) + @(ls ../common/*.c -r)
$h = @(ls -r *.h) + @(ls ../common/*.h -r)
$jobs = @()
[void](mkdir obj -Force)

$jobs += Start-ThreadJob {   
    fasm runtime.asm obj/asm.o
    $z = @()
    #$Speed = "-O3", "-mavx2"
    $using:files | % {$id=0}{
        $o = (rvpa -Path $_ -Relative -RelativeBasePath $PSScriptRoot)-replace"\.c$",".o"-replace"\\|/","-"
        $o = "obj/$o"
        $z += $o
        if ((@($_.LastWriteTime) + ($using:h).LastWriteTime) -gt (gi $o 2>$null).LastWriteTime)
        {
            Write-Host "rebuild $_" -Fore green
            & $using:CC $_ -c -o $o $Speed -g $using:rlsDef -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE $using:FLAGS $using:rlsFF || Write-Host "Error in compilation"
        }
        else
        {
            Write-Host "skip building $_" -Fore yellow
        }
        Write-Progress -Id 1 -Activity "Building target Release" -Status "building..." -PercentComplete ([int](100.0*($id++)/($using:files).Count))
    }
    Write-Progress -Id 1 -Activity "Building target Release" -Status "Linking..." -PercentComplete 100
    & $using:CC $z obj/asm.o -g -o a.exe $Speed "-Wl,/subsystem:console" "-Wl,/MAP:release.map" $using:LF $using:FLAGS $using:rlsLF $using:rlsFF || Write-Host "Error in compilation"
    Write-Progress -Id 1 -Activity "Building target Release" -Completed
}
$jobs += Start-ThreadJob {
    fasm runtime_dbg.asm obj/asm_dbg.o
    $z = @()
    $using:files | % {$id=0}{
        $o = (rvpa -Path $_ -Relative -RelativeBasePath $PSScriptRoot)-replace"\.c$",".o"-replace"\\|/","-"
        $o = "obj/dbg_$o"
        $z += $o
        if ((@($_.LastWriteTime) + ($using:h).LastWriteTime) -gt (gi $o 2>$null).LastWriteTime)
        {
            Write-Host "rebuild $_" -Fore green
            & $using:CC $_ -c -o $o -g $using:dbgDef -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE $using:FLAGS  || Write-Host "Error in compilation" # -fsanitize=address
        }
        else
        {
            Write-Host "skip building $_" -Fore yellow
        }
        Write-Progress -Id 2 -Activity "Building target Debug" -Status "building..." -PercentComplete ([int](100.0*($id++)/($using:files).Count))
    }
    Write-Progress -Id 2 -Activity "Building target Debug" -Status "Linking..." -PercentComplete 100
    & $using:CC $z obj/asm_dbg.o -g -o d.exe "-Wl,/subsystem:console" "-Wl,/MAP:debug.map" $using:LF $using:FLAGS $using:dbgLF  || Write-Host "Error in compilation" # -fsanitize=address
    Write-Progress -Id 2 -Activity "Building target Debug" -Completed
}
$jobs | Receive-Job -Wait
popd
