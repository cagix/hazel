open Sexplib.Std;
open Util;

let exp_id = Term.UExp.rep_id;
let pat_id = Term.UPat.rep_id;
let typ_id = Term.UTyp.rep_id;
let utpat_id = Term.UTPat.rep_id;

/* STATICS

     This module determines the statics semantics of the language.
     It takes a term and returns a map which associates the unique
     ids of each term to an 'info' data structure which reflects that
     term's statics. The statics collected depend on the term's sort,
     but every term has a syntactic class (The cls types from Term),
     except Invalid terms which Term could not parse.

     The map generated by this module is intended to be generated once
     from a given term and then reused anywhere there is logic which
     depends on static information.
   */

/* Expressions are assigned a mode (reflecting the static expectations
   if any of their syntactic parent), a self (reflecting what their
   statics would be in isolation), a context (variables in scope), and
   free (variables occuring free in the expression. */
[@deriving (show({with_path: false}), sexp, yojson)]
type info_exp = {
  cls: Term.UExp.cls,
  term: Term.UExp.t,
  mode: Typ.mode,
  self: Typ.self,
  ctx: Ctx.t,
  free: Ctx.co,
};

/* Patterns are assigned a mode (reflecting the static expectations
   if any of their syntactic parent) and a self (reflecting what their
   statics would be in isolation), a context (variables in scope) */
[@deriving (show({with_path: false}), sexp, yojson)]
type info_pat = {
  cls: Term.UPat.cls,
  term: Term.UPat.t,
  mode: Typ.mode,
  self: Typ.self,
  ctx: Ctx.t // TODO: detect in-pattern shadowing
};

[@deriving (show({with_path: false}), sexp, yojson)]
type status_typ =
  | Ok(Typ.t)
  | FreeTypeVar
  | DuplicateTag
  | ApOutsideSum
  | TagExpected(Typ.t);

[@deriving (show({with_path: false}), sexp, yojson)]
type status_tag =
  | Unique
  | Duplicate;

[@deriving (show({with_path: false}), sexp, yojson)]
type typ_mode =
  | Normal
  | VariantExpected(status_tag);

[@deriving (show({with_path: false}), sexp, yojson)]
type info_typ = {
  cls: Term.UTyp.cls,
  term: Term.UTyp.t,
  mode: typ_mode,
  ctx: Ctx.t,
  status: status_typ,
};

[@deriving (show({with_path: false}), sexp, yojson)]
type info_rul = {
  cls: Term.URul.cls,
  term: Term.UExp.t,
};

[@deriving (show({with_path: false}), sexp, yojson)]
type status_tpat =
  | Ok
  | NotAName;

[@deriving (show({with_path: false}), sexp, yojson)]
type info_tpat = {
  cls: Term.UTPat.cls,
  term: Term.UTPat.t,
  status: status_tpat,
};

/* The Info aka Cursorinfo assigned to each subterm. */
[@deriving (show({with_path: false}), sexp, yojson)]
type t =
  | Invalid(TermBase.parse_flag)
  | InfoExp(info_exp)
  | InfoPat(info_pat)
  | InfoTyp(info_typ)
  | InfoRul(info_rul)
  | InfoTPat(info_tpat);

/* The InfoMap collating all info for a composite term */
[@deriving (show({with_path: false}), sexp, yojson)]
type map = Id.Map.t(t);

let terms = (map: map): Id.Map.t(Term.any) =>
  map
  |> Id.Map.filter_map(_ =>
       fun
       | Invalid(_) => None
       | InfoExp({term, _}) => Some(Term.Exp(term))
       | InfoPat({term, _}) => Some(Term.Pat(term))
       | InfoTyp({term, _}) => Some(Term.Typ(term))
       | InfoRul({term, _}) => Some(Term.Exp(term))
       | InfoTPat({term, _}) => Some(Term.TPat(term))
     );

/* Static error classes */

let status_tpat = (utpat: Term.UTPat.t): status_tpat =>
  switch (utpat.term) {
  | Var(_) => Ok
  | _ => NotAName
  };

[@deriving (show({with_path: false}), sexp, yojson)]
type error =
  | Self(Typ.self_error)
  | SynInconsistentBranches(list(Typ.t))
  | TypeInconsistent(Typ.t, Typ.t);

/* Statics non-error classes */
[@deriving (show({with_path: false}), sexp, yojson)]
type happy =
  | SynConsistent(Typ.t)
  | AnaConsistent(Typ.t, Typ.t, Typ.t) //ana, syn, join
  | AnaInternalInconsistent(Typ.t, list(Typ.t)) // ana, branches
  | AnaExternalInconsistent(Typ.t, Typ.t); // ana, syn

