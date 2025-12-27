param([string]$Path="grammar")

pushd $PSScriptRoot

Write-Host "Generating f# code..." -Foreground green

$id = @{
    S=1;
    Sn=2;
    identifer=3;
    integer=4;
    float=5;
}
$special = $id.Keys
$unbound = $special + (
    gc $Path -raw | sls "\b\s*rule\s*""(\w+)""\s*\[" -AllMatches | % Matches | % {$_.Groups[1].Value}
) | %{Write-Host $_; $_}

@"
// this file was generated using grammar_gen.ps1

let prefix = "<RULE:$(Get-Random)>"

$(
    ($unbound | %{
        "let $_ = prefix + ""$_"""
    }) -join "`n"
)
    
open System.IO

type Expr = Terminal of string | NonTerminal of string
type Variant = Expr list * Expr list * Expr list
type Rule = Variant list
type Grammar = Map<string, Rule>

let mkTok (s:string) =
    if s.StartsWith(prefix) then NonTerminal (s.Substring(prefix.Length))
    else Terminal s

let cvt xs = xs |> List.map mkTok
let v p per s = (cvt p, cvt per, cvt s)
let rule name variants = name, variants

let grammar =
    [
$((gc $Path | %{"        $_"})-join"`n")
    ] |> Map.ofList
    
let keyToIndexMap inputMap =
    inputMap
    |> Map.toSeq
    |> Seq.mapi (fun i (k, _) -> (k, i + 20))
    |> Map.ofSeq

let indexMap = keyToIndexMap grammar $(($special | %{" |> Map.add ""$_"" $($id[$_])"})-join" ")

let printTerm = function 
    | Terminal s -> sprintf "\"%s\"" s 
    | NonTerminal s -> sprintf "array + %d" (indexMap[s])
let printAtom xs = xs |> List.map printTerm |> String.concat ", "
let printVariant (prf,per,suf) = $"RuleVariant(vector<Atom>{{{printAtom prf}}}, vector<Atom>{{{printAtom per}}}, vector<Atom>{{{printAtom suf}}})"
let codegen = 
    grammar 
    |> Map.toList 
    |> List.map (fun (k, v) -> 
                 $"""new(array + {indexMap[k]})Rule({indexMap[k]}, "{k}", vector<RuleVariant>{{{v |> List.map printVariant |> String.concat ", "}}});""" ) 
    |> String.concat "\n"

let code = "// this sourse file was generated using grammar.fsx\n\
            #include <vector>\n\
            #include <map>\n\
            using namespace std;\n\
            #include \"ast.hpp\"\n\
            Rule *grammar;\n\
            int64_t grammar_len;\n\
            Rule *generateGrammar()\n\
            {\n\
                // allocate memory for all elements\n\
                grammar_len = " + (grammar.Count + 21).ToString() + ";\n\
                Rule *array = (Rule *)malloc(sizeof(*array) * grammar_len);\n\
            " + File.ReadAllText("grammar.inc") + codegen + "\n\
            return array;\n\
            }"

File.WriteAllText("./grammar.cpp", code)
"@ >grammar.fsx

Write-Host "Building" -Foreground green
dotnet fsi grammar.fsx
if ($?) { Write-Host "Build finished" -Foreground green } else { Write-Host "Error at build" -Foreground red }

popd
