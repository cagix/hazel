open Sexplib.Std;

/* TYPE_PROVENANCE: From whence does an unknown type originate?
   Is it generated from an unannotated pattern variable (SynSwitch),
   a pattern variable annotated with a type hole (TypeHole), or
   generated by an internal judgement (Internal)? */
[@deriving (show({with_path: false}), sexp, yojson)]
type type_provenance =
  | SynSwitch
  | TypeHole
  | Internal;

[@deriving (show({with_path: false}), sexp, yojson)]
type ann('item) = {
  item: 'item,
  ann: string,
};

/* TYP.T: Hazel types */
[@deriving (show({with_path: false}), sexp, yojson)]
type t =
  | Unknown(type_provenance)
  | Int
  | Float
  | Bool
  | String
  | Var(ann(option(int)))
  | List(t)
  | Arrow(t, t)
  | LabelSum(list(tagged))
  | Sum(t, t) // unused
  | Prod(list(t))
  | Rec(ann(t))
  | Forall(ann(t))
and tagged = {
  tag: Token.t,
  typ: t,
};

let sort_tagged: list(tagged) => list(tagged) =
  List.sort(({tag: t1, _}, {tag: t2, _}) => compare(t1, t2));

[@deriving (show({with_path: false}), sexp, yojson)]
type adt = (Token.t, list(tagged));

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
  | Syn
  | Ana(t);

/* Strip location information from a list of sources */
let source_tys = List.map((source: source) => source.ty);

/* How type provenance information should be collated when
   joining unknown types. This probably requires more thought,
   but right now TypeHole strictly predominates over Internal
   which strictly predominates over SynSwitch. */
let join_type_provenance =
    (p1: type_provenance, p2: type_provenance): type_provenance =>
  switch (p1, p2) {
  | (TypeHole, TypeHole | Internal | SynSwitch)
  | (Internal | SynSwitch, TypeHole) => TypeHole
  | (Internal, Internal | SynSwitch)
  | (SynSwitch, Internal) => Internal
  | (SynSwitch, SynSwitch) => SynSwitch
  };

/* MATCHED JUDGEMENTS: Note that matched judgements work
   a bit different than hazel2 here since hole fixing is
   implicit. Somebody should check that what I'm doing
   here actually makes sense -Andrew */

let matched_arrow: t => (t, t) =
  fun
  | Arrow(ty_in, ty_out) => (ty_in, ty_out)
  | Unknown(prov) => (Unknown(prov), Unknown(prov))
  | _ => (Unknown(Internal), Unknown(Internal));

let matched_forall: t => ann(t) =
  fun
  | Forall(ann) => ann
  | Unknown(prov) => {item: Unknown(prov), ann: "expected_forall"}
  | _ => {item: Unknown(Internal), ann: "expected_forall"};

let matched_arrow_mode: mode => (mode, mode) =
  fun
  | Syn => (Syn, Syn)
  | Ana(ty) => {
      let (ty_in, ty_out) = matched_arrow(ty);
      (Ana(ty_in), Ana(ty_out));
    };

let matched_forall_mode: mode => mode =
  fun
  | Syn => Syn
  | Ana(ty) => {
      let ann = matched_forall(ty);
      Ana(ann.item);
    };

let matched_prod_mode = (mode: mode, length): list(mode) =>
  switch (mode) {
  | Ana(Prod(ana_tys)) when List.length(ana_tys) == length =>
    List.map(ty => Ana(ty), ana_tys)
  | _ => List.init(length, _ => Syn)
  };

let matched_list: t => t =
  fun
  | List(ty) => ty
  | Unknown(prov) => Unknown(prov)
  | _ => Unknown(Internal);

let matched_list_mode: mode => mode =
  fun
  | Syn => Syn
  | Ana(ty) => Ana(matched_list(ty));

let matched_list_lit_mode = (mode: mode, length): list(mode) =>
  switch (mode) {
  | Syn => List.init(length, _ => Syn)
  | Ana(ty) => List.init(length, _ => Ana(matched_list(ty)))
  };

let ap_mode: mode = Syn;

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
  | Rec(_)
  | Prod([])
  | LabelSum(_)
  | List(_) => precedence_const
  | Prod(_)
  | Forall(_) => precedence_Prod
  | Sum(_, _) => precedence_Sum
  | Arrow(_, _) => precedence_Arrow
  };

/* equality
   At the moment, this coincides with default equality,
   but this will change when polymorphic types are implemented */
let rec eq = (t1, t2) => {
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
  | (LabelSum(tys1), LabelSum(tys2)) =>
    let (tys1, tys2) = (sort_tagged(tys1), sort_tagged(tys2));
    List.length(tys1) == List.length(tys2)
    && List.for_all2(
         (ts1, ts2) => ts1.tag == ts2.tag && eq(ts1.typ, ts2.typ),
         tys1,
         tys2,
       );
  | (LabelSum(_), _) => false
  | (Var({item: x1, _}), Var({item: x2, _})) => x1 == x2
  | (Var(_), _) => false
  | (Rec({item: t1, _}), Rec({item: t2, _})) => eq(t1, t2)
  | (Rec(_), _) => false
  | (Forall({item: t1, _}), Forall({item: t2, _})) => eq(t1, t2)
  | (Forall(_), _) => false
  };
};

// let rec free_vars = (ty: t): list(Token.t) =>
//   switch (ty) {
//   | Unknown(_)
//   | Int
//   | Float
//   | Bool
//   | String => []
//   | Var({item, ann}) =>
//     switch (item) {
//     | Some(_) => []
//     | None => [ann]
//     }
//   // | List(ty) => free_vars(ty)
//   // | Arrow(t1, t2)
//   // | Sum(t1, t2) => free_vars(~bound, t1) @ free_vars(~bound, t2)
//   // | LabelSum(tags) =>
//   //   List.concat(List.map(tag => free_vars(~bound, tag.typ), tags))
//   // | Prod(tys) => List.concat(List.map(free_vars(~bound), tys))
//   // | Rec(x, ty) => free_vars(~bound=[x] @ bound, ty)
//   // | Forall(x, ty) => free_vars(~bound=[x] @ bound, ty)
//   };

let rec subst = (s: t, ~x: int=0, ty: t) => {
  let subst' = subst(~x=x + 1, s);
  switch (ty) {
  | Int => Int
  | Float => Float
  | Bool => Bool
  | String => String
  | Unknown(prov) => Unknown(prov)
  | Arrow(ty1, ty2) => Arrow(subst'(ty1), subst'(ty2))
  | Prod(tys) => Prod(List.map(ty => subst'(ty), tys))
  | LabelSum(tys) =>
    LabelSum(List.map(ty => {tag: ty.tag, typ: subst'(ty.typ)}, tys))
  | Sum(ty1, ty2) => Sum(subst'(ty1), subst'(ty2))
  | List(ty) => List(subst'(ty))
  | Rec({item, ann}) => Rec({item: subst'(item), ann})
  | Forall({item, ann}) => Forall({item: subst'(item), ann})
  | Var({item: y, _}) => Some(x) == y ? s : ty
  };
};
