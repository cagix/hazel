[@deriving (sexp, eq, ord)]
type t =
  | NecessarilyComplete
  | NecessarilyIncomplete
  | IndeterminatelyIncomplete;

let join: (t, t) => t;

let join_fold: list(t) => t;
