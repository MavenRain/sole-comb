module K = Eterm
module R = Erase

let number text = Option.value (Bignum.of_decimal text) ~default:Bignum.zero
let rec force_chain count body =
  if count = 0 then body else K.KForce (force_chain (count - 1) body)
let rec let_chain count body =
  if count = 0 then body else K.KLet ("x", K.KVar 0, let_chain (count - 1) body)

let vars depth term =
  "[" ^ String.concat "; " (List.map string_of_int (R.runtime_vars depth term)) ^ "]"

let pruned (ps, args, body) =
  "[" ^ String.concat "; " (List.map K.print_repr ps) ^ "]|"
  ^ K.ktm_list args ^ "|" ^ K.print_ktm body

let boolean value = if value then "true" else "false"
