open Sexplib.Std;

module ContextRef = {
  /* TODO: (eric) is there a way to incorporate peer type info? */
  [@deriving sexp]
  type s('idx) = {
    index: Index.t('idx),
    stamp: int,
    predecessors: list(string),
    successors: list(string),
  };

  [@deriving sexp]
  type t = s(Index.absolute);

  let equal = (cref: t, cref': t): bool =>
    Index.equal(cref.index, cref'.index)
    && Int.equal(cref.stamp, cref'.stamp);
};

module HTyp_syntax: {
  [@deriving sexp]
  type t('idx) =
    | Hole
    | Int
    | Float
    | Bool
    | Arrow(t('idx), t('idx))
    | Sum(t('idx), t('idx))
    | Prod(list(t('idx)))
    | List(t('idx))
    | TyVar(ContextRef.s('idx), TyVar.t)
    | TyVarHole(TyVarErrStatus.HoleReason.t, MetaVar.t, TyVar.t);
  let to_rel: (~offset: int=?, t(Index.absolute)) => t(Index.relative);
  let to_abs: (~offset: int=?, t(Index.relative)) => t(Index.absolute);
  let subst_tyvar: (t('idx), Index.t('idx), t('idx)) => t('idx);
  let subst_tyvars: (t('idx), list((Index.t('idx), t('idx)))) => t('idx);
} = {
  [@deriving sexp]
  type t('idx) =
    | Hole
    | Int
    | Float
    | Bool
    | Arrow(t('idx), t('idx))
    | Sum(t('idx), t('idx))
    | Prod(list(t('idx)))
    | List(t('idx))
    | TyVar(ContextRef.s('idx), TyVar.t)
    | TyVarHole(TyVarErrStatus.HoleReason.t, MetaVar.t, TyVar.t);

  let rec to_rel =
          (~offset: int=0, ty: t(Index.absolute)): t(Index.relative) =>
    switch (ty) {
    | TyVar(cref, t) =>
      let cref = {...cref, index: Index.Abs.to_rel(~offset, cref.index)};
      TyVar(cref, t);
    | TyVarHole(reason, u, name) => TyVarHole(reason, u, name)
    | Hole => Hole
    | Int => Int
    | Float => Float
    | Bool => Bool
    | Arrow(ty1, ty2) => Arrow(to_rel(~offset, ty1), to_rel(~offset, ty2))
    | Sum(tyL, tyR) => Sum(to_rel(~offset, tyL), to_rel(~offset, tyR))
    | Prod(tys) => Prod(List.map(to_rel(~offset), tys))
    | List(ty) => List(to_rel(~offset, ty))
    };

  let rec to_abs =
          (~offset: int=0, ty: t(Index.relative)): t(Index.absolute) =>
    switch (ty) {
    | TyVar(cref, t) =>
      let index = Index.Rel.to_abs(~offset, cref.index);
      let stamp = cref.stamp + offset;
      let cref = {...cref, index, stamp};
      TyVar(cref, t);
    | TyVarHole(reason, u, name) => TyVarHole(reason, u, name)
    | Hole => Hole
    | Int => Int
    | Float => Float
    | Bool => Bool
    | Arrow(ty1, ty2) => Arrow(to_abs(~offset, ty1), to_abs(~offset, ty2))
    | Sum(tyL, tyR) => Sum(to_abs(~offset, tyL), to_abs(~offset, tyR))
    | Prod(tys) => Prod(List.map(to_abs(~offset), tys))
    | List(ty) => List(to_abs(~offset, ty))
    };

  let rec subst_tyvar =
          (ty: t('idx), idx: Index.t('idx), new_ty: t('idx)): t('idx) => {
    let go = ty1 => subst_tyvar(ty1, idx, new_ty);
    switch (ty) {
    | TyVar(cref', _) => Index.equal(idx, cref'.index) ? new_ty : ty
    | TyVarHole(_)
    | Hole
    | Int
    | Float
    | Bool => ty
    | Arrow(ty1, ty2) => Arrow(go(ty1), go(ty2))
    | Sum(tyL, tyR) => Sum(go(tyL), go(tyR))
    | Prod(tys) => Prod(List.map(go, tys))
    | List(ty) => List(go(ty))
    };
  };

  let subst_tyvars =
      (ty: t('idx), tyvars: list((Index.t('idx), t('idx)))): t('idx) =>
    List.fold_left(
      (ty, (idx, ty_idx)) => subst_tyvar(ty, idx, ty_idx),
      ty,
      tyvars,
    );
};

module Kind_core: {
  [@deriving sexp]
  type s('idx) =
    | Hole
    | Type
    | S(HTyp_syntax.t('idx));
  let to_rel: (~offset: int=?, s(Index.absolute)) => s(Index.relative);
  let to_abs: (~offset: int=?, s(Index.relative)) => s(Index.absolute);
} = {
  [@deriving sexp]
  type s('idx) =
    | Hole
    | Type
    | S(HTyp_syntax.t('idx));

  let to_rel = (~offset: int=0, k: s(Index.absolute)): s(Index.relative) =>
    switch (k) {
    | Hole => Hole
    | Type => Type
    | S(ty) => S(HTyp_syntax.to_rel(~offset, ty))
    };

  let to_abs = (~offset: int=0, k: s(Index.relative)): s(Index.absolute) =>
    switch (k) {
    | Hole => Hole
    | Type => Type
    | S(ty) => S(HTyp_syntax.to_abs(~offset, ty))
    };
};

module rec Context: {
  [@deriving sexp]
  type binding;
  [@deriving sexp]
  type t = list(binding);
  [@deriving sexp]
  type entry =
    | VarEntry(Var.t, HTyp.t)
    | TyVarEntry(TyVar.t, Kind.t);
  let binding_name: binding => string;
  let empty: unit => t;
  let to_list:
    t =>
    (
      list((Var.t, HTyp_syntax.t(Index.relative))),
      list((TyVar.t, Kind_core.s(Index.relative))),
    );
  let of_entries: list(entry) => t;
  let entries: t => list(entry);
  let length: t => int;
  let rescope: (t, ContextRef.t) => ContextRef.t;
  let tyvars: t => list((ContextRef.t, TyVar.t, Kind.t));
  let tyvar: (t, ContextRef.t) => option(TyVar.t);
  let tyvar_ref: (t, TyVar.t) => option(ContextRef.t);
  let tyvar_kind: (t, ContextRef.t) => option(Kind.t);
  let add_tyvar: (t, TyVar.t, Kind.t) => t;
  let reduce_tyvars: (t, t, HTyp.t) => HTyp.t;
  let vars: t => list((ContextRef.t, Var.t, HTyp.t));
  let var: (t, ContextRef.t) => option(Var.t);
  let var_ref: (t, Var.t) => option(ContextRef.t);
  let var_type: (t, Var.t) => option(HTyp.t);
  let add_var: (t, Var.t, HTyp.t) => t;
} = {
  [@deriving sexp]
  type binding =
    | VarBinding(Var.t, HTyp_syntax.t(Index.relative))
    | TyVarBinding(TyVar.t, Kind_core.s(Index.relative));

  [@deriving sexp]
  type t = list(binding);

  [@deriving sexp]
  type entry =
    | VarEntry(Var.t, HTyp.t)
    | TyVarEntry(TyVar.t, Kind.t);

  let empty: unit => t = () => [];

  let to_list =
      (ctx: t)
      : (
          list((Var.t, HTyp_syntax.t(Index.relative))),
          list((TyVar.t, Kind_core.s(Index.relative))),
        ) =>
    List.fold_right(
      (binding, (vars, tyvars)) =>
        switch (binding) {
        | VarBinding(x, ty) => ([(x, ty), ...vars], tyvars)
        | TyVarBinding(t, k) => (vars, [(t, k), ...tyvars])
        },
      ctx,
      ([], []),
    );

  let length = List.length;

  let binding_name =
    fun
    | VarBinding(x, _) => x
    | TyVarBinding(t, _) => t;

  // The general idea behind "peer tracking", i.e., predecessors and successors,is:
  //
  // 1. Predecessors should never change.
  //
  //   different predecessors  ===>  different pasts
  //
  // 2. One successor should always be a prefix of the other.
  //
  //   deviating successors  ==>  deviating futures (since cref was constructed)
  //
  // new stamp = old stamp  ==>  new successors = pivot(ctx, index)[0]
  // new stamp > old stamp  ==>  new successors = old successors + new entries
  // new stamp < old stamp  ==>  new successors = new successors - old entries

  let rescope = (ctx: t, cref: ContextRef.t): ContextRef.t =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("cref", () => ContextRef.sexp_of_t(cref)),
      ],
      ~result_sexp=ContextRef.sexp_of_t,
      () => {
        let stamp = Context.length(ctx);
        if (stamp == 0) {
          failwith(
            __LOC__ ++ ": cannot rescope type variables in an empty context",
          );
        };
        let amount = stamp - cref.stamp;
        let i = Index.Abs.to_int(cref.index) + amount;
        if (i >= stamp) {
          failwith(__LOC__ ++ ": rescoping context is in the past");
        } else if (i < 0) {
          failwith(__LOC__ ++ ": rescoping context is in the future");
        };
        let (successors, _, predecessors) =
          ctx |> List.map(binding_name) |> ListUtil.pivot(i);
        if (List.length(predecessors) != List.length(cref.predecessors)
            || !List.for_all2(String.equal, predecessors, cref.predecessors)) {
          failwith(
            __LOC__
            ++ ": cannot rescope index in an incompatbile context: different pasts",
          );
        };
        let n = List.length(successors);
        let m = List.length(cref.successors);
        let (succs1, succs2) =
          switch (n > m, n < m) {
          | (true, _) => (ListUtil.drop(n - m, successors), cref.successors)
          | (_, true) => (successors, ListUtil.drop(m - n, cref.successors))
          | _ => (successors, cref.successors)
          };
        if (!List.for_all2(String.equal, succs1, succs2)) {
          failwith(
            __LOC__
            ++ ": cannot rescope index in incompatible context: diverging futures",
          );
        };
        let index = Index.Abs.of_int(i);
        let cref = ContextRef.{index, stamp, predecessors, successors};
        if (Index.Abs.to_int(cref.index) >= stamp) {
          failwith(__LOC__ ++ ": rescoped type variable is in the future");
        };
        if (Index.Abs.to_int(cref.index) < 0) {
          failwith(__LOC__ ++ ": rescoped type variable is in the past");
        };
        cref;
      },
    );

  let nth_var_binding =
      (ctx: t, n: int): option((Var.t, HTyp_syntax.t(Index.relative))) => {
    open OptUtil.Syntax;
    let* binding = List.nth_opt(ctx, n);
    switch (binding) {
    | VarBinding(x, ty) => Some((x, ty))
    | TyVarBinding(_) => None
    };
  };

  let nth_tyvar_binding =
      (ctx: t, n: int): option((Var.t, Kind_core.s(Index.relative))) => {
    open OptUtil.Syntax;
    let* binding = List.nth_opt(ctx, n);
    switch (binding) {
    | TyVarBinding(t, k) => Some((t, k))
    | VarBinding(_) => None
    };
  };

  let first_var_binding =
      (ctx: t, f: (Var.t, HTyp_syntax.t(Index.relative)) => bool)
      : option((int, Var.t, HTyp_syntax.t(Index.relative))) => {
    let rec go = (i, ctx) =>
      switch (ctx) {
      | [VarBinding(x, ty), ...ctx'] =>
        f(x, ty) ? Some((i, x, ty)) : go(i + 1, ctx')
      | [TyVarBinding(_), ...ctx'] => go(i + 1, ctx')
      | [] => None
      };
    go(0, ctx);
  };

  let first_tyvar_binding =
      (ctx: t, f: (TyVar.t, Kind_core.s(Index.relative)) => bool)
      : option((int, TyVar.t, Kind_core.s(Index.relative))) => {
    let rec go = (i, ctx) =>
      switch (ctx) {
      | [TyVarBinding(t, k), ...ctx'] =>
        f(t, k) ? Some((i, t, k)) : go(i + 1, ctx')
      | [VarBinding(_), ...ctx'] => go(i + 1, ctx')
      | [] => None
      };
    go(0, ctx);
  };

  /* Type Variables */

  let tyvars = (ctx: t): list((ContextRef.t, TyVar.t, Kind.t)) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[("ctx", () => sexp_of_t(ctx))],
      ~result_sexp=
        Sexplib.Std.sexp_of_list(((cref, t, k)) =>
          List([
            ContextRef.sexp_of_t(cref),
            TyVar.sexp_of_t(t),
            Kind.sexp_of_t(k),
          ])
        ),
      () => {
        let stamp = length(ctx);
        ctx
        |> List.mapi((i, binding) => (i, binding))
        |> List.fold_left(
             (((predecessors, successors), tyvars), (i, binding)) =>
               switch (binding) {
               | TyVarBinding(t, k) =>
                 let index = Index.Abs.of_int(i);
                 let k = Kind_core.to_abs(~offset=i, k);
                 let predecessors = ListUtil.drop(1, predecessors);
                 let cref =
                   ContextRef.{index, stamp, predecessors, successors};
                 let successors = successors @ [t];
                 ((predecessors, successors), [(cref, t, k), ...tyvars]);
               | VarBinding(x, _) =>
                 let predecessors = ListUtil.drop(1, predecessors);
                 let successors = successors @ [x];
                 ((predecessors, successors), tyvars);
               },
             ((List.map(binding_name, ctx), []), []),
           )
        |> snd
        /* TODO: (eric) redo the right way around */
        |> List.rev;
      },
    );

  let tyvar = (ctx: t, cref: ContextRef.t): option(TyVar.t) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => sexp_of_t(ctx)),
        ("cref", () => ContextRef.sexp_of_t(cref)),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_option(TyVar.sexp_of_t),
      () => {
        open OptUtil.Syntax;
        let cref = rescope(ctx, cref);
        let+ (t, _) = nth_tyvar_binding(ctx, Index.Abs.to_int(cref.index));
        t;
      },
    );

  let tyvar_ref = (ctx: t, t: TyVar.t): option(ContextRef.t) => {
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => sexp_of_t(ctx)),
        ("t", () => TyVar.sexp_of_t(t)),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_option(ContextRef.sexp_of_t),
      () => {
        open OptUtil.Syntax;
        let+ (i, _, _) =
          first_tyvar_binding(ctx, (t', _) => TyVar.equal(t, t'));
        let index = Index.Abs.of_int(i);
        let stamp = Context.length(ctx);
        let (successors, _, predecessors) =
          ctx |> List.map(binding_name) |> ListUtil.pivot(i);
        ContextRef.{index, stamp, predecessors, successors};
      },
    );
  };

  let tyvar_kind = (ctx: t, cref: ContextRef.t): option(Kind.t) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => sexp_of_t(ctx)),
        ("cref", () => ContextRef.sexp_of_t(cref)),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_option(Kind.sexp_of_t),
      () => {
        open OptUtil.Syntax;
        let cref = rescope(ctx, cref);
        let i = Index.Abs.to_int(cref.index);
        let+ (_, k) = nth_tyvar_binding(ctx, i);
        let k' = Kind_core.to_abs(~offset=i + 1, k);
        Kind.rescope(ctx, k');
      },
    );

  let add_tyvar = (ctx: t, t: TyVar.t, k: Kind.t): t =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => sexp_of_t(ctx)),
        ("t", () => TyVar.sexp_of_t(t)),
        ("k", () => Kind.sexp_of_t(k)),
      ],
      ~result_sexp=sexp_of_t,
      () =>
      [TyVarBinding(t, Kind_core.to_rel(k)), ...ctx]
    );

  /* Assumes indices in ty are scoped to new_ctx. */
  let reduce_tyvars = (new_ctx: t, old_ctx: t, ty: HTyp.t): HTyp.t =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("new_ctx", () => Context.sexp_of_t(new_ctx)),
        ("old_ctx", () => Context.sexp_of_t(old_ctx)),
        ("ty", () => HTyp.sexp_of_t(ty)),
      ],
      ~result_sexp=HTyp.sexp_of_t,
      () => {
        let new_tyvars = tyvars(new_ctx);
        let old_tyvars = tyvars(old_ctx);
        let n = List.length(new_tyvars) - List.length(old_tyvars);
        let tyvars =
          ListUtil.take(new_tyvars, n)
          |> List.map(((cref: ContextRef.t, _, k)) => {
               let cref = rescope(new_ctx, cref);
               let ty = HTyp.rescope(new_ctx, Kind.to_htyp(k));
               (cref, ty);
             });
        HTyp.subst_tyvars(new_ctx, ty, tyvars);
      },
    );

  /* Expression Variables */

  let vars = (ctx: t): list((ContextRef.t, Var.t, HTyp.t)) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[("ctx", () => sexp_of_t(ctx))],
      ~result_sexp=
        Sexplib.Std.sexp_of_list(((cref, x, ty)) =>
          List([
            ContextRef.sexp_of_t(cref),
            Var.sexp_of_t(x),
            HTyp.sexp_of_t(ty),
          ])
        ),
      () => {
        let stamp = length(ctx);
        ctx
        |> List.mapi((i, binding) => (i, binding))
        |> List.fold_left(
             (((predecessors, successors), vars), (i, binding)) =>
               switch (binding) {
               | VarBinding(x, ty) =>
                 let index = Index.Abs.of_int(i);
                 let ty = HTyp.of_syntax(HTyp_syntax.to_abs(~offset=i, ty));
                 let predecessors = ListUtil.drop(1, predecessors);
                 let cref =
                   ContextRef.{index, stamp, predecessors, successors};
                 let successors = successors @ [x];
                 ((predecessors, successors), [(cref, x, ty), ...vars]);
               | TyVarBinding(t, _) =>
                 let predecessors = ListUtil.drop(1, predecessors);
                 let successors = successors @ [t];
                 ((predecessors, successors), vars);
               },
             ((List.map(binding_name, ctx), []), []),
           )
        |> snd
        /* TODO: (eric) redo the right way around */
        |> List.rev;
      },
    );

  let var = (ctx: t, cref: ContextRef.t): option(Var.t) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => sexp_of_t(ctx)),
        ("cref", () => ContextRef.sexp_of_t(cref)),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_option(Var.sexp_of_t),
      () => {
        open OptUtil.Syntax;
        let cref = rescope(ctx, cref);
        let+ (x, _) = nth_var_binding(ctx, Index.Abs.to_int(cref.index));
        x;
      },
    );

  let var_ref = (ctx: t, x: Var.t): option(ContextRef.t) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => sexp_of_t(ctx)),
        ("x", () => Var.sexp_of_t(x)),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_option(ContextRef.sexp_of_t),
      () => {
        open OptUtil.Syntax;
        let+ (i, _, _) = first_var_binding(ctx, (x', _) => Var.eq(x, x'));
        let index = Index.Abs.of_int(i);
        let stamp = Context.length(ctx);
        let (successors, _, predecessors) =
          ctx |> List.map(binding_name) |> ListUtil.pivot(i);
        ContextRef.{index, stamp, predecessors, successors};
      },
    );

  let var_ref_type = (ctx: t, cref: ContextRef.t): option(HTyp.t) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => sexp_of_t(ctx)),
        ("cref", () => ContextRef.sexp_of_t(cref)),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_option(HTyp.sexp_of_t),
      () => {
        open OptUtil.Syntax;
        let cref = rescope(ctx, cref);
        let i = Index.Abs.to_int(cref.index);
        let+ (_, ty) = nth_var_binding(ctx, i);
        let ty = HTyp.of_syntax(HTyp_syntax.to_abs(~offset=i, ty));
        HTyp.rescope(ctx, ty);
      },
    );

  let var_type = (ctx: t, x: Var.t): option(HTyp.t) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => sexp_of_t(ctx)),
        ("x", () => Var.sexp_of_t(x)),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_option(HTyp.sexp_of_t),
      () => {
        open OptUtil.Syntax;
        let* cref = var_ref(ctx, x);
        var_ref_type(ctx, cref);
      },
    );

  let add_var = (ctx: t, x: Var.t, ty: HTyp.t): t =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => sexp_of_t(ctx)),
        ("x", () => Var.sexp_of_t(x)),
      ],
      ~result_sexp=sexp_of_t,
      () =>
      [VarBinding(x, HTyp_syntax.to_rel(HTyp.to_syntax(ty))), ...ctx]
    );

  let entries = (ctx: t): list(entry) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[("ctx", () => sexp_of_t(ctx))],
      ~result_sexp=Sexplib.Std.sexp_of_list(sexp_of_entry),
      () =>
      List.mapi(
        (i, binding) =>
          switch (i, binding) {
          | (i, VarBinding(x, ty)) =>
            VarEntry(x, HTyp.of_syntax(HTyp_syntax.to_abs(~offset=i, ty)))
          | (i, TyVarBinding(t, k)) =>
            TyVarEntry(t, Kind_core.to_abs(~offset=i, k))
          },
        ctx,
      )
    );

  let of_entries = (entries: list(entry)): t =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("entries", () => Sexplib.Std.sexp_of_list(sexp_of_entry, entries)),
      ],
      ~result_sexp=sexp_of_t,
      () =>
      List.fold_right(
        (entry, ctx) =>
          switch (entry) {
          | VarEntry(x, ty) => add_var(ctx, x, ty)
          | TyVarEntry(t, k) => add_tyvar(ctx, t, k)
          },
        entries,
        [],
      )
    );
}

and HTyp: {
  [@deriving sexp]
  type t;

  let to_string: t => string;
  let to_syntax: t => HTyp_syntax.t(Index.absolute);
  let of_syntax: HTyp_syntax.t(Index.absolute) => t;

  let shift_indices: (t, int) => t;
  let rescope: (Context.t, t) => t;

  let hole: unit => t;
  let int: unit => t;
  let float: unit => t;
  let bool: unit => t;
  let arrow: (t, t) => t;
  let sum: (t, t) => t;
  let product: list(t) => t;
  let list: t => t;

  let is_hole: t => bool;
  let is_int: t => bool;
  let is_float: t => bool;
  let is_tyvar: t => bool;

  let consistent: (Context.t, t, t) => bool;
  let equivalent: (Context.t, t, t) => bool;
  let complete: t => bool;

  let precedence_Prod: unit => int;
  let precedence_Arrow: unit => int;
  let precedence_Sum: unit => int;
  let precedence: t => int;

  let matched_arrow: (Context.t, t) => option((t, t));
  let matched_sum: (Context.t, t) => option((t, t));
  let matched_list: (Context.t, t) => option(t);

  let tyvar: (Context.t, Index.Abs.t, TyVar.t) => t;
  let tyvarhole: (TyVarErrStatus.HoleReason.t, MetaVar.t, TyVar.t) => t;

  let tyvar_ref: t => option(ContextRef.t);
  let tyvar_name: t => option(TyVar.t);

  let subst_tyvars: (Context.t, t, list((ContextRef.t, t))) => t;

  type join =
    | GLB
    | LUB;

  let join: (Context.t, join, t, t) => option(t);
  let join_all: (Context.t, join, list(t)) => option(t);

  [@deriving sexp]
  type normalized = HTyp_syntax.t(Index.absolute);

  let of_normalized: normalized => t;
  let normalize: (Context.t, t) => normalized;
  let normalized_consistent: (normalized, normalized) => bool;
  let normalized_equivalent: (normalized, normalized) => bool;

  [@deriving sexp]
  type ground_cases =
    | Hole
    | Ground
    | NotGroundOrHole(normalized) /* the argument is the corresponding ground type */;

  let grounded_Arrow: unit => ground_cases;
  let grounded_Sum: unit => ground_cases;
  let grounded_Prod: int => ground_cases;
  let grounded_List: unit => ground_cases;

  let ground_cases_of: normalized => ground_cases;

  [@deriving sexp]
  type head_normalized =
    | Hole
    | Int
    | Float
    | Bool
    | Arrow(t, t)
    | Sum(t, t)
    | Prod(list(t))
    | List(t)
    | TyVar(ContextRef.t, TyVar.t)
    | TyVarHole(TyVarErrStatus.HoleReason.t, MetaVar.t, TyVar.t);

  let of_head_normalized: head_normalized => t;
  let head_normalize: (Context.t, t) => head_normalized;

  let get_prod_elements: head_normalized => list(t);
  let get_prod_arity: head_normalized => int;
} = {
  [@deriving sexp]
  type t = HTyp_syntax.t(Index.absolute);

  let to_string = (ty: t): string => {
    switch (ty) {
    | Hole => "a"
    | Int => "an Integer"
    | Float => "a Float"
    | Bool => "a Boolean"
    | Arrow(_, _) => "a Function"
    | Sum(_, _) => "a Sum"
    | Prod(_) => "a Product"
    | List(_) => "a List"
    | TyVar(_, t) => "a " ++ t
    | TyVarHole(_) => "a"
    };
  };

  let to_syntax: t => HTyp_syntax.t(Index.absolute) = ty => ty;
  let of_syntax: HTyp_syntax.t(Index.absolute) => t = ty => ty;

  let rec shift_indices = (ty: t, amount: int): t =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ty", () => sexp_of_t(ty)),
        ("amount", () => Sexplib.Std.sexp_of_int(amount)),
      ],
      ~result_sexp=sexp_of_t,
      () =>
      switch (ty) {
      | TyVar(cref, t) =>
        let index = Index.shift(~above=-1, ~amount, cref.index);
        let stamp = cref.stamp + amount;
        TyVar({...cref, index, stamp}, t);
      | Hole
      | Int
      | Float
      | Bool
      | TyVarHole(_) => ty
      | Arrow(ty1, ty2) =>
        let ty1 = shift_indices(ty1, amount);
        let ty2 = shift_indices(ty2, amount);
        Arrow(ty1, ty2);
      | Sum(tyL, tyR) =>
        let tyL = shift_indices(tyL, amount);
        let tyR = shift_indices(tyR, amount);
        Sum(tyL, tyR);
      | Prod(tys) => Prod(List.map(ty1 => shift_indices(ty1, amount), tys))
      | List(ty1) => List(shift_indices(ty1, amount))
      }
    );

  let hole: unit => t = () => Hole;
  let int: unit => t = () => Int;
  let float: unit => t = () => Float;
  let bool: unit => t = () => Bool;
  let arrow = (ty1: t, ty2: t): t => Arrow(ty1, ty2);
  let sum = (tyL: t, tyR: t): t => Sum(tyL, tyR);
  let product = (tys: list(t)): t => Prod(tys);
  let list = (ty: t): t => List(ty);

  let is_hole = (ty: t): bool => ty == Hole;
  let is_int = (ty: t): bool => ty == Int;
  let is_float = (ty: t): bool => ty == Float;

  /* Type Variables */

  let is_tyvar = (ty: t): bool =>
    switch (ty) {
    | TyVar(_) => true
    | _ => false
    };

  let tyvar = (ctx: Context.t, index: Index.Abs.t, t: TyVar.t): t =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("index", () => Index.Abs.sexp_of_t(index)),
        ("t", () => TyVar.sexp_of_t(t)),
      ],
      ~result_sexp=sexp_of_t,
      () => {
        let stamp = Context.length(ctx);
        let (successors, _, predecessors) =
          ctx
          |> List.map(Context.binding_name)
          |> ListUtil.pivot(Index.Abs.to_int(index));
        TyVar({index, stamp, successors, predecessors}, t);
      },
    );

  let tyvarhole =
      (reason: TyVarErrStatus.HoleReason.t, u: MetaVar.t, t: TyVar.t): t =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("reason", () => TyVarErrStatus.HoleReason.sexp_of_t(reason)),
        ("u", () => MetaVar.sexp_of_t(u)),
        ("t", () => TyVar.sexp_of_t(t)),
      ],
      ~result_sexp=sexp_of_t,
      () =>
      TyVarHole(reason, u, t)
    );