/* The error status which 'wraps' each term. */
[@deriving (show({with_path: false}), sexp, yojson)]
type error_status =
  | InHole(error)
  | NotInHole(happy);

/* Determines whether an expression or pattern is in an error hole,
   depending on the mode, which represents the expectations of the
   surrounding syntactic context, and the self which represents the
   makeup of the expression / pattern itself. */
let error_status = (ctx: Ctx.t, mode: Typ.mode, self: Typ.self): error_status =>
  switch (mode, self) {
  | (SynFun, Just(ty)) =>
    switch (Ctx.join(ctx, Arrow(Unknown(Internal), Unknown(Internal)), ty)) {
    | None => InHole(Self(NoFun(ty)))
    | Some(_) => NotInHole(SynConsistent(ty))
    }
  | (SynFun, Joined(_wrap, tys_syn)) =>
    let tys_syn = Typ.source_tys(tys_syn);
    switch (Ctx.join_all(ctx, tys_syn)) {
    | None => InHole(SynInconsistentBranches(tys_syn))
    | Some(ty_joined) =>
      switch (
        Ctx.join(
          ctx,
          Arrow(Unknown(Internal), Unknown(Internal)),
          ty_joined,
        )
      ) {
      | None => InHole(Self(NoFun(ty_joined)))
      | Some(_) => NotInHole(SynConsistent(ty_joined))
      }
    };
  | (Syn | SynFun | Ana(_), Self(Multi)) =>
    NotInHole(SynConsistent(Unknown(Internal)))
  | (Syn | SynFun | Ana(_), Self(err)) => InHole(Self(err))

  | (Syn, Just(ty)) => NotInHole(SynConsistent(ty))
  | (Syn, Joined(wrap, tys_syn)) =>
    let tys_syn = Typ.source_tys(tys_syn);
    switch (Ctx.join_all(ctx, tys_syn)) {
    | None => InHole(SynInconsistentBranches(tys_syn))
    | Some(ty_joined) => NotInHole(SynConsistent(wrap(ty_joined)))
    };
  | (Ana(ty_ana), Just(ty_syn)) =>
    switch (Ctx.join(ctx, ty_ana, ty_syn)) {
    | None => InHole(TypeInconsistent(ty_syn, ty_ana))
    | Some(ty_join) => NotInHole(AnaConsistent(ty_ana, ty_syn, ty_join))
    }
  | (Ana(ty_ana), Joined(wrap, tys_syn)) =>
    switch (Ctx.join_all(ctx, Typ.source_tys(tys_syn))) {
    | Some(ty_syn) =>
      let ty_syn = wrap(ty_syn);
      switch (Ctx.join(ctx, ty_syn, ty_ana)) {
      | None => NotInHole(AnaExternalInconsistent(ty_ana, ty_syn))
      | Some(ty_join) => NotInHole(AnaConsistent(ty_ana, ty_syn, ty_join))
      };
    | None =>
      NotInHole(AnaInternalInconsistent(ty_ana, Typ.source_tys(tys_syn)))
    }
  };

/* Determines whether any term is in an error hole. Currently types cannot
   be in error, and Invalids (things to which Term was unable to assign a
   parse) are always in error. The error status of expressions and patterns
   are determined by error_status above. */
let is_error = (ci: t): bool => {
  switch (ci) {
  | Invalid(Secondary) => false
  | Invalid(_) => true
  | InfoExp({mode, self, ctx, _})
  | InfoPat({mode, self, ctx, _}) =>
    switch (error_status(ctx, mode, self)) {
    | InHole(_) => true
    | NotInHole(_) => false
    }
  | InfoTyp({status, _}) =>
    switch (status) {
    | Ok(_) => false
    | _ => true
    }
  | InfoTPat({status, _}) => status != Ok
  | InfoRul(_) => false
  };
};

/* Determined the type of an expression or pattern 'after hole wrapping';
   that is, all ill-typed terms are considered to be 'wrapped in
   non-empty holes', i.e. assigned Unknown type. */
let typ_after_fix = (ctx, mode: Typ.mode, self: Typ.self): Typ.t =>
  switch (error_status(ctx, mode, self)) {
  | InHole(_) => Unknown(Internal)
  | NotInHole(SynConsistent(t)) => t
  | NotInHole(AnaConsistent(_, _, ty_join)) => ty_join
  | NotInHole(AnaExternalInconsistent(ty_ana, _)) => ty_ana
  | NotInHole(AnaInternalInconsistent(ty_ana, _)) => ty_ana
  };

