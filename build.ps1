param(
    # You can select compiler using this flag, clang++ is recommended. (need at least support of gnu++2c std from compiler)
    [string]$CppCompiler="clang++",
    # You can select number of parallel builds
    [int]$Threads=1,
    # Enable sanitizers [strongly slows down code]
    [switch]$Sanitize
)

Write-Host "Waring: Using precompiled grammar files, to update them use guide in `grammar` directory" -Fore Red

pushd $PSScriptRoot

# create obj directory
mkdir obj 2>$null

$FLAGS = (,"-O3") + (!$Sanitize ? @() : @(,"-fsanitize=address")) + ($IsLinux ? @(,"-Wno-format") : @())
$FLAGS = @()

# look for files & headers
$f = (ls -r *.cpp)
$h = ls -r *.hpp | % LastWriteTime | measure -Max | % Max*

# build them
$state = [System.Collections.Concurrent.ConcurrentDictionary[string, int]]::new()
$state.TryAdd("id", 0) | Out-Null

$senitel = New-Object object
$os = $f | % -ThrottleLimit $Threads -Parallel {

    # 'atomic increment'
    $id = ($using:state).AddOrUpdate("id", 1, { param($key, $oldValue) $oldValue + 1 })
    
    $f = Resolve-Path -Relative $_
    $o = join-path obj "$($f-replace"^\w+:|/|\\","-").o"
    $d1 = gi $o -ErrorAction SilentlyContinue 2>$null | % LastWriteTime
    $d2 = gi $f -ErrorAction SilentlyContinue 2>$null | % LastWriteTime
    # if .o is older than .cpp or any .hpp
    if ($d2 -gt $d1 -or $using:h -gt $d1)
    {
        Write-Host "Builing $f" -Fore yellow
        & $using:CppCompiler -c -std=gnu++2c $f -o $o -g -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE $using:FLAGS
        # return mark of need of linking
        $using:senitel
    }
    Write-Progress -Activity "Building" -Status "$f" -PercentComplete ([int]($id/$using:f.count * 100))
    # return data
    $o
}
Write-Progress -Activity "Building" -Status "Completed" -Completed
if ($senitel -in $os)
{
    Write-Host "Linking" -Fore yellow
    & $CppCompiler -std=gnu++2c ($os|?{$_-ne$senitel}) -o a.exe -g -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE $FLAGS 
}
else
{
    Write-Host "Nothing changed" -Fore green
}

popd
