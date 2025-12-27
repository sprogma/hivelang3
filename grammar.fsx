// this file was generated using grammar_gen.ps1

let prefix = "<RULE:2029986196>"

let Sn = prefix + "Sn"
let float = prefix + "float"
let integer = prefix + "integer"
let identifer = prefix + "identifer"
let identifer_or_number = prefix + "identifer_or_number"
let S = prefix + "S"
let Global = prefix + "Global"
let _record = prefix + "_record"
let _union = prefix + "_union"
let _class = prefix + "_class"
let var_type = prefix + "var_type"
let var_type_moditifer = prefix + "var_type_moditifer"
let worker = prefix + "worker"
let worker_decl = prefix + "worker_decl"
let attribute_list = prefix + "attribute_list"
let arguments_list = prefix + "arguments_list"
let code_block = prefix + "code_block"
let statement = prefix + "statement"
let case_branch = prefix + "case_branch"
let var_declaration = prefix + "var_declaration"
let expression = prefix + "expression"
let call_arg_list = prefix + "call_arg_list"
let call_result_list = prefix + "call_result_list"
let result_list_identifer = prefix + "result_list_identifer"
let SetOperation = prefix + "SetOperation"
let CompareOperation = prefix + "CompareOperation"
let cmp_op = prefix + "cmp_op"
let AddOperation = prefix + "AddOperation"
let add_op = prefix + "add_op"
let MulOperation = prefix + "MulOperation"
let mul_op = prefix + "mul_op"
let QueryOperation = prefix + "QueryOperation"
let IndexOperation = prefix + "IndexOperation"
let SimpleTerm = prefix + "SimpleTerm"
let new_operator = prefix + "new_operator"
    
open System.IO

type Expr = Terminal of string | NonTerminal of string
type Variant = Expr list * Expr list * Expr list
type Rule = Variant list
type Grammar = Map<string, Rule>

let mkTok (s:string) =
    if s.StartsWith(prefix) then NonTerminal (s.Substring(prefix.Length))
    else Terminal s

let convertToTypes xs = xs |> List.map mkTok
let v p per s = (convertToTypes p, convertToTypes per, convertToTypes s)
let rule name variants = name, variants

