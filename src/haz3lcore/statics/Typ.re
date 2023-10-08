include TypBase.Typ;

<<<<<<< HEAD
/* TYPE_PROVENANCE: From whence does an unknown type originate?

   Forms associated with a unique Id.t linking them to some UExp/UPat
   ------------------------------------------------------------
     SynSwitch:  Generated from an unannotated pattern variable
     AstNode:   Generated from an expression/pattern/type in the source code

   Forms without a unique Id.t of their own
   ----------------------------------------
     Matched:  Always derived from some other provenance for use in global inference.
                 Composed of a 'matched_provenenace' indicating how it was derived,
                 and the provenance it was derived from.
                 Generally, will always link to some form with its own unique Id.t
                 Currently supports matched list, arrow, and prod.

     NoProvenance:  Generated for unknown types with no provenance. They did not originate from
                   any expression/pattern/type in the source program, directly or indirectly.
                   Consequently, NoProvenance unknown types do not accumulate constraints
                   or receive inference results.*/
[@deriving (show({with_path: false}), sexp, yojson)]
type type_provenance =
  | NoProvenance
  | SynSwitch(Id.t)
  | AstNode(Id.t)
  | Matched(matched_provenance, type_provenance)
and matched_provenance =
  | Matched_Arrow_Left
  | Matched_Arrow_Right
  | Matched_Prod_Left
  | Matched_Prod_Right
  | Matched_List;

/* TYP.T: Hazel types */
[@deriving (show({with_path: false}), sexp, yojson)]
type t =
  | Unknown(type_provenance)
  | Int
  | Float
  | Bool
  | String
  | Var(string)
  | List(t)
  | Arrow(t, t)
  | Sum(t, t) // unused
  | Prod(list(t));

[@deriving (show({with_path: false}), sexp, yojson)]
type equivalence = (t, t)
and constraints = list(equivalence);

/* SOURCE: Hazel type annotated with a relevant source location.
   Currently used to track match branches for inconsistent
   branches errors, but could perhaps be used more broadly
   for type debugging UI. */
[@deriving (show({with_path: false}), sexp, yojson)]
type source = {
  id: int,
  ty: t,
};

[@deriving (show({with_path: false}), sexp, yojson)]
type free_errors =
  | Variable
  | Tag
  | TypeVariable;

/* SELF: The (synthetic) type information derivable from a term
   in isolation, using the typing context but not the syntactic
   context. This can either be Free (no type, in the case of
   unbound/undefined names), Joined (a list of types, possibly
   inconsistent, generated by branching forms like ifs,
   matches, and list literals), or Just a regular type. */
[@deriving (show({with_path: false}), sexp, yojson)]
type self =
  | Just(t)
  // TODO: make it so that joined applies only to inconsistent types; rename NoJoin
  | Joined(t => t, list(source))
  | Multi
  | Free(free_errors);

/* MODE: The (analytic) type information derived from a term's
   syntactic context. This can either Syn (no type expectation),
   or Ana (a type expectation). It is conjectured [citation needed]
   that the Syn mode is functionally indistinguishable from
   Ana(Unknown(SynSwitch)), and that this type is thus vestigial. */
[@deriving (show({with_path: false}), sexp, yojson)]
type mode =
  | SynFun
  | Syn
  | Ana(t);

/* Strip location information from a list of sources */
let source_tys = List.map((source: source) => source.ty);

/*
 I THINK THIS MIGHT BE THE PROBLEM: WHY IS INFERENCE < SYNSWITCH?
 NVM LOL

 How type provenance information should be collated when
  joining unknown types. This probably requires more thought,
  but right now TypeHole strictly predominates over Internal
  which strictly predominates over SynSwitch, which
  strictly predominates over NoProvenance.
  If two provenances have different Ids, either can be taken as a
  representative of the other in later computations regarding the
  type as a whole.
  Similarly, if two Internal provenances have different matched provenance
  strucutres, either structure can be taken. Precedence:
  TypeHole > Internal > SynSwitch > Matched > NoProvenance*/
let join_type_provenance =
    (p1: type_provenance, p2: type_provenance): type_provenance =>
  switch (p1, p2) {
  | (AstNode(_) as t, Matched(_) | AstNode(_) | SynSwitch(_) | NoProvenance)
  | (Matched(_) | SynSwitch(_) | NoProvenance, AstNode(_) as t) => t
  | (SynSwitch(_) as s, Matched(_) | SynSwitch(_) | NoProvenance)
  | (Matched(_) | NoProvenance, SynSwitch(_) as s) => s
  | (Matched(_) as inf, NoProvenance | Matched(_))
  | (NoProvenance, Matched(_) as inf) => inf
  | (NoProvenance, NoProvenance) => NoProvenance
  };

/* Lattice join on types. This is a LUB join in the hazel2
   sense in that any type dominates Unknown */
