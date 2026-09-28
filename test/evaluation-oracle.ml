(* A.3 observations, linked against a fresh copy of the pinned kernel sources. *)
module T = Term
module V = Value
module S = Shape
module Q = Quantity
module G = Global
module L = Level
module Lit = Literal
module Pos = Positivity
module P = Prim
let number negative digits =
  let n = List.fold_left (fun n d -> Z.add (Z.mul n (Z.of_int 10)) (Z.of_int d)) Z.zero digits in
  if negative then Z.neg n else n
let level n = Option.value (L.of_int n) ~default:L.zero
let quantity = function Q.Zero -> "0" | Q.One -> "1" | Q.Many -> "w"
let strings xs = String.concat "" (List.map (fun s -> s ^ ";") xs)
let literal = function Lit.LInt n -> Bignum.to_string n | Lit.LString s -> "string:" ^ s
let option_text o = Option.value o ~default:"none"
let rec render = function
  | T.Var i -> "v" ^ string_of_int i
  | T.Univ l -> "u" ^ L.to_string l
  | T.Global n -> "g:" ^ n
  | T.Lit l -> "lit:" ^ literal l
  | T.Auto -> "auto"
  | T.Lan (s,d) -> "lan(" ^ shape s ^ "," ^ render d ^ ")"
  | T.Ran (s,d) -> "ran(" ^ shape s ^ "," ^ render d ^ ")"
  | T.In (s,a,xs) -> "in(" ^ shape s ^ "," ^ addr a ^ ",[" ^ terms xs ^ "])"
  | T.Sec (s,ls) -> "sec(" ^ shape s ^ ",[" ^ strings (List.map leg ls) ^ "])"
  | T.Out (s,a,v) -> "out(" ^ shape s ^ "," ^ addr a ^ "," ^ render v ^ ")"
  | T.Let (n,ty,v,b) -> "let(" ^ n ^ "," ^ render ty ^ "," ^ render v ^ "," ^ render b ^ ")"
  | T.Ann (t,ty) -> "ann(" ^ render t ^ "," ^ render ty ^ ")"
  | T.Elim e -> "elim(" ^ shape e.e_shape ^ "," ^ render e.e_scrut ^ "," ^ quantity e.e_scrut_q ^ "," ^ motive e.e_motive ^ ",[" ^ strings (List.map (fun (a,l) -> addr a ^ ":" ^ leg l) e.e_branches) ^ "])"
and terms xs = strings (List.map render xs)
and shape = function
  | S.SPi (q,n,a) -> "pi(" ^ quantity q ^ "," ^ n ^ "," ^ render a ^ ")"
  | S.SColl n -> "coll" ^ string_of_int n
  | S.SPar (a,b) -> "par(" ^ render a ^ "," ^ render b ^ ")"
  | S.SMu (n,xs) -> "mu(" ^ n ^ ",[" ^ terms xs ^ "])"
  | S.SNu (n,xs) -> "nu(" ^ n ^ ",[" ^ terms xs ^ "])"
and addr = function
  | T.APt (q,a) -> "pt(" ^ quantity q ^ "," ^ render a ^ ")"
  | T.ALeg i -> "leg" ^ string_of_int i
  | T.ACtor n -> "ctor:" ^ n
and leg l = "([" ^ strings (List.map (fun (q,n) -> quantity q ^ ":" ^ n) l.T.l_binders) ^ "]," ^ render l.T.l_body ^ ")"
and motive m = Option.fold ~none:"none" ~some:(fun m -> "some(" ^ option_text m.T.m_ind ^ ",[" ^ strings m.T.m_idx ^ "]," ^ m.T.m_self ^ "," ^ render m.T.m_body ^ ")") m
let error e = "error:" ^ Error.to_string e
let term_result r = Result.fold ~ok:render ~error r
let bool_result r = Result.fold ~ok:string_of_bool ~error r
let nf g n env t = term_result (Result.bind (Eval.eval g env t) (Eval.quote g n))
let weak g n v = term_result (Result.bind (Eval.whnf g v) (Eval.quote g n))
type context = { globals:G.t; size:int; env:V.t list; head_ty:V.t; probe:int; exhausted:bool }
let context globals head_ty probe exhausted = {globals;size=0;env=[];head_ty;probe;exhausted}
let guard c r = if c.exhausted then Error (Error.Budget_exhausted "evaluation-test") else r
let infer_univ c _ = guard c (match c.probe with 0 -> Ok L.one | 1 -> Ok L.zero | _ -> Error (Error.Cannot_infer "evaluation-test"))
let rec ops : context Rules.ops = {
  o_infer = (fun _ _ _ -> Error (Error.Cannot_infer "unused"));
  o_check = (fun _ _ _ _ -> Error (Error.Cannot_infer "unused"));
  o_close = (fun _ _ _ _ -> Error (Error.Cannot_infer "unused"));
  o_infer_univ = infer_univ;
  o_conv = (fun c ~ty a b -> Conv.conv ops c ~ty a b);
  o_conv_type = (fun c a b -> Conv.conv_type ops c a b);
  o_eval = (fun c t -> guard c (Eval.eval c.globals c.env t));
  o_whnf = (fun c v -> guard c (Eval.whnf c.globals v));
  o_bind = (fun _ _ _ c -> {c with size=c.size+1;env=V.var c.size::c.env});
  o_size = (fun c -> c.size); o_env = (fun c -> c.env);
  o_ev = (fun c -> {Rules.ev_eval=(fun env t -> guard c (Eval.eval c.globals env t))});
  o_pp = (fun _ _ -> "unused");
  o_quote = (fun c v -> guard c (Eval.quote c.globals c.size v));
  o_head_ty = (fun c _ -> guard c (Ok c.head_ty));
  o_family = (fun c n -> G.find_family n c.globals);
}
let conv c ty a b = bool_result (Conv.conv ops c ~ty a b)
let conv_type c a b = bool_result (Conv.conv_type ops c a b)
