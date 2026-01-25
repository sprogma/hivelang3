
$exe = "pwsh"
$ag = "-NoExit", "-c", 'gc "@"; lldb d.exe -o """process launch -i @ -- 3 4"""'

$h1 = Start-Hive -Executable $exe -ArgumentList $ag
$h2 = Start-Hive -Executable $exe -ArgumentList $ag
$h3 = Start-Hive -Executable $exe -ArgumentList $ag

Connect-Hive $h1 $h2
Connect-Hive $h2 $h3
Connect-Hive $h1 $h3

$h1, $h2, $h3 | Deploy-System
