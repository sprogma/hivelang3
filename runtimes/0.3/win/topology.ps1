param(
    [switch]$Release,
    [switch]$Debugger
)

$exe = "pwsh"
if ($Debugger)
{
    if ($Release)
    {
        $ag = "-NoExit", "-c", 'gc "@"; lldb a.exe -o """process launch -i @ -- c j3"""'
        $agMain = "-NoExit", "-c", 'gc "@"; lldb a.exe -o """process launch -i @ -- j3"""'
    }
    else
    {
        $ag = "-NoExit", "-c", 'gc "@"; lldb d.exe -o """process launch -i @ -- c"""'
        $agMain = "-NoExit", "-c", 'gc "@"; lldb d.exe -o """process launch -i @ --"""'
    }
}
else
{
    if ($Release)
    {
        $ag = "-NoExit", "-c", 'gc "@"; gc "@" | .\a.exe c j3'
        $agMain = "-NoExit", "-c", 'gc "@"; gc "@" | .\a.exe j3'
    }
    else
    {
        $ag = "-NoExit", "-c", 'gc "@"; gc "@" | .\d.exe c -- 3 5'
        $agMain = "-NoExit", "-c", 'gc "@"; gc "@" | .\d.exe -- 3 5'
    }
}

# $h1 = Start-Hive -Executable $exe -ArgumentList $agMain
# $h2 = Start-Hive -Executable $exe -ArgumentList $ag
# $h3 = Start-Hive -Executable $exe -ArgumentList $ag
# $h4 = Start-Hive -Executable $exe -ArgumentList $ag
# $h5 = Start-Hive -Executable $exe -ArgumentList $ag
# 
# Connect-Hive $h1 $h2
# Connect-Hive $h2 $h3
# Connect-Hive $h3 $h4
# Connect-Hive $h4 $h5
# Connect-Hive $h1 $h5
# 
# $h1, $h2, $h3, $h4, $h5 | Deploy-System

$h1 = Start-Hive -Executable $exe -ArgumentList $agMain
$h2 = Start-Hive -Executable $exe -ArgumentList $ag

Connect-Hive $h1 $h2

$h1, $h2 | Deploy-System