let rec join = (ty1: t, ty2: t): option(t) =>
  switch (ty1, ty2) {
  | (Unknown(p1), Unknown(p2)) =>
    Some(Unknown(join_type_provenance(p1, p2)))
  | (Unknown(_), ty)
  | (ty, Unknown(_)) => Some(ty)
  | (Int, Int) => Some(Int)
  | (Int, _) => None
  | (Float, Float) => Some(Float)
  | (Float, _) => None
  | (Bool, Bool) => Some(Bool)
  | (Bool, _) => None
  | (String, String) => Some(String)
  | (String, _) => None
  | (Arrow(ty1_1, ty1_2), Arrow(ty2_1, ty2_2)) =>
    switch (join(ty1_1, ty2_1), join(ty1_2, ty2_2)) {
    | (Some(ty1), Some(ty2)) => Some(Arrow(ty1, ty2))
    | _ => None
    }
  | (Arrow(_), _) => None
  | (Prod(tys1), Prod(tys2)) =>
    if (List.length(tys1) != List.length(tys2)) {
      None;
    } else {
      switch (List.map2(join, tys1, tys2) |> Util.OptUtil.sequence) {
      | None => None
      | Some(tys) => Some(Prod(tys))
      };
    }
  | (Prod(_), _) => None
  | (Sum(ty1_1, ty1_2), Sum(ty2_1, ty2_2)) =>
    switch (join(ty1_1, ty2_1), join(ty1_2, ty2_2)) {
    | (Some(ty1), Some(ty2)) => Some(Sum(ty1, ty2))
    | _ => None
    }
  | (Sum(_), _) => None
  | (List(ty_1), List(ty_2)) =>
    switch (join(ty_1, ty_2)) {
    | Some(ty) => Some(List(ty))
    | None => None
    }
  | (List(_), _) => None
  | (Var(n1), Var(n2)) when n1 == n2 => Some(ty1)
  | (Var(_), _) => None
  };

let join_all: list(t) => option(t) =
  List.fold_left(
    (acc, ty) => Util.OptUtil.and_then(join(ty), acc),
    Some(Unknown(NoProvenance)),
  );

