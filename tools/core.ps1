Update-FormatData -AppendPath "$PSScriptRoot/HiveTools.types.ps1xml"

function Get-ColorTokens
{
    param (
        [Parameter(Mandatory=$true)]
        [string]$Code
    )

    $Rules = @(
        ($PSStyle.Foreground.White      , $null, @('\b[A-Z_][0-9A-Z_]+\b')),
        ($PSStyle.Foreground.Cyan       , $null, @('\b(i(8|16|32|64)|u(8|16|32|64)|f(32|64))\b')),
        ($PSStyle.Foreground.Red        , $null, @('\b(struct|union|class)\b')),
        ($PSStyle.Foreground.Red        , $null, @('\b(while|match)\b')),
        ($PSStyle.Foreground.Green      , $null, @('\[.*:.*\]')),
        ($PSStyle.Foreground.Red        , $null, @('\?', '[-+*/%=<>.;~&|^!]|\bnew\b')),
        ($PSStyle.Foreground.White      , $null, @('[(){}]|\[[^: ]*\]')),
        ($PSStyle.Foreground.Magenta    , $null, @( '\b(([0-9]*[.][0-9]+|[0-9]+[.][0-9]*)([Ee][+-]?[0-9]+)?|[0-9]+[Ee][+-]?[0-9]+)\b',
                                                    '\b0x[0-9A-Fa-f]+\b',
                                                    '\b[0-9]+\b'
                                                  )),
        ($PSStyle.Foreground.Yellow     , $null, @('"(?:\\.|[^"\\])*"', "'(?:\\.|[^'\\])*'")),
        ($PSStyle.Foreground.BrightBlack, $null, @('#.*$'))
    )

    $Tokens = New-Object System.Collections.Generic.List[PSCustomObject]

    foreach ($Rule in $Rules) {
        foreach ($Pattern in $Rule[2]) {
            $Matches = [System.Text.RegularExpressions.Regex]::Matches($Code, $Pattern)
            foreach ($Match in $Matches) {
                $Tokens.Add([PSCustomObject]@{
                    From  = $Match.Index
                    To    = $Match.Index + $Match.Length
                    Foreground = $Rule[0]
                    Background = $Rule[1]
                })
            }
        }
    }

    return $Tokens
}


function Get-ColoredString {
    param (
        [string]$Code,
        $Tokens
    )
    $chars = $Code.ToCharArray()
    $styleMap = New-Object 'System.Object[]' $chars.Count

    foreach ($token in $Tokens) 
    {
        for ($i = $token.From; $i -lt $token.To; $i++) 
        {
            if ($i -lt 0 -or $i -ge $chars.Count) { continue }
            
            if ($null -eq $styleMap[$i]) { $styleMap[$i] = @{ fg = $null; bg = $null } }
            if ($null -ne $token.Foreground) { $styleMap[$i].fg = $token.Foreground }
            if ($null -ne $token.Background) { $styleMap[$i].bg = $token.Background }
        }
    }

    $result = New-Object System.Text.StringBuilder
    $lastFg = $null
    $lastBg = $null
    
    [void]$result.Append($PSStyle.Reset)

    for ($i = 0; $i -lt $chars.Count; $i++) {
        $style = $styleMap[$i]
        $fg = $style.fg ?? $PSStyle.Reset
        $bg = $style.bg ?? $PSStyle.Reset

        if ($fg -ne $lastFg -or $bg -ne $lastBg) {
            if ($null -ne $style.fg)
            {
                [void]$result.Append($bg)
                [void]$result.Append($fg)
            }
            else
            {
                [void]$result.Append($fg)
                [void]$result.Append($bg)
            }
            $lastFg = $fg
            $lastBg = $bg
        }
        [void]$result.Append($chars[$i])
    }
    
    [void]$result.Append($PSStyle.Reset)
    return $result.ToString()
}

