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