/* The type of an expression after hole wrapping */
let exp_typ = (ctx, m: map, e: Term.UExp.t): Typ.t =>
  switch (Id.Map.find_opt(exp_id(e), m)) {
  | Some(InfoExp({mode, self, _})) => typ_after_fix(ctx, mode, self)
  | Some(InfoPat(_) | InfoTyp(_) | InfoRul(_) | InfoTPat(_) | Invalid(_))
  | None => failwith(__LOC__ ++ ": XXX")
  };

let t_of_self = (ctx): (Typ.self => Typ.t) =>
  fun
  | Just(t) => t
  | Joined(wrap, ss) =>
    switch (ss |> List.map((s: Typ.source) => s.ty) |> Ctx.join_all(ctx)) {
    | None => Unknown(Internal)
    | Some(t) => wrap(t)
    }
  | Self(_) => Unknown(Internal);

let exp_self_typ_id = (ctx, m: map, id): Typ.t =>
  switch (Id.Map.find_opt(id, m)) {
  | Some(InfoExp({self, _})) => t_of_self(ctx, self)
  | Some(InfoPat(_) | InfoTyp(_) | InfoRul(_) | InfoTPat(_) | Invalid(_))
  | None => failwith(__LOC__ ++ ": XXX")
  };

let exp_self_typ = (ctx, m: map, e: Term.UExp.t): Typ.t =>
  exp_self_typ_id(ctx, m, exp_id(e));

let exp_mode_id = (m: map, id): Typ.mode =>
  switch (Id.Map.find_opt(id, m)) {
  | Some(InfoExp({mode, _})) => mode
  | Some(InfoPat(_) | InfoTyp(_) | InfoRul(_) | InfoTPat(_) | Invalid(_))
  | None => failwith(__LOC__ ++ ": XXX")
  };

let exp_mode = (m: map, e: Term.UExp.t): Typ.mode =>
  exp_mode_id(m, exp_id(e));

/* The type of a pattern after hole wrapping */
let pat_typ = (ctx, m: map, p: Term.UPat.t): Typ.t =>
  switch (Id.Map.find_opt(pat_id(p), m)) {
  | Some(InfoPat({mode, self, _})) => typ_after_fix(ctx, mode, self)
  | Some(InfoExp(_) | InfoTyp(_) | InfoRul(_) | InfoTPat(_) | Invalid(_))
  | None => failwith(__LOC__ ++ ": XXX")
  };
let pat_self_typ = (ctx, m: map, p: Term.UPat.t): Typ.t =>
  switch (Id.Map.find_opt(pat_id(p), m)) {
  | Some(InfoPat({self, _})) => t_of_self(ctx, self)
  | Some(InfoExp(_) | InfoTyp(_) | InfoRul(_) | InfoTPat(_) | Invalid(_))
  | None => failwith(__LOC__ ++ ": XXX")
  };

// NOTE(andrew): changed this from union to disj_union...
let union_m = List.fold_left(Id.Map.disj_union, Id.Map.empty);

let add_info = (ids, info: 'a, m: Ptmap.t('a)) =>
  ids
  |> List.map(id => Id.Map.singleton(id, info))
  |> List.fold_left(Id.Map.disj_union, m);

let extend_let_def_ctx =
    (ctx: Ctx.t, pat: Term.UPat.t, pat_ctx: Ctx.t, def: Term.UExp.t) =>
  if (Term.UPat.is_tuple_of_arrows(pat)
      && Term.UExp.is_tuple_of_functions(def)) {
    pat_ctx;
  } else {
    ctx;
  };

let tag_ana_typ = (ctx: Ctx.t, mode: Typ.mode, tag: Token.t): option(Typ.t) =>
  /* If a tag is being analyzed against (an arrow type returning)
     a sum type having that tag as a variant, we consider the
     tag's type to be determined by the sum type */
  switch (mode) {
  | Ana(Arrow(_, ty_ana))
  | Ana(ty_ana) =>
    switch (Ctx.resolve_typ(ctx, ty_ana)) {
    | Some(TSum(tags))
    | Some(Rec(_, TSum(tags))) => Typ.ana_sum(tag, tags, ty_ana)
    | _ => None
    }
  | _ => None
  };

