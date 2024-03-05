open Util;
open OptUtil.Syntax;

/*
 Currently, Elaboration does the following things:

  - Insert casts
  - Insert non-empty hole wrappers
  - Remove TyAlias [should we do this??]
  - Annotate functions with types, and names
  - Insert implicit fixpoints (in types and expressions)
  - Remove parentheses

  Going the other way:
  - There's going to be a horrible case with implicit fixpoint shadowing

  A nice property would be that elaboration is idempotent...
  */

module Elaboration = {
  [@deriving (show({with_path: false}), sexp, yojson)]
  type t = {
    d: DExp.t,
    info_map: Statics.Map.t,
  };
};

module ElaborationResult = {
  [@deriving sexp]
  type t =
    | Elaborates(DExp.t, Typ.t, Delta.t)
    | DoesNotElaborate;
};

let fixed_exp_typ = (m: Statics.Map.t, e: UExp.t): option(Typ.t) =>
  switch (Id.Map.find_opt(UExp.rep_id(e), m)) {
  | Some(InfoExp({ty, _})) => Some(ty)
  | _ => None
  };

let fixed_pat_typ = (m: Statics.Map.t, p: UPat.t): option(Typ.t) =>
  switch (Id.Map.find_opt(UPat.rep_id(p), m)) {
  | Some(InfoPat({ty, _})) => Some(ty)
  | _ => None
  };

let cast = (ctx: Ctx.t, mode: Mode.t, self_ty: Typ.t, d: DExp.t) =>
  switch (mode) {
  | Syn => d
  | SynFun =>
    switch (self_ty) {
    | Unknown(prov) =>
      DExp.fresh_cast(
        d,
        Unknown(prov),
        Arrow(Unknown(prov), Unknown(prov)),
      )
    | Arrow(_) => d
    | _ => failwith("Elaborator.wrap: SynFun non-arrow-type")
    }
  | Ana(ana_ty) =>
    let ana_ty = Typ.normalize(ctx, ana_ty);
    /* Forms with special ana rules get cast from their appropriate Matched types */
    switch (DExp.term_of(d)) {
    | ListLit(_)
    | ListConcat(_)
    | Cons(_) =>
      switch (ana_ty) {
      | Unknown(prov) =>
        DExp.fresh_cast(d, List(Unknown(prov)), Unknown(prov))
      | _ => d
      }
    | Fun(_) =>
      /* See regression tests in Documentation/Dynamics */
      let (_, ana_out) = Typ.matched_arrow(ctx, ana_ty);
      let (self_in, _) = Typ.matched_arrow(ctx, self_ty);
      DExp.fresh_cast(d, Arrow(self_in, ana_out), ana_ty);
    | Tuple(ds) =>
      switch (ana_ty) {
      | Unknown(prov) =>
        let us = List.init(List.length(ds), _ => Typ.Unknown(prov));
        DExp.fresh_cast(d, Prod(us), Unknown(prov));
      | _ => d
      }
    | Constructor(_) =>
      switch (ana_ty, self_ty) {
      | (Unknown(prov), Rec(_, Sum(_)))
      | (Unknown(prov), Sum(_)) =>
        DExp.fresh_cast(d, self_ty, Unknown(prov))
      | _ => d
      }
    | Ap(_, f, _) =>
      switch (DExp.term_of(f)) {
      | Constructor(_) =>
        switch (ana_ty, self_ty) {
        | (Unknown(prov), Rec(_, Sum(_)))
        | (Unknown(prov), Sum(_)) =>
          DExp.fresh_cast(d, self_ty, Unknown(prov))
        | _ => d
        }
      | StaticErrorHole(_, g) =>
        switch (DExp.term_of(g)) {
        | Constructor(_) =>
          switch (ana_ty, self_ty) {
          | (Unknown(prov), Rec(_, Sum(_)))
          | (Unknown(prov), Sum(_)) =>
            DExp.fresh_cast(d, self_ty, Unknown(prov))
          | _ => d
          }
        | _ => DExp.fresh_cast(d, self_ty, ana_ty)
        }
      | _ => DExp.fresh_cast(d, self_ty, ana_ty)
      }
    /* Forms with special ana rules but no particular typing requirements */
    | Match(_)
    | If(_)
    | Seq(_)
    | Let(_)
    | FixF(_) => d
    /* Hole-like forms: Don't cast */
    | Invalid(_)
    | EmptyHole
    | MultiHole(_)
    | StaticErrorHole(_) => d
    /* DExp-specific forms: Don't cast */
    | Cast(_)
    | Closure(_)
    | Filter(_)
    | FailedCast(_)
    | DynamicErrorHole(_) => d
    /* Normal cases: wrap */
    | Var(_)
    | BuiltinFun(_)
    | Parens(_)
    | Bool(_)
    | Int(_)
    | Float(_)
    | String(_)
    | UnOp(_)
    | BinOp(_)
    | TyAlias(_)
    | Test(_) => DExp.fresh_cast(d, self_ty, ana_ty)
    };
  };

