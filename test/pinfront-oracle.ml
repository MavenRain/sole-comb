(* Test-only observation of the unmodified pinned lexer, parser and syntax. *)
module S = Syntax
module K = Kanon_kernel
let text s = string_of_int (String.length s) ^ ":" ^ s
let node tag xs = "(" ^ tag ^ " " ^ String.concat "" (List.map (fun x -> x ^ ";") xs) ^ ")"
let seq xs = String.concat "" (List.map (fun x -> x ^ ";") xs)
let quantity = function K.Quantity.Zero -> "0" | K.Quantity.One -> "1" | K.Quantity.Many -> "many"
let optional f value = Option.fold ~none:(node "None" []) ~some:(fun x -> node "Some" [f x]) value
let rec term = function
  | S.SVar x -> node "SVar" [text x]
  | S.SNat n -> node "SNat" [K.Bignum.to_string n]
  | S.SProp -> node "SProp" []
  | S.SType n -> node "SType" [string_of_int n]
  | S.SPrim p -> node "SPrim" [text (S.prim_name p)]
  | S.SUnit -> node "SUnit" []
  | S.SAuto -> node "SAuto" []
  | S.SPair (a, b) -> node "SPair" [term a; term b]
  | S.STuple xs -> node "STuple" (List.map term xs)
  | S.SSum xs -> node "SSum" (List.map term xs)
  | S.SProd xs -> node "SProd" (List.map term xs)
  | S.SProj (a, k) -> node "SProj" [term a; string_of_int k]
  | S.SInj (k, n, a) -> node "SInj" [string_of_int k; string_of_int n; term a]
  | S.SAbsurd a -> node "SAbsurd" [term a]
  | S.SApp (f, a) -> node "SApp" [term f; term a]
  | S.SFun (bs, body) -> node "SFun" [node "List" (List.map binder bs); term body]
  | S.SArrow (b, body) -> node "SArrow" [binder b; term body]
  | S.SStar (b, body) -> node "SStar" [binder b; term body]
  | S.SLet (n, ty, value, body) -> node "SLet" [text n; term ty; term value; term body]
  | S.SAnn (a, ty) -> node "SAnn" [term a; term ty]
  | S.SCase (s, mo, bs) -> node "SCase" [term s; optional motive mo; node "List" (List.map branch bs)]
  | S.SMatch (s, mo, bs) -> node "SMatch" [term s; optional motive mo; node "List" (List.map branch bs)]
and binder (b : S.binder) = node "Binder" [quantity b.b_q; text b.b_name; term b.b_ty]
and field (f : S.field) = node "Field" [quantity f.fd_q; text f.fd_name; optional term f.fd_ty]
and motive (m : S.motive) =
  node "Motive" [text m.mo_self; optional text m.mo_ind; node "List" (List.map text m.mo_idx); term m.mo_body]
and branch = function
  | S.BrLeg (k, bs, body) -> node "BrLeg" [string_of_int k; node "List" (List.map binder bs); term body]
  | S.BrCtor (c, fs, body) -> node "BrCtor" [text c; node "List" (List.map field fs); term body]
let constructor (c : S.fam_ctor) = node "Ctor" [text c.fc_name; term c.fc_ty]
let family (f : S.fam) =
  node "Fam" [text f.fm_name; node "List" (List.map binder f.fm_params); term f.fm_ty; node "List" (List.map constructor f.fm_ctors)]
let recursive (r : S.rec_def) = node "Rec" [text r.rd_name; term r.rd_ty; term r.rd_body]
let decl = function
  | S.DDef (n, ty, body) -> node "DDef" [text n; term ty; term body]
  | S.DAxiom (n, ty) -> node "DAxiom" [text n; term ty]
  | S.DMu fs -> node "DMu" (List.map family fs)
  | S.DRec ms -> node "DRec" (List.map recursive ms)
let ast ds = seq (List.map decl ds)
let token_kind = function
  | Token.Bytes bs -> "byte literal[" ^ String.concat "," (List.map string_of_int bs) ^ "]"
  | (Token.LParen | Token.RParen | Token.Colon | Token.ColonEq | Token.Arrow
    | Token.DArrow | Token.Star | Token.Comma | Token.Dot | Token.Dot1 | Token.Dot2
    | Token.Pipe | Token.Unit | Token.KDef | Token.KAxiom | Token.KFun | Token.KInj
    | Token.KOf | Token.KCase | Token.KMatch | Token.KAs | Token.KReturn | Token.KWith
    | Token.KTuple | Token.KSum | Token.KProd | Token.KAbsurd | Token.KProp
    | Token.KType | Token.KLet | Token.KIn | Token.KAuto | Token.KMu | Token.KMutual
    | Token.KEnd | Token.KNu | Token.KAnd | Token.KRec | Token.KNatAdd | Token.KNatSub
    | Token.KNatMul | Token.KNatEq | Token.KNatLt | Token.Ident _ | Token.Nat _ | Token.Eof) as k -> Token.describe k
let token (t : Token.t) = Printf.sprintf "%d:%d:%s;" t.loc.line t.loc.col (text (token_kind t.kind))
let roundtrip ds =
  Result.fold ~error:(fun e -> "error:" ^ K.Error.to_string e)
    ~ok:(fun again -> string_of_bool (ds = again)) (Parser.parse (S.print ds))
let parsed_observation lexed ds =
  lexed ^ "\nAST\n" ^ ast ds ^ "\nPRINT\n" ^ S.print ds ^ "\nROUNDTRIP\n" ^ roundtrip ds
let lexed_observation ts =
  let lexed = "LEX\n" ^ String.concat "" (List.map token ts) in
  Result.fold ~error:(fun e -> lexed ^ "\nPARSE-ERROR\n" ^ K.Error.to_string e)
    ~ok:(parsed_observation lexed) (Parser.parse_decls ts [])
let observe source =
  Result.fold ~error:(fun e -> "LEX-ERROR\n" ^ K.Error.to_string e)
    ~ok:lexed_observation (Lexer.lex source)
let hex s =
  String.to_seq s |> Seq.map (fun c -> Printf.sprintf "%02x" (Char.code c))
  |> List.of_seq |> String.concat ""
let emit name source = Printf.printf "%s\t%s\n" name (hex (observe source))
