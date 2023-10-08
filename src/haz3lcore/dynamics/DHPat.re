open Sexplib.Std;

[@deriving (show({with_path: false}), sexp, yojson)]
type t =
  | EmptyHole(MetaVar.t, MetaVarInst.t)
  | NonEmptyHole(ErrStatus.HoleReason.t, MetaVar.t, MetaVarInst.t, t)
  | Wild
  | ExpandingKeyword(MetaVar.t, MetaVarInst.t, ExpandingKeyword.t)
  | InvalidText(MetaVar.t, MetaVarInst.t, string)
  | BadTag(MetaVar.t, MetaVarInst.t, string)
  | Var(Var.t)
  | IntLit(int)
  | FloatLit(float)
  | BoolLit(bool)
  | StringLit(string)
  | ListLit(Typ.t, list(t))
  | Cons(t, t)
  | Tuple(list((option(LabeledTuple.t), t)))
  | Tag(string)
  | Ap(t, t);

let mk_tuple: list((option(LabeledTuple.t), t)) => t =
  fun
  | []
  | [_] => failwith("mk_tuple: expected at least 2 elements")
  | dps => Tuple(dps);

/**
 * Whether dp contains the variable x outside of a hole.
 */
let rec binds_var = (x: Var.t, dp: t): bool =>
  switch (dp) {
  | EmptyHole(_, _)
  | NonEmptyHole(_, _, _, _)
  | Wild
  | InvalidText(_)
  | BadTag(_)
  | IntLit(_)
  | FloatLit(_)
  | BoolLit(_)
  | StringLit(_)
  | Tag(_)
  | ExpandingKeyword(_, _, _) => false
  | Var(y) => Var.eq(x, y)
  | Tuple(dps) => dps |> List.exists(((_, e)) => binds_var(x, e))
  | Cons(dp1, dp2) => binds_var(x, dp1) || binds_var(x, dp2)
  | ListLit(_, d_list) =>
    let new_list = List.map(binds_var(x), d_list);
    List.fold_left((||), false, new_list);
  | Ap(_, _) => false
  };
