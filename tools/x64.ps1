function Load-x64Database {
    param(
        [Parameter(Mandatory=$true)]
        [string]$HDBPath
    )

    $xml = [xml]::new()
    # $xml.PreserveWhitespace = $true 
        
    $xml.Load((Convert-Path -LiteralPath $HDBPath))
    $xml
}

function Get-x64CodeByAddress {
    param (
        [Parameter(Mandatory=$true)]
        [xml]$Database,
        [Parameter(Mandatory=$true)]
        [int]$TargetAddress
    )

    $mapping = $Database.database.addressToLine.mapping | ? {
        [int]$_.address.start -le $TargetAddress -and [int]$_.address.end -gt $TargetAddress
    } | Select-Object -First 1

    if (-not $mapping) { return }

    $source = $Database.database.source.'#cdata-section'
    $startPos = [int]$mapping.line.start
    $endPos = [int]$mapping.line.end

    $preText = $source.Substring(0, $startPos)
    
    $startLine = [regex]::Matches($preText, "\n").Count + 1
    
    $lastLF = $preText.LastIndexOf("`n")
    if ($lastLF -lt 0) { $startCol = $startPos + 1 }
    else { $startCol = $startPos - $lastLF }

    $matchText = $source.Substring($startPos, $endPos - $startPos)
    $linesInMatch = [regex]::Matches($matchText, "\n").Count
    
    $endLine = $startLine + $linesInMatch
    
    if ($linesInMatch -gt 0) {
        $endCol = ($matchText -split "\r?\n")[-1].Length
    } else {
        $endCol = $startCol + $matchText.Length - 1
    }

    return [PSCustomObject]@{
        PSTypeName    = 'Hivex64CodeSpanView'
        StartAddress  = $mapping.address.start
        EndAddress    = $mapping.address.end
        StartPos      = $startPos
        EndPos        = $endPos
        StartLine     = $startLine
        StartColumn   = $startCol
        EndLine       = $endLine
        EndColumn     = $endCol
        Database      = $Database
    }
}


function Get-x64AssemblyByAddress {
    param (
        [Parameter(Mandatory=$true)]
        [xml]$Database,
        [Parameter(Mandatory=$true)]
        [int]$TargetAddress
    )

    $bytes = gc "res.bin" -AsByteStream -First 20 | s -Skip 12
    $headerSize = [System.BitConverter]::ToInt64($bytes, 0)

    objdump -D -b binary -m i386:x86-64 -M intel --start-address=$headerSize res.bin

    Write-Host "Search for address $($headerSize + $TargetAddress)"
}
