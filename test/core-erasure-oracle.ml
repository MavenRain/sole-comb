(* The ordinary erased mode of pinned bin/kanon.ml, freshly compiled. *)
let report source =
  Result.bind (Elab.check_in Global.initial source)
    (fun (globals, rows) -> Erase.program globals rows)
  |> Result.fold
       ~error:(fun error -> prerr_endline (Error.to_string error); 1)
       ~ok:(fun rows -> print_string (Erase.print rows); 0)

let layout_report source names =
  let ( let* ) = Result.bind in
  let* globals, _rows = Elab.check_in Global.initial source in
  let c = Check.make globals Budget.unlimited in
  let c = Check.bind "ambient" Quantity.Many (Value.VNeutral (Value.HGlobal "Nat", [])) c in
  let ec : Erase.ectx = { c; slots = []; self = "layout" } in
  let family name =
    let* fam = Global.find_family name globals |> Option.to_result ~none:(Error.Unbound name) in
    let* tids = Erase.mu_group_tids ec name fam in
    let ctor ct =
      let* fields = Erase.mu_layout ec fam ct in
      let texts = List.map (Option.fold ~none:"drop" ~some:Eterm.print_repr) fields in
      Ok (ct.Positivity.c_name ^ " [" ^ String.concat ";" texts ^ "]\n")
    in
    let* lines = Rules.all_ok (List.map ctor fam.Positivity.f_ctors) in
    Ok (name ^ " [" ^ String.concat ";" (List.map Eterm.tid_text tids) ^ "]\n" ^ String.concat "" lines)
  in
  Result.map (String.concat "") (Rules.all_ok (List.map family names))

let () =
  match Array.to_list Sys.argv with
  | [_exe; path] ->
      exit (report (In_channel.with_open_bin path In_channel.input_all))
  | _exe :: "--layouts" :: path :: (_ :: _ as names) ->
      layout_report (In_channel.with_open_bin path In_channel.input_all) names
      |> Result.fold
           ~error:(fun error -> prerr_endline (Error.to_string error); exit 1)
           ~ok:print_string
  | _ -> prerr_endline "usage: core-erasure-oracle FILE | --layouts FILE FAMILY..."; exit 64