/* Handles cast insertion and non-empty-hole wrapping
   for elaborated expressions */
let wrap = (ctx: Ctx.t, u: Id.t, mode: Mode.t, self, d: DExp.t): DExp.t =>
  switch (Info.status_exp(ctx, mode, self)) {
  | NotInHole(_) =>
    let self_ty =
      switch (Self.typ_of_exp(ctx, self)) {
      | Some(self_ty) => Typ.normalize(ctx, self_ty)
      | None => Unknown(Internal)
      };
    cast(ctx, mode, self_ty, d);
  | InHole(
      FreeVariable(_) | Common(NoType(_)) |
      Common(Inconsistent(Internal(_))),
    ) => d
  | InHole(Common(Inconsistent(Expectation(_) | WithArrow(_)))) =>
    DExp.fresh(StaticErrorHole(u, d))
  };

let rec dhexp_of_uexp =
        (m: Statics.Map.t, uexp: UExp.t, in_filter: bool): option(DExp.t) => {
  let dhexp_of_uexp = (~in_filter=in_filter, m, uexp) => {
    dhexp_of_uexp(m, uexp, in_filter);
  };
  switch (Id.Map.find_opt(UExp.rep_id(uexp), m)) {
  | Some(InfoExp({mode, self, ctx, _})) =>
    let err_status = Info.status_exp(ctx, mode, self);
    let id = UExp.rep_id(uexp); /* NOTE: using term uids for hole ids */
    let rewrap = DExp.mk(uexp.ids);
    let+ d: DExp.t =
      switch (uexp.term) {
      // TODO: make closure actually convert
      | Closure(_, d) => dhexp_of_uexp(m, d)
      | Cast(d1, t1, t2) =>
        let+ d1' = dhexp_of_uexp(m, d1);
        Cast(d1', t1, t2) |> rewrap;
      | Invalid(t) => Some(DExp.Invalid(t) |> rewrap)
      | EmptyHole => Some(DExp.EmptyHole |> rewrap)
      | MultiHole(_: list(TermBase.Any.t)) => Some(EmptyHole |> rewrap)
      // switch (
      //   us
      //   |> List.filter_map(
      //        fun
      //        | TermBase.Any.Exp(x) => Some(x)
      //        | _ => None,
      //      )
      // ) {
      // | [] => Some(DExp.EmptyHole |> rewrap)
      // | us =>
      //   let+ ds = us |> List.map(dhexp_of_uexp(m)) |> OptUtil.sequence;
      //   DExp.MultiHole(ds) |> rewrap;
      // }
      | StaticErrorHole(_, e) => dhexp_of_uexp(m, e)
      | DynamicErrorHole(e, err) =>
        let+ d1 = dhexp_of_uexp(m, e);
        DExp.DynamicErrorHole(d1, err) |> rewrap;
      | FailedCast(e, t1, t2) =>
        let+ d1 = dhexp_of_uexp(m, e);
        DExp.FailedCast(d1, t1, t2) |> rewrap;
      /* TODO: add a dhexp case and eval logic for multiholes.
         Make sure new dhexp form is properly considered Indet
         to avoid casting issues. */
      | Bool(_)
      | Int(_)
      | Float(_)
      | String(_) => Some(uexp)
      | ListLit(es) =>
        let+ ds = es |> List.map(dhexp_of_uexp(m)) |> OptUtil.sequence;
        DExp.ListLit(ds) |> rewrap;
      | Fun(p, body, _, _) =>
        let+ d1 = dhexp_of_uexp(m, body);
        DExp.Fun(p, d1, None, None) |> rewrap;
      | Tuple(es) =>
        let+ ds = es |> List.map(dhexp_of_uexp(m)) |> OptUtil.sequence;
        DExp.Tuple(ds) |> rewrap;
      | Cons(e1, e2) =>
        let* dc1 = dhexp_of_uexp(m, e1);
        let+ dc2 = dhexp_of_uexp(m, e2);
        DExp.Cons(dc1, dc2) |> rewrap;
      | ListConcat(e1, e2) =>
        let* dc1 = dhexp_of_uexp(m, e1);
        let+ dc2 = dhexp_of_uexp(m, e2);
        DExp.ListConcat(dc1, dc2) |> rewrap;
      | UnOp(Meta(Unquote), e) =>
        switch (e.term) {
        | Var("e") when in_filter => Some(Constructor("$e") |> DExp.fresh)
        | Var("v") when in_filter => Some(Constructor("$v") |> DExp.fresh)
        | _ => Some(DExp.EmptyHole |> rewrap)
        }
      | UnOp(Int(Minus), e) =>
        let+ dc = dhexp_of_uexp(m, e);
        DExp.UnOp(Int(Minus), dc) |> rewrap;
      | UnOp(Bool(Not), e) =>
        let+ dc = dhexp_of_uexp(m, e);
        DExp.UnOp(Bool(Not), dc) |> rewrap;
      | BinOp(op, e1, e2) =>
        let* dc1 = dhexp_of_uexp(m, e1);
        let+ dc2 = dhexp_of_uexp(m, e2);
        DExp.BinOp(op, dc1, dc2) |> rewrap;
      | BuiltinFun(name) => Some(DExp.BuiltinFun(name) |> rewrap)
      | Parens(e) => dhexp_of_uexp(m, e)
      | Seq(e1, e2) =>
        let* d1 = dhexp_of_uexp(m, e1);
        let+ d2 = dhexp_of_uexp(m, e2);
        DExp.Seq(d1, d2) |> rewrap;
      | Test(test) =>
        let+ dtest = dhexp_of_uexp(m, test);
        DExp.Test(dtest) |> rewrap;
      | Filter(Filter({act, pat: cond}), body) =>
        let* dcond = dhexp_of_uexp(~in_filter=true, m, cond);
        let+ dbody = dhexp_of_uexp(m, body);
        DExp.Filter(Filter({act, pat: dcond}), dbody) |> rewrap;
      | Filter(Residue(_) as residue, body) =>
        let+ dbody = dhexp_of_uexp(m, body);
        DExp.Filter(residue, dbody) |> rewrap;
      | Var(name) => Some(Var(name) |> rewrap)
      | Constructor(name) => Some(Constructor(name) |> rewrap)
      | Let(p, def, body) =>
        let add_name: (option(string), DExp.t) => DExp.t = (
          (name, d) => {
            let (term, rewrap) = DExp.unwrap(d);
            switch (term) {
            | Fun(p, e, ctx, _) => DExp.Fun(p, e, ctx, name) |> rewrap
            | _ => d
            };
          }
        );
        let* ddef = dhexp_of_uexp(m, def);
        let+ dbody = dhexp_of_uexp(m, body);
        switch (UPat.get_recursive_bindings(p)) {
        | None =>
          /* not recursive */
          DExp.Let(p, add_name(UPat.get_var(p), ddef), dbody) |> rewrap
        | Some(b) =>
          DExp.Let(
            p,
            FixF(p, add_name(Some(String.concat(",", b)), ddef), None)
            |> DExp.fresh,
            dbody,
          )
          |> rewrap
        };
      | FixF(p, e, _) =>
        let+ de = dhexp_of_uexp(m, e);
        DExp.FixF(p, de, None) |> rewrap;
      | Ap(dir, fn, arg) =>
        let* c_fn = dhexp_of_uexp(m, fn);
        let+ c_arg = dhexp_of_uexp(m, arg);
        DExp.Ap(dir, c_fn, c_arg) |> rewrap;
      | If(c, e1, e2) =>
        let* c' = dhexp_of_uexp(m, c);
        let* d1 = dhexp_of_uexp(m, e1);
        let+ d2 = dhexp_of_uexp(m, e2);
        // Use tag to mark inconsistent branches
        switch (err_status) {
        | InHole(Common(Inconsistent(Internal(_)))) =>
          DExp.If(c', d1, d2) |> rewrap
        | _ => DExp.If(c', d1, d2) |> rewrap
        };
      | Match(scrut, rules) =>
        let* d_scrut = dhexp_of_uexp(m, scrut);
        let+ d_rules =
          List.map(
            ((p, e)) => {
              let+ d_e = dhexp_of_uexp(m, e);
              (p, d_e);
            },
            rules,
          )
          |> OptUtil.sequence;
        switch (err_status) {
        | InHole(Common(Inconsistent(Internal(_)))) =>
          DExp.Match(d_scrut, d_rules) |> rewrap
        | _ => DExp.Match(d_scrut, d_rules) |> rewrap
        };
      | TyAlias(_, _, e) => dhexp_of_uexp(m, e)
      };
    switch (uexp.term) {
    | Parens(_) => d
    | _ => wrap(ctx, id, mode, self, d)
    };
  | Some(InfoPat(_) | InfoTyp(_) | InfoTPat(_) | Secondary(_))
  | None => None
  };
};

//let dhexp_of_uexp = Core.Memo.general(~cache_size_bound=1000, dhexp_of_uexp);

let uexp_elab = (m: Statics.Map.t, uexp: UExp.t): ElaborationResult.t =>
  switch (dhexp_of_uexp(m, uexp, false)) {
  | None => DoesNotElaborate
  | Some(d) =>
    //let d = uexp_elab_wrap_builtins(d);
    let ty =
      switch (fixed_exp_typ(m, uexp)) {
      | Some(ty) => ty
      | None => Typ.Unknown(Internal)
      };
    Elaborates(d, ty, Delta.empty);
  };
