// this file was generated using grammar_gen.ps1

let prefix = "<RULE:1502599581>"

let identifer = prefix + "identifer"
let float = prefix + "float"
let Sn = prefix + "Sn"
let integer = prefix + "integer"
let S = prefix + "S"
let Global = prefix + "Global"
let Expr = prefix + "Expr"
let Term = prefix + "Term"
    
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
        rule "Global" [
            v [S] [] []
            v [Expr] [] []
        ]
        rule "Expr" [
            v ["a"; Term; "b"] [] []
        ]
        rule "Term" [
            v ["t"] [] []
            v ["v"] [] []
        ]
    ] |> Map.ofList
    
let keyToIndexMap inputMap =
    inputMap
    |> Map.toSeq
    |> Seq.mapi (fun i (k, _) -> (k, i + 20))
    |> Map.ofSeq

let indexMap = keyToIndexMap grammar  |> Map.add "identifer" 3  |> Map.add "float" 5  |> Map.add "Sn" 2  |> Map.add "integer" 4  |> Map.add "S" 1

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
