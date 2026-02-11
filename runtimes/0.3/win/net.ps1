param(
    [switch]$Release,
    [switch]$Debugger
)

function New-TCPConnection {
    param(
        [Parameter(Mandatory=$true)][string] $RemoteHost,
        [Parameter(Mandatory=$true)][int] $Port
    )
    try 
    {
        $client = New-Object System.Net.Sockets.TcpClient($RemoteHost, $Port)
        $stream = $client.GetStream()
        
        [PSCustomObject]@{
            Client = $client
            Stream = $stream
        }
    } 
    catch 
    {
        Write-Error "Failed to connect to $RemoteHost on port $Port : $_"
    }
}

function Send-TCPBytes {
    param(
        [Parameter(Mandatory=$true)] $Connection,
        [Parameter(Mandatory=$true, ValueFromPipeline=$true)][byte[]] $Bytes
    )
    process {
        if ($Connection.Client.Connected) 
        {
            $Connection.Stream.Write($Bytes, 0, $Bytes.Length)
        } 
        else 
        {
            Write-Warning "Connection is not active."
        } 
    }
    end {    
        if ($Connection.Client.Connected)
        {
            $Connection.Stream.Flush()
        }
        else 
        {
            Write-Warning "Connection is not active."
        }
    }
}

function Close-TCPConnection {
    param(
        [Parameter(Mandatory=$true, ValueFromPipeline=$true)] $Connection
    )
    process 
    {
        if ($null -ne $Connection.Stream) { $Connection.Stream.Close() }
        if ($null -ne $Connection.Client) { $Connection.Client.Close() }
        Write-Debug "Connection to $($Connection.Client.Client.RemoteEndPoint) closed."
    }
}

function Start-Hive
{
    param(
        [Parameter(Mandatory=$true)][string]$Executable,
        [string[]]$ArgumentList
    )
    $p = Get-Random -min 16000 -max 32000
    [pscustomobject]@{
        Port=$p
        Executable=$Executable
        ArgumentList=$ArgumentList
        Input=,"p $p"
    }
}

function Deploy-System
{
    param(
        [Parameter(Mandatory=$true, ValueFromPipeline=$true)] $hive
    )
    begin
    {
        $ToRun = @()
        $tmps = @()
        $res = @()
    }
    process 
    {
        $hive.Input += "r`n0"
        $tmp = New-TemporaryFile
        $hive.Input -join "`n" | Set-Content $tmp
        $hive.ArgumentList = $hive.ArgumentList -replace "@", "$($tmp.FullName)"
        Write-Host $hive.ArgumentList
        #  -RedirectStandardInput $tmp
        $ToRun += @{FilePath=$hive.Executable; ArgumentList=$hive.ArgumentList; WindowStyle="Normal"; PassThru=$true}
        $tmps += $tmp
    }
    end
    {
        Write-Host "System deployed" -Fore green
        Write-Host "[waiting for closing all nodes]" -Fore darkgray
        $ToRun | % -ThrottleLimit ($ToRun.Count * 2) -Parallel {Start-Process @_} | Wait-Process
        $tmps | rm
        Write-Host "All processes terminated"
    }
}

function Connect-Hive
{
    param(
        $hive1,
        $hive2
    )
    $hive1.Input += ,"c ::1 $($hive2.Port)"
}



. .\topology.ps1 -Release:$Release -Debugger:$Debugger
