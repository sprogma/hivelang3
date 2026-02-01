param(
    [string]$BasePath=(pwd),
    [string]$TrackedFiles="\.c(pp)?$|\.h(pp)?$|\.asm$|\.fsi$|\.txt$|\.md$|\.md$|\.json$|\.ps1$",
    [switch]$NoInitialUpdate,
    [switch]$NoWatcher
)

function Update-OnChange
{
    ls $BasePath -r | ? Name -match $TrackedFiles | % {
        $file = $_
        $initialContent = [IO.File]::ReadAllText($_) -replace 
                                                  "(\n|^)(( |\t)*(#|//|;))<<--Quote-->> from:(?<file>[^:]*):(?<reg>.*)\n(.|\n)*?(#|//|;)<<--QuoteEnd-->>", 
                                                 {"$(-join$_.Groups[1..2])<<--Quote:$($_.Groups['file']):$($_.Groups['reg'])"}
        $resultContent = $initialContent -replace "(\n|^)(( |\t)*(#|//|;))<<--Quote:(?<file>[^:]*):(?<reg>.*)(?=(\n|$))", 
                                                 {$file = (rvpa $_.Groups['file'].Value.Trim() -RelativeBasePath (Split-Path -Parent $file))
                                                  if ($file -eq $null)
                                                  {
                                                      "$(-join$_.Groups[1..2])<<--Quote-->> from:$($_.Groups['file']):$reg`n"+
                                                      "$($_.Groups[2]) Error: Quoted file not found`n"+
                                                      "$($_.Groups[2])<<--QuoteEnd-->>"
                                                  } else {
                                                      $content = [IO.File]::ReadAllText($file)
                                                      $match = [regex]::Match($content, ($reg=$_.Groups['reg'].Value))
                                                      if ($match.Success) {
                                                          "$(-join$_.Groups[1..2])<<--Quote-->> from:$($_.Groups['file']):$reg`n"+
                                                          "$($c=$_.Groups[2].ToString();$match.Value-replace"(^|\n)",{"$_$c "})`n"+
                                                          "$($_.Groups[2])<<--QuoteEnd-->>"
                                                      } else {
                                                          "$(-join$_.Groups[1..2])<<--Quote-->> from:$($_.Groups['file']):$reg`n"+
                                                          "$($_.Groups[2]) Error: can't pattern not found in file`n"+
                                                          "$($_.Groups[2])<<--QuoteEnd-->>"
                                                          Write-Error "Failed to find pattern required from file: $file"
                                                      }
                                                  }}
        if ($resultContent -ne $initialContent)
        {
            [IO.File]::WriteAllText($_, $resultContent)
        }
    }
}

if (!$NoWatcher)
{
    $watcher = New-Object System.IO.FileSystemWatcher
    $watcher.Path = $BasePath
    $watcher.IncludeSubdirectories = $true
    $watcher.EnableRaisingEvents = $true
    "Changed", "Created", "Deleted", "Renamed" | % { 
        Register-ObjectEvent $watcher $_ -Action {
            $path = $Event.SourceEventArgs.FullPath
            $changeType = $Event.SourceEventArgs.ChangeType
            Write-Verbose "Change: [$changeType] at file $path"
            Update-OnChange
        }
    }
}

if (!$NoInitialUpdate)
{
    Update-OnChange
}
