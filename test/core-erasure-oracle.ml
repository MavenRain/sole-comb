(* The ordinary erased mode of pinned bin/kanon.ml, freshly compiled. *)
let report source =
  Result.bind (Elab.check_in Global.initial source)
    (fun (globals, rows) -> Erase.program globals rows)
  |> Result.fold
       ~error:(fun error -> prerr_endline (Error.to_string error); 1)
       ~ok:(fun rows -> print_string (Erase.print rows); 0)

let () =
  match Array.to_list Sys.argv with
  | [_exe; path] ->
      exit (report (In_channel.with_open_bin path In_channel.input_all))
  | _ -> prerr_endline "usage: core-erasure-oracle FILE"; exit 64
