open TermBase.Pat;

// [@deriving (show({with_path: false}), sexp, yojson)]
// type term =
//   | Invalid(string)
//   | EmptyHole
//   // TODO: Multihole
//   | Wild
//   | Int(int)
//   | Float(float)
//   | Bool(bool)
//   | String(string)
//   // TODO: Remove Triv from UPat
//   | ListLit(list(t))
//   | Constructor(string)
//   | Cons(t, t)
//   | Var(Var.t)
//   | Tuple(list(t))
//   // TODO: parens
//   | Ap(t, t)
//   // TODO: Add Type Annotations???
//   // TODO: Work out what to do with invalids
//   | NonEmptyHole(ErrStatus.HoleReason.t, MetaVar.t, MetaVarInst.t, t)
//   | BadConstructor(MetaVar.t, MetaVarInst.t, string)
// and t = {
//   ids: list(Id.t),
//   term,
// };

let rep_id = ({ids, _}) => List.hd(ids);
let term_of = ({term, _}) => term;
// All children of term must have expression-unique ids.
let unwrap = ({ids, term}) => (term, term => {ids, term});
let fresh = term => {
  {ids: [Id.mk()], term};
};

/**
 * Whether dp contains the variable x outside of a hole.
 */
let rec binds_var = (m: Statics.Map.t, x: Var.t, dp: t): bool =>
  switch (Statics.get_pat_error_at(m, rep_id(dp))) {
  | Some(_) => false
  | None =>
    switch (dp |> term_of) {
    | EmptyHole
    | MultiHole(_)
    | Wild
    | Invalid(_)
    | Int(_)
    | Float(_)
    | Bool(_)
    | String(_)
    | Constructor(_) => false
    | TypeAnn(y, _)
    | Parens(y) => binds_var(m, x, y)
    | Var(y) => Var.eq(x, y)
    | Tuple(dps) => dps |> List.exists(binds_var(m, x))
    | Cons(dp1, dp2) => binds_var(m, x, dp1) || binds_var(m, x, dp2)
    | ListLit(d_list) =>
      let new_list = List.map(binds_var(m, x), d_list);
      List.fold_left((||), false, new_list);
    | Ap(_, _) => false
    }
  };

let rec bound_vars = (m, dp: t): list(Var.t) =>
  switch (Statics.get_pat_error_at(m, rep_id(dp))) {
  | Some(_) => []
  | None =>
    switch (dp |> term_of) {
    | EmptyHole
    | MultiHole(_)
    | Wild
    | Invalid(_)
    | Int(_)
    | Float(_)
    | Bool(_)
    | String(_)
    | Constructor(_) => []
    | TypeAnn(y, _)
    | Parens(y) => bound_vars(m, y)
    | Var(y) => [y]
    | Tuple(dps) => List.flatten(List.map(bound_vars(m), dps))
    | Cons(dp1, dp2) => bound_vars(m, dp1) @ bound_vars(m, dp2)
    | ListLit(dps) => List.flatten(List.map(bound_vars(m), dps))
    | Ap(_, dp1) => bound_vars(m, dp1)
    }
  };

let rec get_var = (pat: t) => {
  switch (pat |> term_of) {
  | Var(x) => Some(x)
  | Parens(x) => get_var(x)
  | TypeAnn(x, _) => get_var(x)
  | Wild
  | Int(_)
  | Float(_)
  | Bool(_)
  | String(_)
  | ListLit(_)
  | Cons(_, _)
  | Tuple(_)
  | Constructor(_)
  | EmptyHole
  | Invalid(_)
  | MultiHole(_)
  | Ap(_) => None
  };
};
