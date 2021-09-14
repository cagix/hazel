[@deriving sexp]
type t =
  | Collapsed
  | HoleLabel
  | Delim
  | EmptyTagHole(bool, MetaVar.t)
  | EmptyHole(bool, HoleInstance.t)
  | NonEmptyHole(ErrStatus.HoleReason.t, HoleInstance.t)
  | VarHole(VarErrStatus.HoleReason.t, HoleInstance.t)
  | InjHole(InjErrStatus.HoleReason.t, HoleInstance.t)
  | InconsistentBranches(HoleInstance.t)
  | Invalid(HoleInstance.t)
  | FailedCastDelim
  | FailedCastDecoration
  | CastDecoration
  | DivideByZero;