let grammar =
    [
        rule "Global" [
            v [S] [] []
            v [_record] [] []
            v [_union] [] []
            v [_class] [] []
            v [worker] [] []
            v [worker_decl] [] []
        ]
        rule "_record" [
            v [Sn; "struct"; S; identifer; Sn; "{"] [var_type; S; identifer; Sn; ";"] [Sn; "}"]
        ]
        rule "_union" [
            v [Sn; "union"; S; identifer; Sn; "{"] [var_type; S; identifer; Sn; ";"] [Sn; "}"]
        ]
        rule "_class" [
            v [Sn; "class"; S; identifer; Sn; "{"] [var_type; S; identifer; Sn; ";"] [Sn; "}"]
        ]
        rule "var_type" [
            v [Sn; identifer] [var_type_moditifer] []
        ]
        rule "var_type_moditifer" [
            v [Sn; "[]"] [] []
            v [Sn; "|"] [] []
            v [Sn; "?"] [] []
        ]
        rule "worker" [
            v [attribute_list; Sn; "("; arguments_list; Sn; ")"; Sn; identifer; Sn; "("; arguments_list; Sn; ")"; code_block] [] []
        ]
        rule "worker_decl" [
            v [attribute_list; Sn; "("; arguments_list; Sn; ")"; Sn; identifer; Sn; "("; arguments_list; Sn; ")"; Sn; ";"] [] []
        ]
        rule "attribute_list" [
            v [] [Sn; "["; Sn; identifer_or_number; Sn; ":"; Sn; identifer_or_number; Sn; "]"] []
        ]
        rule "arguments_list" [
            v [] [var_type; S; identifer; Sn; ","] [var_type; S; identifer]
            v [] [var_type; S; identifer; Sn; ","] []
        ]
        rule "code_block" [
            v [Sn; "{"] [statement] [Sn; "}"]
        ]
        rule "statement" [
            v [var_declaration; Sn; ";"] [] []
            v [expression; Sn; ";"] [] []
            v [Sn; "while"; expression; code_block] [] []
            v [Sn; "match"; expression; Sn; "{"] [case_branch] [Sn; "}"]
        ]
        rule "case_branch" [
            v [expression; code_block] [] []
        ]
        rule "var_declaration" [
            v [var_type; S; identifer] [Sn; ","; Sn; identifer] [Sn; ","]
            v [var_type; S; identifer] [Sn; ","; Sn; identifer] []
        ]
        rule "expression" [
            v [SetOperation] [] []
        ]
        rule "call_arg_list" [
            v [] [expression; Sn; ","] [expression]
            v [] [expression; Sn; ","] []
        ]
        rule "call_result_list" [
            v [] [result_list_identifer; Sn; ","] [result_list_identifer]
            v [] [result_list_identifer; Sn; ","] []
        ]
        rule "result_list_identifer" [
            v [Sn; "*"] [] []
            v [Sn; identifer] [] []
        ]
        rule "SetOperation" [
            v [IndexOperation; Sn; "<-"] [IndexOperation; Sn; "<-"] [CompareOperation]
            v [CompareOperation] [] []
        ]
        rule "CompareOperation" [
            v [AddOperation; cmp_op] [AddOperation; cmp_op] [AddOperation]
            v [AddOperation] [] []
        ]
        rule "cmp_op" [
            v [Sn; ">"] [] []
            v [Sn; ">="] [] []
            v [Sn; "<"] [] []
            v [Sn; "<="] [] []
            v [Sn; "="] [] []
            v [Sn; "<>"] [] []
        ]
        rule "AddOperation" [
            v [MulOperation; add_op] [MulOperation; add_op] [MulOperation]
            v [MulOperation] [] []
        ]
        rule "add_op" [
            v [Sn; "+"] [] []
            v [Sn; "-"] [] []
        ]
        rule "MulOperation" [
            v [QueryOperation; mul_op] [QueryOperation; mul_op] [QueryOperation]
            v [QueryOperation] [] []
        ]
        rule "mul_op" [
            v [Sn; "*"] [] []
            v [Sn; "/"] [] []
        ]
        rule "QueryOperation" [
            v [Sn; "?"; IndexOperation] [] []
            v [IndexOperation; Sn; "?"; Sn; IndexOperation] [] []
            v [IndexOperation] [] []
        ]
        rule "IndexOperation" [
            v [SimpleTerm; Sn; "["; expression; Sn; "]"] [] []
            v [SimpleTerm] [] []
        ]
        rule "SimpleTerm" [
            v [Sn; new_operator] [] []
            v [Sn; float] [] []
            v [Sn; integer] [] []
            v [Sn; identifer] ["."; identifer] []
            v [Sn; "("; call_arg_list; Sn; ")"; Sn; identifer; attribute_list; Sn; "("; call_result_list; Sn; ")"] [] []
            v [Sn; "("; expression; Sn; ")"] [] []
        ]
        rule "new_operator" [
            v [Sn; "new"; attribute_list; Sn; identifer; Sn; "("; call_arg_list; Sn; ")"] [] []
            v [Sn; "new"; S; identifer; Sn; "("; call_arg_list; Sn; ")"] [] []
        ]
    ] |> Map.ofList
    
let keyToIndexMap inputMap =
    inputMap
    |> Map.toSeq
    |> Seq.mapi (fun i (k, _) -> (k, i + 20))
    |> Map.ofSeq

let indexMap = keyToIndexMap grammar  |> Map.add "Sn" 2  |> Map.add "float" 6  |> Map.add "integer" 5  |> Map.add "identifer" 3  |> Map.add "identifer_or_number" 4  |> Map.add "S" 1

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
