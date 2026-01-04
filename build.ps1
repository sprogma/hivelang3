pushd $PSScriptRoot
mkdir obj 2>$null
$f = (ls -r *.cpp)
$need = $false
$id=0
$h = ls -r *.hpp | % LastWriteTime | measure -Max | % Max*
$first = $true
$os = $f | %{
    $_ = Resolve-Path -Relative $_
    $o = join-path obj "$($_-replace"^\w+:|/|\\","-").o"
    $d1 = gi $o -ErrorAction SilentlyContinue 2>$null | % LastWriteTime
    $d2 = gi $_ -ErrorAction SilentlyContinue 2>$null | % LastWriteTime
    if ($d2 -gt $d1 -or $h -gt $d1)
    {
        if ($first)
        {
            Write-Progress -Activity "Building" -Status "Starting..." -PercentComplete 0
            $first = $false
        }
        Write-Host "Builing $_" -Fore yellow
        clang++ -c -std=gnu++2c $_ -o $o -g -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -fsanitize=address
        $need = $true
    }
    $id++
    if (!$first)
    {
        Write-Progress -Activity "Building" -Status "$_" -PercentComplete ([int]($id/$f.count * 100))
    }
    $o
}
if (!$first)
{
    Write-Progress -Activity "Building" -Status "Completed" -Completed
}
if ($need)
{
    Write-Host "Linking" -Fore yellow
    clang++ -std=gnu++2c $os -o a.exe -g -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -fsanitize=address
}
else
{
    Write-Host "Nothing changed" -Fore green
}

popd
