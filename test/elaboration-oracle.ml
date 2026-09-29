(* Pinned expression/declaration entry points, excluding the recursive program driver. *)
let ( let* ) = Result.bind
let body_text body = Option.fold ~none:"" ~some:(fun tm -> " = " ^ Pp.term [] tm) body
let kind_text = function Check.Definition -> "def " | Check.Postulate -> "axiom "
let declaration_text (d : Check.decl) =
  kind_text d.d_kind ^ d.d_name ^ " : " ^ Pp.term [] d.d_ty ^ body_text d.d_body ^ ";\n"
let admit g = function
  | Syntax.DMu families ->
      let* next = Elab.elab_mu_group g families in
      Ok (next, String.concat "" (List.map (fun (f : Syntax.fam) -> "mu " ^ f.fm_name ^ ";\n") families))
  | (Syntax.DDef _ | Syntax.DAxiom _ | Syntax.DRec _) as d ->
      let* decl = Elab.elab_decl (Check.make g Budget.unlimited) d in
      let* entry = Check.check_decl g Budget.unlimited decl in
      Ok (Global.add decl.Check.d_name entry g, declaration_text decl)
let rec declarations g = function
  | [] -> Ok ""
  | d :: rest ->
      let* next, text = admit g d in
      let* tail = declarations next rest in
      Ok (text ^ tail)
let observe source =
  Result.fold ~error:(fun e -> "error:" ^ Error.to_string e)
    ~ok:(fun text -> "ok:" ^ text)
    (Result.bind (Parser.parse source) (declarations Global.initial))
let hex s = String.to_seq s |> Seq.map (fun c -> Printf.sprintf "%02x" (Char.code c)) |> List.of_seq |> String.concat ""
let emit name source = Printf.printf "%s\t%s\n" name (hex (observe source))
