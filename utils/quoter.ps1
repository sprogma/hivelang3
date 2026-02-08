
param(
    [string]$BasePath=(pwd),
    [string]$TrackedFiles="\.c(pp)?$|\.h(pp)?$|\.asm$|\.fsi$|\.txt$|\.md$|\.md$|\.json$|\.ps1$",
    [switch]$NoInitialUpdate,
    [switch]$StartWatcher
)

function Update-OnChange
{
    param(
        [string]$BasePath,
        [string]$TrackedFiles
    )
    ls $BasePath -r -File | ? Name -match $TrackedFiles | % {
        $file = $_
        $numb = 0
        $initialContent = [IO.File]::ReadAllText($file);
        $intermediate = $initialContent -replace  "(\n|^)(( |\t)*(#|//|;))<<--Quote-->> from:(?<file>[^:]*):(?<reg>.*?)\r?\n(.|\n)*?(#|//|;)<<--QuoteEnd-->>", 
                                                 {"$(-join$_.Groups[1..2])<<--Quote:$($_.Groups['file']):$($_.Groups['reg'])"}
        $resultContent = $intermediate -replace   "(\n|^)(( |\t)*(#|//|;))<<--Quote:(?<file>[^:]*):(?<reg>.*?)\r?(?=(\n|$))", 
                                                 {
                                                  $numb++
                                                  $eee = $true
                                                  $fl = $_.Groups['file'].Value.Trim()
                                                  $reg=$_.Groups['reg'].Value
                                                  $c=$_.Groups[2].ToString()
                                                  if ([string]::IsNullOrEmpty($fl))
                                                  {
                                                      pushd (split-path $file)
                                                      try
                                                      {
                                                          $match = $reg | iex -ErrorAction Stop
                                                      }
                                                      catch
                                                      {
                                                          Write-Error "Error in file $file [quote#$numb]"
                                                          Write-Error $_
                                                      }
                                                      popd
                                                  }
                                                  else
                                                  {
                                                      $reqfile = (rvpa $fl -RelativeBasePath (Split-Path -Parent $file))
                                                      if ($reqfile -eq $null)
                                                      {
                                                          "$(-join$_.Groups[1..2])<<--Quote-->> from:$($_.Groups['file']):$reg`n"+
                                                          "$($_.Groups[2]) Error: Quoted file not found`n"+
                                                          "$($_.Groups[2])<<--QuoteEnd-->>"
                                                          $eee = $false
                                                      } else {
                                                          $content = [IO.File]::ReadAllText($reqfile)
                                                          $match = [regex]::Matches($content, $reg).Value -replace "(^|\n)", {"$_$c "}
                                                      }
                                                  }
                                                  if ($eee)
                                                  {
                                                      if ($match.Count -gt 0) {
                                                          "$(-join$_.Groups[1..2])<<--Quote-->> from:$($_.Groups['file']):$reg`n"+
                                                          "$($match-join"`n")`n"+
                                                          "$($_.Groups[2])<<--QuoteEnd-->>"
                                                      } else {
                                                          "$(-join$_.Groups[1..2])<<--Quote-->> from:$($_.Groups['file']):$reg`n"+
                                                          "$($_.Groups[2]) Error: can't find pattern in file`n"+
                                                          "$($_.Groups[2])<<--QuoteEnd-->>"
                                                          Write-Error "Failed to find pattern required from file: $file [pattern=$reg]"
                                                      }
                                                  }}
        if ($resultContent -ne $initialContent)
        {
            [IO.File]::WriteAllText($file, $resultContent)
            Write-Host "Updated file $file" -Fore green
        }
    }
}

if ($StartWatcher)
{
    $watcher = New-Object System.IO.FileSystemWatcher
    $watcher.Path = $BasePath
    $watcher.IncludeSubdirectories = $true
    $watcher.EnableRaisingEvents = $true
    $funcBody = gc "function:Update-OnChange"
    "Changed", "Created", "Deleted", "Renamed" | % {
        Register-ObjectEvent $watcher $_ -MessageData ($funcBody, $BasePath, $TrackedFiles) -Action {
            si "function:Update-OnChange" -Value $Event.MessageData[0]
            $path = $Event.SourceEventArgs.FullPath
            $changeType = $Event.SourceEventArgs.ChangeType
            Write-Host "Change: [$changeType] at file $path" -Fore yellow
            $Sender.EnableRaisingEvents = $false
            try {
                Update-OnChange -BasePath $Event.MessageData[1] -TrackedFiles $Event.MessageData[2]
            } finally {
                $Sender.EnableRaisingEvents = $true
            }
        }
    }
}

if (!$NoInitialUpdate)
{
    Update-OnChange -BasePath $BasePath -TrackedFiles $TrackedFiles
}


