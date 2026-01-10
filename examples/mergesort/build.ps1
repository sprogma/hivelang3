$src = (Join-Path $PSScriptRoot .\example.c)
$res = Join-Path $PSScriptRoot "./a.exe"

if ((gi $res 2>$null).LastWriteTime -lt (gi $src).LastWriteTime)
{
    gcc $src -Ofast -o $res
}

[pscustomobject]@{
    example="mergesort"
    name="n=1e6"
    executable=$res
    input=Join-Path $PSScriptRoot "test6.txt"
}
[pscustomobject]@{
    example="mergesort"
    name="n=1e3"
    executable=$res
    input=Join-Path $PSScriptRoot "test3.txt"
}
