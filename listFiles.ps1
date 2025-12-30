ls *.cpp,*.hpp -r | %{ [pscustomobject]@{name=(Resolve-Path -Relative $_); lines=gc $_ -raw | measure -Line|% Lines}} | sort name