let tag_ap_mode = (ctx: Ctx.t, mode: Typ.mode, name: Token.t): Typ.mode =>
  /* If a tag application is being analyzed against a sum type for
     which that tag is a variant, then we consider the tag to be in
     analytic mode against an arrow returning that sum type; otherwise
     we use the typical mode for function applications */
  switch (tag_ana_typ(ctx, mode, name)) {
  | Some(Arrow(_) as ty_ana) => Ana(ty_ana)
  | Some(ty_ana) => Ana(Arrow(Unknown(Internal), ty_ana))
  | _ => Typ.ap_mode
  };

let tag_self = (ctx: Ctx.t, mode: Typ.mode, tag: Token.t): Typ.self =>
  /* If a tag is being analyzed against (an arrow type returning)
     a sum type having that tag as a variant, its self type is
     considered to be determined by the sum type; otherwise,
     check the context for the tag's type */
  switch (tag_ana_typ(ctx, mode, tag)) {
  | Some(ana_ty) => Just(ana_ty)
  | _ =>
    switch (Ctx.lookup_tag(ctx, tag)) {
    | Some(syn) => Just(syn.typ)
    | None => Self(FreeTag)
    }
  };

let typ_exp_binop_bin_int: Term.UExp.op_bin_int => Typ.t =
  fun
  | (Plus | Minus | Times | Power | Divide) as _op => Int
  | (LessThan | GreaterThan | LessThanOrEqual | GreaterThanOrEqual | Equals) as _op =>
    Bool;

let typ_exp_binop_bin_float: Term.UExp.op_bin_float => Typ.t =
  fun
  | (Plus | Minus | Times | Power | Divide) as _op => Float
  | (LessThan | GreaterThan | LessThanOrEqual | GreaterThanOrEqual | Equals) as _op =>
    Bool;

let typ_exp_binop_bin_string: Term.UExp.op_bin_string => Typ.t =
  fun
  | Equals as _op => Bool;

let typ_exp_binop: Term.UExp.op_bin => (Typ.t, Typ.t, Typ.t) =
  fun
  | Bool(And | Or) => (Bool, Bool, Bool)
  | Int(op) => (Int, Int, typ_exp_binop_bin_int(op))
  | Float(op) => (Float, Float, typ_exp_binop_bin_float(op))
  | String(op) => (String, String, typ_exp_binop_bin_string(op));

let typ_exp_unop: Term.UExp.op_un => (Typ.t, Typ.t) =
  fun
  | Int(Minus) => (Int, Int);

let rec any_to_info_map = (~ctx: Ctx.t, any: Term.any): (Ctx.co, map) =>
  switch (any) {
  | Exp(e) =>
    let (_, co, map) = uexp_to_info_map(~ctx, e);
    (co, map);
  | Pat(p) =>
    let (_, _, map) = upat_to_info_map(~is_synswitch=false, ~ctx, p);
    (VarMap.empty, map);
  | TPat(tp) =>
    let map = utpat_to_info_map(~ctx, tp);
    (VarMap.empty, map);
  | Typ(ty) =>
    let (_, map) = utyp_to_info_map(~ctx, ty);
    (VarMap.empty, map);
  // TODO(d) consider Rul case
  | Rul(_)
  | Nul ()
  | Any () => (VarMap.empty, Id.Map.empty)
  }