  let tyvar_ref = (ty: t): option(ContextRef.t) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[("ty", () => sexp_of_t(ty))],
      ~result_sexp=Sexplib.Std.sexp_of_option(ContextRef.sexp_of_t),
      () =>
      switch (ty) {
      | TyVar(cref, _) => Some(cref)
      | TyVarHole(_)
      | Hole
      | Int
      | Float
      | Bool
      | Arrow(_)
      | Sum(_)
      | Prod(_)
      | List(_) => None
      }
    );

  let tyvar_name = (ty: t): option(string) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[("ty", () => sexp_of_t(ty))],
      ~result_sexp=Sexplib.Std.sexp_of_option(Sexplib.Std.sexp_of_string),
      () =>
      switch (ty) {
      | TyVar(_, t) => Some(t)
      | TyVarHole(_)
      | Hole
      | Int
      | Float
      | Bool
      | Arrow(_)
      | Sum(_)
      | Prod(_)
      | List(_) => None
      }
    );

  let rec rescope = (ctx: Context.t, ty: t): t =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("ty", () => sexp_of_t(ty)),
      ],
      ~result_sexp=sexp_of_t,
      () =>
      switch (ty) {
      | TyVar(cref, t) => TyVar(Context.rescope(ctx, cref), t)
      | TyVarHole(_)
      | Hole
      | Int
      | Float
      | Bool => ty
      | Arrow(ty1, ty2) => Arrow(rescope(ctx, ty1), rescope(ctx, ty2))
      | Sum(tyL, tyR) => Sum(rescope(ctx, tyL), rescope(ctx, tyR))
      | Prod(tys) => Prod(List.map(rescope(ctx), tys))
      | List(ty1) => List(rescope(ctx, ty1))
      }
    );

  let subst_tyvar = (ctx: Context.t, ty: t, cref: ContextRef.t, ty': t): t => {
    let cref = Context.rescope(ctx, cref);
    HTyp_syntax.subst_tyvar(ty, cref.index, ty');
  };

  let subst_tyvars =
      (ctx: Context.t, ty: t, tyvars: list((ContextRef.t, t))): t =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ty", () => sexp_of_t(ty)),
        (
          "tyvars",
          () =>
            Sexplib.Std.sexp_of_list(
              ((cref, ty)) =>
                List([ContextRef.sexp_of_t(cref), sexp_of_t(ty)]),
              tyvars,
            ),
        ),
      ],
      ~result_sexp=sexp_of_t,
      () =>
      List.fold_left(
        (ty, (cref, ty')) => subst_tyvar(ctx, ty, cref, ty'),
        ty,
        tyvars,
      )
    );

  let rec equivalent = (ctx: Context.t, ty: t, ty': t): bool =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("ty", () => sexp_of_t(ty)),
        ("ty'", () => sexp_of_t(ty')),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_bool,
      () =>
      switch (ty, ty') {
      | (TyVar(cref, _), TyVar(cref', _)) =>
        Index.equal(cref.index, cref'.index)
        && Int.equal(cref.stamp, cref'.stamp)
        || (
          switch (
            Context.tyvar_kind(ctx, Context.rescope(ctx, cref)),
            Context.tyvar_kind(ctx, Context.rescope(ctx, cref')),
          ) {
          | (Some(k), Some(k')) => Kind.equivalent(ctx, k, k')
          | (None, _)
          | (_, None) => false
          }
        )
      | (TyVar(_), _) => false
      | (TyVarHole(_, u, _), TyVarHole(_, u', _)) => MetaVar.eq(u, u')
      | (TyVarHole(_, _, _), _) => false
      | (Hole | Int | Float | Bool, _) => ty == ty'
      | (Arrow(ty1, ty2), Arrow(ty1', ty2'))
      | (Sum(ty1, ty2), Sum(ty1', ty2')) =>
        equivalent(ctx, ty1, ty1') && equivalent(ctx, ty2, ty2')
      | (Arrow(_, _), _) => false
      | (Sum(_, _), _) => false
      | (Prod(tys1), Prod(tys2)) =>
        List.for_all2(equivalent(ctx), tys1, tys2)
      | (Prod(_), _) => false
      | (List(ty), List(ty')) => equivalent(ctx, ty, ty')
      | (List(_), _) => false
      }
    );

  let rec consistent = (ctx: Context.t, ty: t, ty': t): bool =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("ty", () => sexp_of_t(ty)),
        ("ty'", () => sexp_of_t(ty')),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_bool,
      () =>
      switch (ty, ty') {
      | (TyVar(cref, _), TyVar(cref', _)) =>
        switch (
          Context.tyvar_kind(ctx, Context.rescope(ctx, cref)),
          Context.tyvar_kind(ctx, Context.rescope(ctx, cref')),
        ) {
        | (Some(k), Some(k')) =>
          consistent(
            ctx,
            HTyp.to_syntax(Kind.to_htyp(k)),
            HTyp.to_syntax(Kind.to_htyp(k')),
          )
        | (None, _)
        | (_, None) => false
        }
      | (TyVar(cref, _), ty1)
      | (ty1, TyVar(cref, _)) =>
        switch (Context.tyvar_kind(ctx, Context.rescope(ctx, cref))) {
        | Some(k) => consistent(ctx, HTyp.to_syntax(Kind.to_htyp(k)), ty1)
        | None => false
        }
      | (TyVarHole(_) | Hole, _)
      | (_, TyVarHole(_) | Hole) => true
      | (Int | Float | Bool, _) => ty == ty'
      | (Arrow(ty1, ty2), Arrow(ty1', ty2'))
      | (Sum(ty1, ty2), Sum(ty1', ty2')) =>
        consistent(ctx, ty1, ty1') && consistent(ctx, ty2, ty2')
      | (Arrow(_) | Sum(_), _) => false
      | (Prod(tys), Prod(tys')) =>
        List.for_all2(consistent(ctx), tys, tys')
      | (Prod(_), _) => false
      | (List(ty1), List(ty1')) => consistent(ctx, ty1, ty1')
      | (List(_), _) => false
      }
    );

  let inconsistent = (ctx: Context.t, ty1: t, ty2: t): bool =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("ty1", () => sexp_of_t(ty1)),
        ("ty2", () => sexp_of_t(ty2)),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_bool,
      () =>
      !consistent(ctx, ty1, ty2)
    );

  let rec consistent_all = (ctx: Context.t, types: list(t)): bool =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("types", () => Sexplib.Std.sexp_of_list(sexp_of_t, types)),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_bool,
      () =>
      switch (types) {
      | [] => true
      | [hd, ...tl] =>
        !List.exists(inconsistent(ctx, hd), tl) || consistent_all(ctx, tl)
      }
    );

  /* complete (i.e. does not have any holes) */
  let rec complete: t => bool =
    fun
    | Hole
    | TyVarHole(_) => false
    | TyVar(_)
    | Int
    | Float
    | Bool => true
    | Arrow(ty1, ty2)
    | Sum(ty1, ty2) => complete(ty1) && complete(ty2)
    | Prod(tys) => tys |> List.for_all(complete)
    | List(ty) => complete(ty);

  /* HTyp Constructor Precedence */

  let precedence_Prod = () => Operators_Typ.precedence(Prod);
  let precedence_Arrow = () => Operators_Typ.precedence(Arrow);
  let precedence_Sum = () => Operators_Typ.precedence(Sum);
  let precedence_const = () => Operators_Typ.precedence_const;
  let precedence = (ty: t): int =>
    switch (ty) {
    | Int
    | Float
    | Bool
    | Hole
    | Prod([])
    | List(_)
    | TyVar(_)
    | TyVarHole(_) => precedence_const()
    | Prod(_) => precedence_Prod()
    | Sum(_, _) => precedence_Sum()
    | Arrow(_, _) => precedence_Arrow()
    };

  /* Joins */

  [@deriving sexp]
  type join =
    | GLB
    | LUB;

  let rec join = (ctx: Context.t, j: join, ty1: t, ty2: t): option(t) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("j", () => sexp_of_join(j)),
        ("ty1", () => sexp_of_t(ty1)),
        ("ty2", () => sexp_of_t(ty2)),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_option(sexp_of_t),
      () =>
      switch (ty1, ty2) {
      | (TyVarHole(_), TyVarHole(_)) => Some(Hole)
      | (ty, Hole | TyVarHole(_))
      | (Hole | TyVarHole(_), ty) =>
        switch (j) {
        | GLB => Some(Hole)
        | LUB => Some(ty)
        }
      | (TyVar(cref, _), _) =>
        open OptUtil.Syntax;
        let* k = Context.tyvar_kind(ctx, cref);
        switch (k) {
        | S(ty) => join(ctx, j, ty, ty2)
        | Hole => join(ctx, j, Hole, ty2)
        | Type =>
          failwith("impossible for bounded type variables (currently) 1")
        };
      | (_, TyVar(cref, _)) =>
        open OptUtil.Syntax;
        let* k = Context.tyvar_kind(ctx, cref);
        switch (k) {
        | S(ty) => join(ctx, j, ty1, ty)
        | Hole => join(ctx, j, ty1, Hole)
        | Type =>
          failwith("impossible for bounded type variables (currently) 2")
        };
      | (Int | Float | Bool, _) => ty1 == ty2 ? Some(ty1) : None
      | (Arrow(ty1, ty2), Arrow(ty1', ty2')) =>
        open OptUtil.Syntax;
        let* ty1 = join(ctx, j, ty1, ty1');
        let+ ty2 = join(ctx, j, ty2, ty2');
        HTyp_syntax.Arrow(ty1, ty2);
      | (Sum(ty1, ty2), Sum(ty1', ty2')) =>
        open OptUtil.Syntax;
        let* ty1 = join(ctx, j, ty1, ty1');
        let+ ty2 = join(ctx, j, ty2, ty2');
        HTyp_syntax.Sum(ty1, ty2);
      | (Prod(tys1), Prod(tys2)) =>
        open OptUtil.Syntax;
        let+ joined_tys =
          List.map2(join(ctx, j), tys1, tys2) |> OptUtil.sequence;
        HTyp_syntax.Prod(joined_tys);
      | (List(ty), List(ty')) =>
        open OptUtil.Syntax;
        let+ ty = join(ctx, j, ty, ty');
        HTyp_syntax.List(ty);
      | (Arrow(_) | Sum(_) | Prod(_) | List(_), _) => None
      }
    );

  let join_all = (ctx: Context.t, j: join, types: list(t)): option(t) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("j", () => sexp_of_join(j)),
        ("types", () => Sexplib.Std.sexp_of_list(sexp_of_t, types)),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_option(sexp_of_t),
      () =>
      switch (types) {
      | [] => None
      | [hd] => Some(hd)
      | [hd, ...tl] =>
        !consistent_all(ctx, types)
          ? None
          : List.fold_left(
              (common_opt, ty) => {
                open OptUtil.Syntax;
                let* common_ty = common_opt;
                join(ctx, j, common_ty, ty);
              },
              Some(hd),
              tl,
            )
      }
    );

  /* HTyp Normalization */

  [@deriving sexp]
  type normalized = HTyp_syntax.t(Index.absolute);

  let of_normalized: normalized => t = t => t;

  /* Replaces every singleton-kinded type variable with a normalized type. */
  let rec normalize = (ctx: Context.t, ty: t): normalized =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("ty", () => sexp_of_t(ty)),
      ],
      ~result_sexp=sexp_of_normalized,
      () =>
      switch (ty) {
      | TyVar(cref, _) =>
        switch (Context.tyvar_kind(ctx, cref)) {
        | Some(S(ty1)) => normalize(ctx, ty1)
        | Some(_) => ty
        | None =>
          failwith(
            __LOC__
            ++ ": unknown type variable index "
            ++ Index.to_string(cref.index)
            ++ " stamped "
            ++ Int.to_string(cref.stamp),
          )
        }
      | TyVarHole(_)
      | Hole
      | Int
      | Float
      | Bool => ty
      | Arrow(ty1, ty2) => Arrow(normalize(ctx, ty1), normalize(ctx, ty2))
      | Sum(ty1, ty2) => Sum(normalize(ctx, ty1), normalize(ctx, ty2))
      | Prod(tys) => Prod(List.map(normalize(ctx), tys))
      | List(ty1) => List(normalize(ctx, ty1))
      }
    );

  /* Properties of Normalized HTyp */

  let rec normalized_consistent = (ty: normalized, ty': normalized): bool =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ty", () => sexp_of_normalized(ty)),
        ("ty'", () => sexp_of_normalized(ty')),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_bool,
      () =>
      switch (ty, ty') {
      | (TyVar(cref, _), TyVar(cref', _)) =>
        // normalization eliminates all type variables of singleton kind, so these
        // must be of kind Type or Hole
        ContextRef.equal(cref, cref')
      | (TyVar(_) | TyVarHole(_) | Hole, _)
      | (_, TyVar(_) | TyVarHole(_) | Hole) => true
      | (Int | Float | Bool, _) => ty == ty'
      | (Arrow(ty1, ty2), Arrow(ty1', ty2'))
      | (Sum(ty1, ty2), Sum(ty1', ty2')) =>
        normalized_consistent(ty1, ty1') && normalized_consistent(ty2, ty2')
      | (Arrow(_) | Sum(_), _) => false
      | (Prod(tys), Prod(tys')) =>
        List.for_all2(normalized_consistent, tys, tys')
      | (Prod(_), _) => false
      | (List(ty1), List(ty1')) => normalized_consistent(ty1, ty1')
      | (List(_), _) => false
      }
    );

  let rec normalized_equivalent = (ty: normalized, ty': normalized): bool =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ty", () => sexp_of_normalized(ty)),
        ("ty'", () => sexp_of_normalized(ty')),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_bool,
      () =>
      switch (ty, ty') {
      | (TyVar(cref, _), TyVar(cref', _)) => ContextRef.equal(cref, cref')
      | (TyVar(_), _) => false
      | (TyVarHole(_, u, _), TyVarHole(_, u', _)) => MetaVar.eq(u, u')
      | (TyVarHole(_, _, _), _) => false
      | (Hole | Int | Float | Bool, _) => ty == ty'
      | (Arrow(ty1, ty2), Arrow(ty1', ty2'))
      | (Sum(ty1, ty2), Sum(ty1', ty2')) =>
        normalized_equivalent(ty1, ty1') && normalized_equivalent(ty2, ty2')
      | (Arrow(_, _), _) => false
      | (Sum(_, _), _) => false
      | (Prod(tys1), Prod(tys2)) =>
        List.for_all2(normalized_equivalent, tys1, tys2)
      | (Prod(_), _) => false
      | (List(ty), List(ty')) => normalized_equivalent(ty, ty')
      | (List(_), _) => false
      }
    );

  /* Ground Cases */

  [@deriving sexp]
  type ground_cases =
    | Hole
    | Ground
    | NotGroundOrHole(t) /* the argument is the corresponding ground type */;

  let grounded_Arrow = () => NotGroundOrHole(Arrow(Hole, Hole));
  let grounded_Sum = () => NotGroundOrHole(Sum(Hole, Hole));
  let grounded_Prod = length =>
    NotGroundOrHole(Prod(ListUtil.replicate(length, HTyp_syntax.Hole)));
  let grounded_List = () => NotGroundOrHole(List(Hole));

  let ground_cases_of = (ty: normalized): ground_cases =>
    switch (ty) {
    | Hole
    | TyVarHole(_) => Hole
    | Bool
    | Int
    | Float
    | Arrow(Hole, Hole)
    | Sum(Hole, Hole)
    | List(Hole)
    | TyVar(_) => Ground
    | Prod(tys) =>
      let equiv = ty => normalized_equivalent(Hole, ty);
      List.for_all(equiv, tys) ? Ground : tys |> List.length |> grounded_Prod;
    | Arrow(_, _) => grounded_Arrow()
    | Sum(_, _) => grounded_Sum()
    | List(_) => grounded_List()
    };

  /* HTyp Head-Normalization */

  [@deriving sexp]
  type head_normalized =
    | Hole
    | Int
    | Float
    | Bool
    | Arrow(t, t)
    | Sum(t, t)
    | Prod(list(t))
    | List(t)
    | TyVar(ContextRef.t, TyVar.t)
    | TyVarHole(TyVarErrStatus.HoleReason.t, MetaVar.t, TyVar.t);

  let of_head_normalized: head_normalized => t =
    fun
    | Hole => Hole
    | Int => Int
    | Float => Float
    | Bool => Bool
    | Arrow(ty1, ty2) => Arrow(ty1, ty2)
    | Sum(tyL, tyR) => Sum(tyL, tyR)
    | Prod(tys) => Prod(tys)
    | List(ty) => List(ty)
    | TyVar(cref, t) => TyVar(cref, t)
    | TyVarHole(reason, u, name) => TyVarHole(reason, u, name);

  /* Replaces a singleton-kinded type variable with a head-normalized type. */
  let rec head_normalize = (ctx: Context.t, ty: t): head_normalized =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("ty", () => sexp_of_normalized(ty)),
      ],
      ~result_sexp=sexp_of_head_normalized,
      () => {
      Log.fun_call(
        __FUNCTION__,
        ~args=[
          ("ctx", () => Context.sexp_of_t(ctx)),
          ("ty", () => sexp_of_t(ty)),
        ],
        ~result_sexp=sexp_of_head_normalized,
        () =>
        switch (ty) {
        | TyVar(cref, t) =>
          switch (Context.tyvar_kind(ctx, cref)) {
          | Some(S(ty1)) => head_normalize(ctx, ty1)
          | Some(_) => TyVar(cref, t)
          | None =>
            failwith(
              __LOC__
              ++ ": unknown type variable index "
              ++ Index.to_string(cref.index)
              ++ " stamp "
              ++ Int.to_string(cref.stamp),
            )
          }
        | TyVarHole(reason, u, t) => TyVarHole(reason, u, t)
        | Hole => Hole
        | Int => Int
        | Float => Float
        | Bool => Bool
        | Arrow(ty1, ty2) => Arrow(ty1, ty2)
        | Sum(tyL, tyR) => Sum(tyL, tyR)
        | Prod(tys) => Prod(tys)
        | List(ty) => List(ty)
        }
      )
    });

  /* Matched Type Constructors */

  let matched_arrow = (ctx: Context.t, ty: t): option((t, t)) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("ty", () => sexp_of_normalized(ty)),
      ],
      ~result_sexp=
        Sexplib.Std.sexp_of_option(((ty, ty')) =>
          List([sexp_of_t(ty), sexp_of_t(ty')])
        ),
      () =>
      switch (head_normalize(ctx, ty)) {
      | Hole
      | TyVarHole(_)
      | TyVar(_) => Some((Hole, Hole))
      | Arrow(ty1, ty2) => Some((ty1, ty2))
      | _ => None
      }
    );

  let matched_sum = (ctx: Context.t, ty: t): option((t, t)) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("ty", () => sexp_of_normalized(ty)),
      ],
      ~result_sexp=
        Sexplib.Std.sexp_of_option(((ty, ty')) =>
          List([sexp_of_t(ty), sexp_of_t(ty')])
        ),
      () =>
      switch (head_normalize(ctx, ty)) {
      | Hole
      | TyVarHole(_)
      | TyVar(_) => Some((Hole, Hole))
      | Sum(tyL, tyR) => Some((tyL, tyR))
      | _ => None
      }
    );

  let matched_list = (ctx: Context.t, ty: t): option(t) =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("ty", () => sexp_of_normalized(ty)),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_option(sexp_of_t),
      () =>
      switch (head_normalize(ctx, ty)) {
      | Hole
      | TyVarHole(_)
      | TyVar(_) => Some(Hole)
      | List(ty) => Some(ty)
      | _ => None
      }
    );

  /* Product Types */

  let get_prod_elements: head_normalized => list(t) =
    fun
    | Prod(tys) => tys
    | _ as ty => [of_head_normalized(ty)];

  let get_prod_arity = ty => ty |> get_prod_elements |> List.length;
}

and Kind: {
  [@deriving sexp]
  type t = Kind_core.s(Index.absolute);
  let rescope: (Context.t, t) => t;
  let to_htyp: t => HTyp.t;
  let singleton: HTyp.t => t;
  let consistent_subkind: (Context.t, t, t) => bool;
  let equivalent: (Context.t, t, t) => bool;
} = {
  open Kind_core;

  [@deriving sexp]
  type t = s(Index.absolute);

  let rescope = (ctx: Context.t, k: t): t =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("k", () => sexp_of_t(k)),
      ],
      ~result_sexp=sexp_of_t,
      () =>
      switch (k) {
      | Hole
      | Type => k
      | S(ty) => S(HTyp.to_syntax(HTyp.rescope(ctx, HTyp.of_syntax(ty))))
      }
    );

  /* For converting type variables to equivalent [HTyp]s while resolving local
     type aliases. */
  let to_htyp: t => HTyp.t =
    fun
    | Hole
    | Type => HTyp.hole()
    | S(ty) => HTyp.of_syntax(ty);

  let singleton = (ty: HTyp_syntax.t(Index.absolute)): t => S(ty);

  let consistent_subkind = (ctx: Context.t, k: t, k': t): bool =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("k", () => Kind.sexp_of_t(k)),
        ("k'", () => Kind.sexp_of_t(k')),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_bool,
      () =>
      switch (k, k') {
      | (Hole, _)
      | (_, Hole) => true
      | (S(_), Type) => true
      | (S(ty), S(ty')) =>
        HTyp.consistent(ctx, HTyp.of_syntax(ty), HTyp.of_syntax(ty'))
      | (Type, S(_)) => false
      | (Type, Type) => true
      }
    );

  let equivalent = (ctx: Context.t, k: t, k': t): bool =>
    Log.fun_call(
      __FUNCTION__,
      ~args=[
        ("ctx", () => Context.sexp_of_t(ctx)),
        ("k", () => sexp_of_t(k)),
        ("k'", () => sexp_of_t(k')),
      ],
      ~result_sexp=Sexplib.Std.sexp_of_bool,
      () =>
      switch (k, k') {
      | (Hole | Type, Hole | Type) => true
      | (Hole | Type, _) => false
      | (S(ty1), S(ty1')) =>
        HTyp.equivalent(ctx, HTyp.of_syntax(ty1), HTyp.of_syntax(ty1'))
      | (S(_), _) => false
      }
    );
};
