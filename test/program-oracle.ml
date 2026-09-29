(* The check paths of pinned bin/kanon.ml:39-59, using freshly compiled modules. *)
let report source print_form =
  Elab.check_in Global.initial source
  |> Result.fold
       ~error:(fun error -> prerr_endline (Error.to_string error); 1)
       ~ok:(fun (_globals, rows) ->
         if print_form then print_string (Elab.checked_form rows);
         0)

let main args =
  match args with
  | [_exe; mode; path] when String.equal mode "check" || String.equal mode "print" ->
      if Sys.file_exists path then
        report (In_channel.with_open_bin path In_channel.input_all) (String.equal mode "print")
      else (prerr_endline ("kanon: cannot read " ^ path); 64)
  | [] | [_] | [_; _] | _ :: _ :: _ :: _ ->
      prerr_endline "usage: program-oracle check|print FILE"; 64

let () = exit (main (Array.to_list Sys.argv))
