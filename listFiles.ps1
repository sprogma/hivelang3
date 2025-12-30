ls *.cpp,*.hpp -r | %{ [pscustomobject]@{name=$_.Name; lines=gc $_ -raw | measure -Line|% Lines}} | sort name
