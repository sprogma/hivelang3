# TODO: write this file
gci -Directory | % { & (Join-Path $_.FullName build.ps1) } | % {
    # time $_.executable -InputFile $_.input -MeasureTime 10
    $_
}