let join_or_fst = (ty: t, ty': t): t =>
  switch (join(ty, ty')) {
  | None => ty
  | Some(ty) => ty
  };

let rec contains_hole = (ty: t): bool =>
  switch (ty) {
  | Unknown(_) => true
  | Arrow(ty1, ty2)
  | Sum(ty1, ty2) => contains_hole(ty1) || contains_hole(ty2)
  | Prod(tys) => List.exists(contains_hole, tys)
  | _ => false
  };

let t_of_self =
  fun
  | Just(t) => t
  | Joined(wrap, ss) =>
    switch (ss |> List.map(s => s.ty) |> join_all) {
    | None => Unknown(NoProvenance)
    | Some(t) => wrap(t)
    }
  | Multi
  | Free(_) => Unknown(NoProvenance);

/* MATCHED JUDGEMENTS: Note that matched judgements work
   a bit different than hazel2 here since hole fixing is
   implicit. Somebody should check that what I'm doing
   here actually makes sense -Andrew*/

let matched_arrow = (ty: t, termId: Id.t): ((t, t), constraints) => {
  let matched_arrow_of_prov = prov => {
    let (arrow_lhs, arrow_rhs) = (
      Unknown(Matched(Matched_Arrow_Left, prov)),
      Unknown(Matched(Matched_Arrow_Right, prov)),
    );
    (
      (arrow_lhs, arrow_rhs),
      [(Unknown(prov), Arrow(arrow_lhs, arrow_rhs))],
    );
  };
  switch (ty) {
  | Arrow(ty_in, ty_out) => ((ty_in, ty_out), [])
  | Unknown(prov) => matched_arrow_of_prov(prov)
  | _ => matched_arrow_of_prov(AstNode(termId))
  };
};

let matched_arrow_mode =
    (mode: mode, termId: Id.t): ((mode, mode), constraints) => {
  switch (mode) {
  | SynFun
  | Syn => ((Syn, Syn), [])
  | Ana(ty) =>
    let ((ty_in, ty_out), constraints) = matched_arrow(ty, termId);
    ((Ana(ty_in), Ana(ty_out)), constraints);
  };
};

let matched_list = (ty: t, termId: Id.t): (t, constraints) => {
  let matched_list_of_prov = prov => {
    let list_elts_typ = Unknown(Matched(Matched_List, prov));
    (list_elts_typ, [(Unknown(prov), List(list_elts_typ))]);
  };

  switch (ty) {
  | List(ty) => (ty, [])
  | Unknown(prov) => matched_list_of_prov(prov)
  | _ => matched_list_of_prov(AstNode(termId))
  };
};

let matched_list_mode = (mode: mode, termId: Id.t): (mode, constraints) => {
  switch (mode) {
  | SynFun
  | Syn => (Syn, [])
  | Ana(ty) =>
    let (ty_elts, constraints) = matched_list(ty, termId);
    (Ana(ty_elts), constraints);
  };
};

let rec matched_prod_mode = (mode: mode, length): (list(mode), constraints) => {
  let binary_matched_prod_of_prov =
      (prov: type_provenance): ((t, t), equivalence) => {
    let (left_ty, right_ty) = (
      Unknown(Matched(Matched_Prod_Left, prov)),
      Unknown(Matched(Matched_Prod_Right, prov)),
    );
    ((left_ty, right_ty), (Unknown(prov), Prod([left_ty, right_ty])));
  };

  switch (mode, length) {
  | (Ana(Unknown(prov)), 2) =>
    let ((left_ty, right_ty), equivalence) =
      binary_matched_prod_of_prov(prov);
    ([Ana(left_ty), Ana(right_ty)], [equivalence]);
  | (Ana(Unknown(prov)), _) when length > 2 =>
    let ((left_ty, right_ty), equivalence) =
      binary_matched_prod_of_prov(prov);
    let (modes_of_rest, constraints_of_rest) =
      matched_prod_mode(Ana(right_ty), length - 1);
    (
      [Ana(left_ty), ...modes_of_rest],
      [equivalence, ...constraints_of_rest],
    );
  | (Ana(Prod(ana_tys)), _) when List.length(ana_tys) == length => (
      List.map(ty => Ana(ty), ana_tys),
      [],
    )
  | _ => (List.init(length, _ => Syn), [])
  };
};

let ap_mode: mode = SynFun;

/* Legacy code from HTyp */

let precedence_Prod = 1;
let precedence_Arrow = 2;
let precedence_Sum = 3;
let precedence_const = 4;
let precedence = (ty: t): int =>
  switch (ty) {
  | Int
  | Float
  | Bool
  | String
  | Unknown(_)
  | Var(_)
  | Prod([])
  | List(_) => precedence_const
  | Prod(_) => precedence_Prod
  | Sum(_, _) => precedence_Sum
  | Arrow(_, _) => precedence_Arrow
  };

/* equality
   At the moment, this coincides with default equality,
   but this will change when polymorphic types are implemented */
let rec eq = (t1, t2) =>
  switch (t1, t2) {
  | (Int, Int) => true
  | (Int, _) => false
  | (Float, Float) => true
  | (Float, _) => false
  | (Bool, Bool) => true
  | (Bool, _) => false
  | (String, String) => true
  | (String, _) => false
  | (Unknown(_), Unknown(_)) => true
  | (Unknown(_), _) => false
  | (Arrow(t1_1, t1_2), Arrow(t2_1, t2_2)) =>
    eq(t1_1, t2_1) && eq(t1_2, t2_2)
  | (Arrow(_), _) => false
  | (Prod(tys1), Prod(tys2)) =>
    List.length(tys1) == List.length(tys2) && List.for_all2(eq, tys1, tys2)
  | (Prod(_), _) => false
  | (Sum(t1_1, t1_2), Sum(t2_1, t2_2)) => eq(t1_1, t2_1) && eq(t1_2, t2_2)
  | (Sum(_), _) => false
  | (List(t1), List(t2)) => eq(t1, t2)
  | (List(_), _) => false
  | (Var(n1), Var(n2)) => n1 == n2
  | (Var(_), _) => false
  };

let rec typ_to_string = (ty: t): string => {
  typ_to_string_with_parens(false, ty);
}
and typ_to_string_with_parens = (is_left_child: bool, ty: t): string => {
  //TODO: parens on ops when ambiguous
  let parenthesize_if_left_child = s => is_left_child ? "(" ++ s ++ ")" : s;
  switch (ty) {
  | Unknown(_) => "?"
  | Int => "Int"
  | Float => "Float"
  | String => "String"
  | Bool => "Bool"
  | Var(name) => name
  | List(t) => "[" ++ typ_to_string(t) ++ "]"
  | Arrow(t1, t2) =>
    typ_to_string_with_parens(true, t1)
    ++ " -> "
    ++ typ_to_string(t2)
    |> parenthesize_if_left_child
  | Prod([]) => "Unit"
  | Prod([_]) => "BadProduct"
  | Prod([t0, ...ts]) =>
    "("
    ++ List.fold_left(
         (acc, t) => acc ++ ", " ++ typ_to_string(t),
         typ_to_string(t0),
         ts,
       )
    ++ ")"
  | Sum(t1, t2) =>
    typ_to_string_with_parens(true, t1)
    ++ " + "
    ++ typ_to_string(t2)
    |> parenthesize_if_left_child
  };
};
=======
/* Due to otherwise cyclic dependencies, Typ and Ctx
   are jointly located in the TypBase module */
>>>>>>> dev