and uexp_to_info_map =
    (~ctx: Ctx.t, ~mode=Typ.Syn, {ids, term} as uexp: Term.UExp.t)
    : (Typ.t, Ctx.co, map) => {
  /* Maybe switch mode to syn */
  let mode =
    switch (mode) {
    | Ana(Unknown(SynSwitch)) => Typ.Syn
    | _ => mode
    };
  let cls = Term.UExp.cls_of_term(term);
  let go = uexp_to_info_map(~ctx);
  let add = (~self, ~free, ~extra_ids=[], m) => (
    typ_after_fix(ctx, mode, self),
    free,
    add_info(
      ids @ extra_ids,
      InfoExp({cls, self, mode, ctx, free, term: uexp}),
      m,
    ),
  );
  let atomic = self => add(~self, ~free=[], Id.Map.empty);
  switch (term) {
  | Invalid(msg) => (
      Unknown(Internal),
      [],
      add_info(ids, Invalid(msg), Id.Map.empty),
    )
  | MultiHole(tms) =>
    let (free, maps) = tms |> List.map(any_to_info_map(~ctx)) |> List.split;
    add(~self=Self(Multi), ~free=Ctx.union(free), union_m(maps));
  | EmptyHole => atomic(Just(Unknown(Internal)))
  | Triv => atomic(Just(Prod([])))
  | Bool(_) => atomic(Just(Bool))
  | Int(_) => atomic(Just(Int))
  | Float(_) => atomic(Just(Float))
  | String(_) => atomic(Just(String))
  | ListLit([]) => atomic(Just(List(Unknown(Internal))))
  | ListLit(es) =>
    let modes = List.init(List.length(es), _ => Typ.matched_list_mode(mode));
    let e_ids = List.map(exp_id, es);
    let infos = List.map2((e, mode) => go(~mode, e), es, modes);
    let tys = List.map(((ty, _, _)) => ty, infos);
    let self: Typ.self =
      switch (Ctx.join_all(ctx, tys)) {
      | None =>
        Joined(
          ty => List(ty),
          List.map2((id, ty) => Typ.{id, ty}, e_ids, tys),
        )
      | Some(ty) => Just(List(ty))
      };
    let free = Ctx.union(List.map(((_, f, _)) => f, infos));
    let m = union_m(List.map(((_, _, m)) => m, infos));
    add(~self, ~free, m);
  | Cons(e1, e2) =>
    let mode_e = Typ.matched_list_mode(mode);
    let (ty1, free1, m1) = go(~mode=mode_e, e1);
    let (_, free2, m2) = go(~mode=Ana(List(ty1)), e2);
    add(
      ~self=Just(List(ty1)),
      ~free=Ctx.union([free1, free2]),
      union_m([m1, m2]),
    );
  | Var(name) =>
    switch (Ctx.lookup_var(ctx, name)) {
    | None => atomic(Self(Free))
    | Some(var) =>
      add(
        ~self=Just(var.typ),
        ~free=[(name, [{id: exp_id(uexp), mode}])],
        Id.Map.empty,
      )
    }
  | Parens(e) =>
    let (ty, free, m) = go(~mode, e);
    add(~self=Just(ty), ~free, m);
  | UnOp(op, e) =>
    let (ty_in, ty_out) = typ_exp_unop(op);
    let (_, free, m) = go(~mode=Ana(ty_in), e);
    add(~self=Just(ty_out), ~free, m);
  | BinOp(op, e1, e2) =>
    let (ty1, ty2, ty_out) = typ_exp_binop(op);
    let (_, free1, m1) = go(~mode=Ana(ty1), e1);
    let (_, free2, m2) = go(~mode=Ana(ty2), e2);
    add(
      ~self=Just(ty_out),
      ~free=Ctx.union([free1, free2]),
      union_m([m1, m2]),
    );
  | Tuple(es) =>
    let modes = Typ.matched_prod_mode(mode, List.length(es));
    let infos = List.map2((e, mode) => go(~mode, e), es, modes);
    let free = Ctx.union(List.map(((_, f, _)) => f, infos));
    let self = Typ.Just(Prod(List.map(((ty, _, _)) => ty, infos)));
    let m = union_m(List.map(((_, _, m)) => m, infos));
    add(~self, ~free, m);
  | Test(test) =>
    let (_, free_test, m1) = go(~mode=Ana(Bool), test);
    add(~self=Just(Prod([])), ~free=free_test, m1);
  | If(cond, e1, e2) =>
    let (_, free_e0, m1) = go(~mode=Ana(Bool), cond);
    let (ty_e1, free_e1, m2) = go(~mode, e1);
    let (ty_e2, free_e2, m3) = go(~mode, e2);
    add(
      ~self=
        Joined(
          Fun.id,
          [{id: exp_id(e1), ty: ty_e1}, {id: exp_id(e2), ty: ty_e2}],
        ),
      ~free=Ctx.union([free_e0, free_e1, free_e2]),
      union_m([m1, m2, m3]),
    );
  | Seq(e1, e2) =>
    let (_, free1, m1) = go(~mode=Syn, e1);
    let (ty2, free2, m2) = go(~mode, e2);
    add(
      ~self=Just(ty2),
      ~free=Ctx.union([free1, free2]),
      union_m([m1, m2]),
    );
  | Tag(name) => atomic(tag_self(ctx, mode, name))
  | Ap(fn, arg) =>
    let fn_mode =
      switch (fn) {
      | {term: Tag(name), _} => tag_ap_mode(ctx, mode, name)
      | _ => Typ.ap_mode
      };
    let (ty_fn, free_fn, m_fn) = uexp_to_info_map(~ctx, ~mode=fn_mode, fn);
    let (ty_in, ty_out) = Typ.matched_arrow(ty_fn);
    let (_, free_arg, m_arg) =
      uexp_to_info_map(~ctx, ~mode=Ana(ty_in), arg);
    add(
      ~self=Just(ty_out),
      ~free=Ctx.union([free_fn, free_arg]),
      union_m([m_fn, m_arg]),
    );
  | Fun(pat, body) =>
    let (mode_pat, mode_body) = Typ.matched_arrow_mode(mode);
    let (ty_pat, ctx_pat, m_pat) =
      upat_to_info_map(~is_synswitch=false, ~ctx, ~mode=mode_pat, pat);
    let (ty_body, free_body, m_body) =
      uexp_to_info_map(~ctx=ctx_pat, ~mode=mode_body, body);
    add(
      ~self=Just(Arrow(ty_pat, ty_body)),
      ~free=Ctx.subtract_typ(ctx_pat, free_body), // TODO: free may not be accurate since ctx now threaded through pat
      union_m([m_pat, m_body]),
    );
  | Let(pat, def, body) =>
    let (ty_pat, ctx_pat, _m_pat) =
      upat_to_info_map(~is_synswitch=true, ~ctx, ~mode=Syn, pat);
    let def_ctx = extend_let_def_ctx(ctx, pat, ctx_pat, def);
    let (ty_def, free_def, m_def) =
      uexp_to_info_map(~ctx=def_ctx, ~mode=Ana(ty_pat), def);
    /* Analyze pattern to incorporate def type into ctx */
    let (_, ctx_pat_ana, m_pat) =
      upat_to_info_map(~is_synswitch=false, ~ctx, ~mode=Ana(ty_def), pat);
    let (ty_body, free_body, m_body) =
      uexp_to_info_map(~ctx=ctx_pat_ana, ~mode, body);
    add(
      ~self=Just(ty_body),
      ~free=Ctx.union([free_def, Ctx.subtract_typ(ctx_pat_ana, free_body)]), // TODO: free may not be accurate since ctx now threaded through pat
      union_m([m_pat, m_def, m_body]),
    );
  | TyAlias(typat, utyp, body) =>
    let m_typat = utpat_to_info_map(~ctx, typat);
    let ty = Term.UTyp.to_typ(ctx, utyp);
    let ctx =
      switch (typat) {
      | {term: Var(name), _} =>
        let ty_rec =
          List.mem(name, Typ.free_vars(ty)) ? Typ.Rec(name, ty) : ty;
        let ctx = Ctx.add_singleton(ctx, name, utpat_id(typat), ty_rec);
        switch (ty_rec) {
        | TSum(tags)
        | Rec(_, TSum(tags)) => Ctx.add_tags(ctx, name, typ_id(utyp), tags)
        | _ => ctx
        };
      | _ => ctx
      };
    let (ty_body, free, m_body) = uexp_to_info_map(~ctx, ~mode, body);
    let ty_body =
      switch (typat) {
      | {term: Var(name), _} => Typ.subst(ty, name, ty_body)
      | _ => ty_body
      };
    let m_typ = utyp_to_info_map(~ctx, utyp) |> snd;
    add(~self=Just(ty_body), ~free, union_m([m_typat, m_body, m_typ]));
  | Match(scrut, rules) =>
    let (ty_scrut, free_scrut, m_scrut) = go(~mode=Syn, scrut);
    let (pats, branches) = List.split(rules);
    let pat_infos =
      List.map(
        upat_to_info_map(~is_synswitch=false, ~ctx, ~mode=Typ.Ana(ty_scrut)),
        pats,
      );
    let branch_infos =
      List.map2(
        (branch, (_, ctx_pat, _)) =>
          uexp_to_info_map(~ctx=ctx_pat, ~mode, branch),
        branches,
        pat_infos,
      );
    let branch_sources =
      List.map2(
        (e: Term.UExp.t, (ty, _, _)) => Typ.{id: exp_id(e), ty},
        branches,
        branch_infos,
      );
    let pat_ms = List.map(((_, _, m)) => m, pat_infos);
    let branch_ms = List.map(((_, _, m)) => m, branch_infos);
    let branch_frees = List.map(((_, free, _)) => free, branch_infos);
    let self = Typ.Joined(Fun.id, branch_sources);
    let free = Ctx.union([free_scrut] @ branch_frees);
    add(~self, ~free, union_m([m_scrut] @ pat_ms @ branch_ms));
  };
}
and upat_to_info_map =
    (
      ~is_synswitch,
      ~ctx,
      ~mode: Typ.mode=Typ.Syn,
      {ids, term} as upat: Term.UPat.t,
    )
    : (Typ.t, Ctx.t, map) => {
  let upat_to_info_map = upat_to_info_map(~is_synswitch);
  let unknown = Typ.Unknown(is_synswitch ? SynSwitch : Internal);
  let cls = Term.UPat.cls_of_term(term);
  let add = (~self, ~ctx, ~extra_ids=[], m) => (
    typ_after_fix(ctx, mode, self),
    ctx,
    add_info(
      ids @ extra_ids,
      InfoPat({cls, self, mode, ctx, term: upat}),
      m,
    ),
  );
  let atomic = self => add(~self, ~ctx, Id.Map.empty);
  switch (term) {
  | Invalid(msg) => (
      Unknown(Internal),
      ctx,
      add_info(ids, Invalid(msg), Id.Map.empty),
    )
  | MultiHole(tms) =>
    let (_, maps) = tms |> List.map(any_to_info_map(~ctx)) |> List.split;
    add(~self=Self(Multi), ~ctx, union_m(maps));
  | EmptyHole => atomic(Just(unknown))
  | Int(_) => atomic(Just(Int))
  | Float(_) => atomic(Just(Float))
  | Triv => atomic(Just(Prod([])))
  | Bool(_) => atomic(Just(Bool))
  | String(_) => atomic(Just(String))
  | ListLit([]) => atomic(Just(List(Unknown(Internal))))
  | ListLit(ps) =>
    let modes = List.init(List.length(ps), _ => Typ.matched_list_mode(mode));
    let p_ids = List.map(pat_id, ps);
    let (ctx, infos) =
      List.fold_left2(
        ((ctx, infos), e, mode) => {
          let (_, ctx, _) as info = upat_to_info_map(~ctx, ~mode, e);
          (ctx, infos @ [info]);
        },
        (ctx, []),
        ps,
        modes,
      );
    let tys = List.map(((ty, _, _)) => ty, infos);
    let self: Typ.self =
      switch (Ctx.join_all(ctx, tys)) {
      | None =>
        Joined(
          ty => List(ty),
          List.map2((id, ty) => Typ.{id, ty}, p_ids, tys),
        )
      | Some(ty) => Just(List(ty))
      };
    let info: t = InfoPat({cls, self, mode, ctx, term: upat});
    let m = union_m(List.map(((_, _, m)) => m, infos));
    /* Add an entry for the id of each comma tile */
    let m = List.fold_left((m, id) => Id.Map.add(id, info, m), m, ids);
    (typ_after_fix(ctx, mode, self), ctx, m);
  | Cons(hd, tl) =>
    let mode_e = Typ.matched_list_mode(mode);
    let (ty1, ctx, m_hd) = upat_to_info_map(~ctx, ~mode=mode_e, hd);
    let (_, ctx, m_tl) = upat_to_info_map(~ctx, ~mode=Ana(List(ty1)), tl);
    add(~self=Just(List(ty1)), ~ctx, union_m([m_hd, m_tl]));
  | Wild => atomic(Just(unknown))
  | Var(name) =>
    let typ = typ_after_fix(ctx, mode, Just(Unknown(Internal)));
    let entry = Ctx.VarEntry({name, id: pat_id(upat), typ});
    add(~self=Just(unknown), ~ctx=Ctx.extend(entry, ctx), Id.Map.empty);
  | Tuple(ps) =>
    let modes = Typ.matched_prod_mode(mode, List.length(ps));
    let (ctx, infos) =
      List.fold_left2(
        ((ctx, infos), e, mode) => {
          let (_, ctx, _) as info = upat_to_info_map(~mode, ~ctx, e);
          (ctx, infos @ [info]);
        },
        (ctx, []),
        ps,
        modes,
      );
    let self = Typ.Just(Prod(List.map(((ty, _, _)) => ty, infos)));
    let m = union_m(List.map(((_, _, m)) => m, infos));
    add(~self, ~ctx, m);
  | Parens(p) =>
    let (ty, ctx, m) = upat_to_info_map(~ctx, ~mode, p);
    add(~self=Just(ty), ~ctx, m);
  | Tag(name) => atomic(tag_self(ctx, mode, name))
  | Ap(fn, arg) =>
    /* Constructors */
    let fn_mode =
      switch (fn) {
      | {term: Tag(name), _} => tag_ap_mode(ctx, mode, name)
      | _ => Typ.ap_mode
      };
    let (ty_fn, ctx, m_fn) = upat_to_info_map(~ctx, ~mode=fn_mode, fn);
    let (ty_in, ty_out) = Typ.matched_arrow(ty_fn);
    let (_, ctx, m_arg) = upat_to_info_map(~ctx, ~mode=Ana(ty_in), arg);
    add(~self=Just(ty_out), ~ctx, union_m([m_fn, m_arg]));
  | TypeAnn(p, ty) =>
    let (ty_ann, m_typ) = utyp_to_info_map(~ctx, ty);
    let (_, ctx, m) = upat_to_info_map(~ctx, ~mode=Ana(ty_ann), p);
    add(~self=Just(ty_ann), ~ctx, union_m([m, m_typ]));
  };
}
and utyp_to_info_map =
    (~ctx, ~mode=Normal, {ids, term} as utyp: Term.UTyp.t): (Typ.t, map) => {
  let cls: Term.UTyp.cls =
    switch (mode, Term.UTyp.cls_of_term(term)) {
    | (VariantExpected(_), Var) => Tag
    | (_, cls) => cls
    };
  let ty = Term.UTyp.to_typ(ctx, utyp);
  let add = status =>
    add_info(ids, InfoTyp({cls, ctx, mode, status, term: utyp}));
  let ok = (m: map): (Typ.t, map) => (ty, add(Ok(ty), m));
  let error = (err, m) => (Typ.Unknown(Internal), add(err, m));
  let normal = m => mode != Normal ? error(TagExpected(ty), m) : ok(m);
  let go = utyp_to_info_map(~ctx, ~mode=Normal);
  //TODO(andrew): make this return free, replacing Typ.free_vars
  //TODO: refactor this along status+mode=>fix lines
  switch (term) {
  | EmptyHole => ok(Id.Map.empty)
  | Int
  | Float
  | Bool
  | String =>
    let m = Id.Map.empty;
    normal(m);
  | List(t)
  | Parens(t) =>
    let m = go(t) |> snd;
    normal(m);
  | Arrow(t1, t2) =>
    let m = union_m([go(t1) |> snd, go(t2) |> snd]);
    normal(m);
  | Tuple(ts) =>
    let m = ts |> List.map(go) |> List.map(snd) |> union_m;
    normal(m);
  | Var(name)
  | Tag(name) =>
    let m = Id.Map.empty;
    switch (mode) {
    | VariantExpected(Duplicate) => error(DuplicateTag, m)
    | VariantExpected(Unique) => ok(m)
    | Normal => Ctx.is_tvar(ctx, name) ? ok(m) : error(FreeTypeVar, m)
    };
  | Ap(t1, t2) =>
    let t1_mode =
      switch (mode) {
      | VariantExpected(_) => mode
      | Normal => VariantExpected(Unique)
      };
    let m =
      union_m([
        utyp_to_info_map(~ctx, ~mode=t1_mode, t1) |> snd,
        utyp_to_info_map(~ctx, ~mode=Normal, t2) |> snd,
      ]);
    switch (mode) {
    | VariantExpected(_) => ok(m)
    | Normal => error(ApOutsideSum, m)
    };
  | UTSum(ts) =>
    let (ms, _) =
      List.fold_left(
        ((acc, tags), ut) => {
          let (status, tag) =
            switch (Term.UTyp.get_tag(ctx, ut)) {
            | None => (Unique, [])
            | Some(tag) when !List.mem(tag, tags) => (Unique, [tag])
            | Some(tag) => (Duplicate, [tag])
            };
          let m =
            utyp_to_info_map(~ctx, ~mode=VariantExpected(status), ut) |> snd;
          (acc @ [m], tags @ tag);
        },
        ([], []),
        ts,
      );
    normal(union_m(ms));
  | MultiHole(tms) =>
    let (_, maps) = tms |> List.map(any_to_info_map(~ctx)) |> List.split;
    ok(union_m(maps));
  };
}
and utpat_to_info_map = (~ctx as _, {ids, term} as utpat: Term.UTPat.t): map => {
  let cls = Term.UTPat.cls_of_term(term);
  let status = status_tpat(utpat);
  add_info(ids, InfoTPat({cls, status, term: utpat}), Id.Map.empty);
};

let mk_map =
  Core.Memo.general(
    ~cache_size_bound=1000,
    e => {
      let (_, _, map) =
        uexp_to_info_map(~ctx=Builtins.ctx(Builtins.Pervasives.builtins), e);
      map;
    },
  );

let get_binding_site = (id: Id.t, statics_map: map): option(Id.t) => {
  open OptUtil.Syntax;
  let* opt = Id.Map.find_opt(id, statics_map);
  let* info_exp =
    switch (opt) {
    | InfoExp(info_exp) => Some(info_exp)
    | _ => None
    };

  let+ entry =
    switch (info_exp.term.term) {
    | TermBase.UExp.Var(name) => Ctx.lookup_var(info_exp.ctx, name)
    | _ => None
    };
  entry.id;
};
