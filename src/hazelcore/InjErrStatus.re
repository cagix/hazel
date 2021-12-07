module HoleReason = {
  /* Variable: `reason` */
  [@deriving sexp]
  type t =
    | ExpectedTypeNotConsistentWithSums
    | ExpectedArg
    | UnexpectedArg;

  let eq = (x, y) => x == y;
};

/* Variable: `err` */
[@deriving sexp]
type t =
  | NotInHole
  | InHole(HoleReason.t, MetaVar.t);
