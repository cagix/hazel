open Util;
open Term;
open OptUtil.Syntax;

/* STATICS

     This module determines the statics semantics of the language.
     It takes a term and returns a map which associates the unique
     ids of each term to an Info.t data structure which reflects that
     term's statics. The statics collected depend on the term's sort,
     but every term has a syntactic class (The cls types from Term),
     except Invalid terms which Term could not parse.

     The map generated by this module is intended to be generated once
     from a given term and then reused anywhere there is logic which
     depends on static information.
   */

[@deriving (show({with_path: false}), sexp, yojson)]
type map = Id.Map.t(Info.t);

let exp_id = Term.UExp.rep_id;
let pat_id = Term.UPat.rep_id;
let typ_id = Term.UTyp.rep_id;
let utpat_id = Term.UTPat.rep_id;

/* The type of an expression after hole wrapping */
let fixed_exp_typ = (ctx, m: map, e: Term.UExp.t): option(Typ.t) => {
  let* info = Id.Map.find_opt(exp_id(e), m);
  Info.typ_after_fix_opt(ctx, info);
};

/* The type of a pattern after hole wrapping */
let fixed_pat_typ = (ctx, m: map, e: Term.UPat.t): option(Typ.t) => {
  let* info = Id.Map.find_opt(pat_id(e), m);
  Info.typ_after_fix_opt(ctx, info);
};

let pat_self_typ = (ctx, m: map, p: Term.UPat.t): option(Typ.t) => {
  let* info = Id.Map.find_opt(pat_id(p), m);
  Info.typ_of_self_info(ctx, info);
};

let union_m = List.fold_left(Id.Map.disj_union, Id.Map.empty);

let add_info = (ids: list(Id.t), info: Info.t, m: map): map =>
  ids
  |> List.map(id => Id.Map.singleton(id, info))
  |> List.fold_left(Id.Map.disj_union, m);

let extend_let_def_ctx =
    (ctx: Ctx.t, pat: Term.UPat.t, pat_ctx: Ctx.t, def: Term.UExp.t): Ctx.t =>
  if (Term.UPat.is_tuple_of_arrows(pat)
      && Term.UExp.is_tuple_of_functions(def)) {
    pat_ctx;
  } else {
    ctx;
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

let rec any_to_info_map =
        (~ctx: Ctx.t, ~ancestors, any: Term.any): (Ctx.co, map) =>
  switch (any) {
  | Exp(e) =>
    let (Info.{free, _}, m) = uexp_to_info_map(~ctx, ~ancestors, e);
    (free, m);
  | Pat(p) =>
    let m = upat_to_info_map(~is_synswitch=false, ~ancestors, ~ctx, p) |> snd;
    (VarMap.empty, m);
  | TPat(tp) => (
      VarMap.empty,
      utpat_to_info_map(~ctx, ~ancestors, tp) |> snd,
    )
  | Typ(ty) => (VarMap.empty, utyp_to_info_map(~ctx, ~ancestors, ty) |> snd)
  | Rul(_)
  | Nul ()
  | Any () => (VarMap.empty, Id.Map.empty)
  }
and uexp_to_info_map =
    (
      ~ctx: Ctx.t,
      ~mode=Typ.Syn,
      ~ancestors,
      {ids, term} as uexp: Term.UExp.t,
    )
    : (Info.exp, map) => {
  /* Maybe switch mode to syn */
  let mode =
    switch (mode) {
    | Ana(Unknown(SynSwitch)) => Typ.Syn
    | _ => mode
    };
  let cls = Term.UExp.cls_of_term(term);
  let add = (~self, ~free, m) => {
    let ty = Info.typ_after_fix(ctx, mode, self);
    let info = Info.{cls, self, ty, mode, ctx, free, ancestors, term: uexp};
    (info, add_info(ids, InfoExp(info), m));
  };
  let ancestors = [exp_id(uexp)] @ ancestors;
  let go' = uexp_to_info_map(~ancestors);
  let go = go'(~ctx);
  let go_pat = upat_to_info_map(~ctx, ~ancestors);
  let atomic = self => add(~self, ~free=[], Id.Map.empty);
  switch (term) {
  | Invalid(token) => atomic(BadToken(token))
  | MultiHole(tms) =>
    let (frees, ms) =
      tms |> List.map(any_to_info_map(~ctx, ~ancestors)) |> List.split;
    add(~self=IsMulti, ~free=Ctx.union(frees), union_m(ms));
  | EmptyHole => atomic(Just(Unknown(Internal)))
  | Triv => atomic(Just(Prod([])))
  | Bool(_) => atomic(Just(Bool))
  | Int(_) => atomic(Just(Int))
  | Float(_) => atomic(Just(Float))
  | String(_) => atomic(Just(String))
  | ListLit([]) => atomic(Just(List(Unknown(Internal))))
  | ListLit(es) =>
    let modes = List.init(List.length(es), _ => Typ.matched_list_mode(mode));
    let (infos, ms) =
      List.map2((e, mode) => go(~mode, e), es, modes) |> List.split;
    let tys = List.map(Info.exp_ty, infos);
    let frees = List.map(Info.exp_free, infos);
    let self: Info.self =
      switch (Typ.join_all(ctx, tys)) {
      | None =>
        NoJoin(List.map2((e, ty) => Info.{id: exp_id(e), ty}, es, tys))
      | Some(ty) => Just(List(ty))
      };
    add(~self, ~free=Ctx.union(frees), union_m(ms));
  | Cons(e1, e2) =>
    let mode_e1 = Typ.matched_list_mode(mode);
    let (Info.{free: free1, ty: ty1, _}, m1) = go(~mode=mode_e1, e1);
    let (Info.{free: free2, _}, m2) = go(~mode=Ana(List(ty1)), e2);
    add(
      ~self=Just(List(ty1)),
      ~free=Ctx.union([free1, free2]),
      union_m([m1, m2]),
    );
  | Var(name) =>
    let self: Info.self =
      switch (Ctx.lookup_var(ctx, name)) {
      | None => FreeVar
      | Some(var) => Just(var.typ)
      };
    add(~self, ~free=[(name, [{id: exp_id(uexp), mode}])], Id.Map.empty);
  | Parens(e) =>
    let (Info.{free, ty, _}, m) = go(~mode, e);
    add(~self=Just(ty), ~free, m);
  | UnOp(op, e) =>
    let (ty_in, ty_out) = typ_exp_unop(op);
    let (Info.{free, _}, m) = go(~mode=Ana(ty_in), e);
    add(~self=Just(ty_out), ~free, m);
  | BinOp(op, e1, e2) =>
    let (ty1, ty2, ty_out) = typ_exp_binop(op);
    let (Info.{free: free1, _}, m1) = go(~mode=Ana(ty1), e1);
    let (Info.{free: free2, _}, m2) = go(~mode=Ana(ty2), e2);
    add(
      ~self=Just(ty_out),
      ~free=Ctx.union([free1, free2]),
      union_m([m1, m2]),
    );
  | Tuple(es) =>
    let modes = Typ.matched_prod_mode(mode, List.length(es));
    let (infos, ms) =
      List.map2((e, mode) => go(~mode, e), es, modes) |> List.split;
    let frees = List.map(Info.exp_free, infos);
    let self = Info.Just(Prod(List.map(Info.exp_ty, infos)));
    add(~self, ~free=Ctx.union(frees), union_m(ms));
  | Test(test) =>
    let (Info.{free, _}, m1) = go(~mode=Ana(Bool), test);
    add(~self=Just(Prod([])), ~free, m1);
  | If(e0, e1, e2) =>
    let (Info.{free: free0, _}, m0) = go(~mode=Ana(Bool), e0);
    let (Info.{free: free1, ty: ty1, _}, m1) = go(~mode, e1);
    let (Info.{free: free2, ty: ty2, _}, m2) = go(~mode, e2);
    let self: Info.self =
      switch (Typ.join(ctx, ty1, ty2)) {
      | None =>
        NoJoin([{id: exp_id(e1), ty: ty1}, {id: exp_id(e2), ty: ty2}])
      | Some(ty) => Just(ty)
      };
    add(
      ~self,
      ~free=Ctx.union([free0, free1, free2]),
      union_m([m0, m1, m2]),
    );
  | Seq(e1, e2) =>
    let (Info.{free: free1, _}, m1) = go(~mode=Syn, e1);
    let (Info.{free: free2, ty: ty2, _}, m2) = go(~mode, e2);
    add(
      ~self=Just(ty2),
      ~free=Ctx.union([free1, free2]),
      union_m([m1, m2]),
    );
  | Tag(tag) => atomic(IsTag(tag, Info.syn_tag_typ(ctx, tag)))
  | Ap(fn, arg) =>
    let fn_mode =
      switch (fn) {
      | {term: Tag(name), _} => Typ.tag_ap_mode(ctx, mode, name)
      | _ => Typ.ap_mode
      };
    let (Info.{free: free_fn, ty: ty_fn, _}, m_fn) = go(~mode=fn_mode, fn);
    let (ty_in, ty_out) = Typ.matched_arrow(ty_fn);
    let (Info.{free: free_arg, _}, m_arg) = go(~mode=Ana(ty_in), arg);
    add(
      ~self=Just(ty_out),
      ~free=Ctx.union([free_fn, free_arg]),
      union_m([m_fn, m_arg]),
    );
  | Fun(pat, body) =>
    let (mode_pat, mode_body) = Typ.matched_arrow_mode(mode);
    let ({ty: ty_pat, ctx: ctx_pat, _}: Info.pat, m_pat) =
      go_pat(~is_synswitch=false, ~mode=mode_pat, pat);
    let (Info.{free: free_body, ty: ty_body, _}, m_body) =
      go'(~ctx=ctx_pat, ~mode=mode_body, body);
    add(
      ~self=Just(Arrow(ty_pat, ty_body)),
      ~free=Ctx.free_in(ctx, ctx_pat, free_body),
      union_m([m_pat, m_body]),
    );
  | Let(pat, def, body) =>
    let ({ty: ty_pat, ctx: ctx_pat, _}: Info.pat, _) =
      go_pat(~is_synswitch=true, ~mode=Syn, pat);
    let def_ctx = extend_let_def_ctx(ctx, pat, ctx_pat, def);
    let (Info.{free: free_def, ty: ty_def, _}, m_def) =
      go'(~ctx=def_ctx, ~mode=Ana(ty_pat), def);
    /* Analyze pattern to incorporate def type into ctx */
    let ({ctx: ctx_pat_ana, _}: Info.pat, m_pat) =
      go_pat(~is_synswitch=false, ~mode=Ana(ty_def), pat);
    let (Info.{free: free_body, ty: ty_body, _}, m_body) =
      go'(~ctx=ctx_pat_ana, ~mode, body);
    add(
      ~self=Just(ty_body),
      ~free=Ctx.union([free_def, Ctx.free_in(ctx, ctx_pat_ana, free_body)]),
      union_m([m_pat, m_def, m_body]),
    );
  | TyAlias(typat, utyp, body) =>
    let m_typat = utpat_to_info_map(~ctx, ~ancestors, typat) |> snd;
    switch (typat.term) {
    | Var(name) =>
      /* NOTE(andrew): This is a slightly dicey piece of logic, debatably
         errors cancelling out. Right now, to_typ returns Unknown(TypeHole)
         for any type variable reference not in its ctx. So any free variables
         in the definition would be oblierated. But we need to check for free
         variables to decide whether to make a recursive type or not. So we
         tentatively add an abtract type to the ctx, representing the
         speculative rec parameter. */
      let (ty_def, ctx_def, ctx_body) = {
        let ty_pre = Term.UTyp.to_typ(Ctx.add_abstract(ctx, name, -1), utyp);
        switch (utyp.term) {
        | USum(_) when List.mem(name, Typ.free_vars(ty_pre)) =>
          let ty_rec = Typ.Rec(name, ty_pre);
          let ctx_def = Ctx.add_alias(ctx, name, utpat_id(typat), ty_rec);
          (ty_rec, ctx_def, ctx_def);
        | _ =>
          let ty = Term.UTyp.to_typ(ctx, utyp);
          (ty, ctx, Ctx.add_alias(ctx, name, utpat_id(typat), ty));
        };
      };
      let ctx_body =
        switch (ty_def) {
        | Sum(sm)
        | Rec(_, Sum(sm)) => Ctx.add_tags(ctx_body, name, typ_id(utyp), sm)
        | _ => ctx_body
        };
      let (Info.{free, ty: ty_body, _}, m_body) =
        uexp_to_info_map(~ctx=ctx_body, ~ancestors, ~mode, body);
      let ty_escape = Typ.subst(ty_def, name, ty_body);
      let m_typ = utyp_to_info_map(~ctx=ctx_def, ~ancestors, utyp) |> snd;
      add(~self=Just(ty_escape), ~free, union_m([m_typat, m_body, m_typ]));
    | _ =>
      let (Info.{free, ty: ty_body, _}, m_body) =
        uexp_to_info_map(~ctx, ~ancestors, ~mode, body);
      let m_typ = utyp_to_info_map(~ctx, ~ancestors, utyp) |> snd;
      add(~self=Just(ty_body), ~free, union_m([m_typat, m_body, m_typ]));
    };
  | Match(scrut, rules) =>
    let (Info.{free: free_scrut, ty: ty_scrut, _}, m_scrut) =
      go(~mode=Syn, scrut);
    let (ps, es) = List.split(rules);
    let (p_infos, pat_ms) =
      List.map(go_pat(~is_synswitch=false, ~mode=Typ.Ana(ty_scrut)), ps)
      |> List.split;
    let p_ctxs = List.map(Info.pat_ctx, p_infos);
    let (e_infos, e_ms) =
      List.map2((e, ctx) => go'(~ctx, ~mode, e), es, p_ctxs) |> List.split;
    let e_tys = List.map(Info.exp_ty, e_infos);
    let e_frees =
      List.map2(Ctx.free_in(ctx), p_ctxs, List.map(Info.exp_free, e_infos));
    let self: Info.self =
      switch (Typ.join_all(ctx, e_tys)) {
      | None =>
        NoJoin(List.map2((e, ty) => Info.{id: exp_id(e), ty}, es, e_tys))
      | Some(ty) => Just(ty)
      };
    add(
      ~self,
      ~free=Ctx.union([free_scrut] @ e_frees),
      union_m([m_scrut] @ pat_ms @ e_ms),
    );
  };
}
and upat_to_info_map =
    (
      ~is_synswitch,
      ~ctx,
      ~ancestors: Info.ancestors,
      ~mode: Typ.mode=Typ.Syn,
      {ids, term} as upat: Term.UPat.t,
    )
    : (Info.pat, map) => {
  let cls = Term.UPat.cls_of_term(term);
  let add = (~self, ~ctx, m) => {
    let ty = Info.typ_after_fix(ctx, mode, self);
    let info = Info.{cls, self, mode, ty, ctx, ancestors, term: upat};
    (info, add_info(ids, InfoPat(info), m));
  };
  let atomic = self => add(~self, ~ctx, Id.Map.empty);
  let ancestors = [pat_id(upat)] @ ancestors;
  let go = upat_to_info_map(~is_synswitch, ~ancestors);
  let unknown = Typ.Unknown(is_synswitch ? SynSwitch : Internal);
  switch (term) {
  | Invalid(token) => atomic(BadToken(token))
  | MultiHole(tms) =>
    let (_, maps) =
      tms |> List.map(any_to_info_map(~ctx, ~ancestors)) |> List.split;
    add(~self=IsMulti, ~ctx, union_m(maps));
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
    let (ctx, rets) =
      List.fold_left2(
        ((ctx, infos), e, mode) => {
          let ({ctx, _}: Info.pat, _) as info = go(~ctx, ~mode, e);
          (ctx, infos @ [info]);
        },
        (ctx, []),
        ps,
        modes,
      );
    let (infos, ms) = List.split(rets);
    let tys = List.map(Info.pat_ty, infos);
    let self: Info.self =
      switch (Typ.join_all(ctx, tys)) {
      | None => NoJoin(List.map2((id, ty) => Info.{id, ty}, p_ids, tys))
      | Some(ty) => Just(List(ty))
      };
    add(~self, ~ctx, union_m(ms));
  | Cons(hd, tl) =>
    let mode_e = Typ.matched_list_mode(mode);
    let ({ty: ty1, ctx, _}: Info.pat, m_hd) = go(~ctx, ~mode=mode_e, hd);
    let ({ctx, _}: Info.pat, m_tl) = go(~ctx, ~mode=Ana(List(ty1)), tl);
    add(~self=Just(List(ty1)), ~ctx, union_m([m_hd, m_tl]));
  | Wild => atomic(Just(unknown))
  | Var(name) =>
    let typ = Info.typ_after_fix(ctx, mode, Just(Unknown(Internal)));
    let entry = Ctx.VarEntry({name, id: pat_id(upat), typ});
    add(~self=Just(unknown), ~ctx=Ctx.extend(entry, ctx), Id.Map.empty);
  | Tuple(ps) =>
    let modes = Typ.matched_prod_mode(mode, List.length(ps));
    let (ctx, rets) =
      List.fold_left2(
        ((ctx, infos), e, mode) => {
          let ({ctx, _}: Info.pat, _) as info = go(~mode, ~ctx, e);
          (ctx, infos @ [info]);
        },
        (ctx, []),
        ps,
        modes,
      );
    let (infos, ms) = List.split(rets);
    let tys = List.map(Info.pat_ty, infos);
    let self = Info.Just(Prod(tys));
    add(~self, ~ctx, union_m(ms));
  | Parens(p) =>
    let ({ty, ctx, _}: Info.pat, m) = go(~ctx, ~mode, p);
    add(~self=Just(ty), ~ctx, m);
  | Tag(tag) => atomic(IsTag(tag, Info.syn_tag_typ(ctx, tag)))
  | Ap(fn, arg) =>
    /* Constructors */
    let fn_mode =
      switch (fn) {
      | {term: Tag(name), _} => Typ.tag_ap_mode(ctx, mode, name)
      | _ => Typ.ap_mode
      };
    let ({ty: ty_fn, ctx, _}: Info.pat, m_fn) = go(~ctx, ~mode=fn_mode, fn);
    let (ty_in, ty_out) = Typ.matched_arrow(ty_fn);
    let ({ctx, _}: Info.pat, m_arg) = go(~ctx, ~mode=Ana(ty_in), arg);
    add(~self=Just(ty_out), ~ctx, union_m([m_fn, m_arg]));
  | TypeAnn(p, ann) =>
    let ({ty: ty_ann, _}: Info.typ, m_ann) =
      utyp_to_info_map(~ctx, ~ancestors, ann);
    let ({ctx, _}: Info.pat, m_p) = go(~ctx, ~mode=Ana(ty_ann), p);
    add(~self=Just(ty_ann), ~ctx, union_m([m_p, m_ann]));
  };
}
and utyp_to_info_map =
    (
      ~ctx,
      ~mode=Info.TypeExpected,
      ~ancestors,
      {ids, term} as utyp: Term.UTyp.t,
    )
    : (Info.typ, map) => {
  let cls: Term.UTyp.cls =
    switch (mode, Term.UTyp.cls_of_term(term)) {
    | (VariantExpected(_), Var) => Tag
    | (_, cls) => cls
    };
  let ty = Term.UTyp.to_typ(ctx, utyp);
  let info = Info.{cls, ctx, ancestors, mode, ty, term: utyp};
  let add = m => (info, add_info(ids, InfoTyp(info), m));
  let ancestors = [typ_id(utyp)] @ ancestors;
  let go' = utyp_to_info_map(~ctx, ~ancestors);
  let go = go'(~mode=TypeExpected);
  //TODO(andrew): make this return free, replacing Typ.free_vars
  switch (term) {
  | Invalid(_)
  | EmptyHole
  | Int
  | Float
  | Bool
  | String => add(Id.Map.empty)
  | Var(_)
  | Tag(_) =>
    /* Names are resolved in Info.status_typ */
    add(Id.Map.empty)
  | List(t)
  | Parens(t) => add(go(t) |> snd)
  | Arrow(t1, t2) => add(union_m([go(t1) |> snd, go(t2) |> snd]))
  | Tuple(ts) => add(ts |> List.map(go) |> List.map(snd) |> union_m)
  | Ap(t1, t2) =>
    let ty_in = Term.UTyp.to_typ(ctx, t2);
    let t1_mode: Info.typ_mode =
      switch (mode) {
      | VariantExpected(m, sum_ty) => TagExpected(m, Arrow(ty_in, sum_ty))
      | _ => TagExpected(Unique, Arrow(ty_in, Unknown(Internal)))
      };
    let m =
      union_m([
        go'(~mode=t1_mode, t1) |> snd,
        go'(~mode=TypeExpected, t2) |> snd,
      ]);
    add(m);
  | USum(ts) =>
    let (ms, _) =
      List.fold_left(
        ((acc, tags), ut) => {
          let (status, tag) =
            switch (Term.UTyp.get_tag(ctx, ut)) {
            | None => (Info.Unique, [])
            | Some(tag) when !List.mem(tag, tags) => (Unique, [tag])
            | Some(tag) => (Duplicate, [tag])
            };
          let m = go'(~mode=VariantExpected(status, ty), ut) |> snd;
          (acc @ [m], tags @ tag);
        },
        ([], []),
        ts,
      );
    add(union_m(ms));
  | MultiHole(tms) =>
    let (_, ms) =
      tms |> List.map(any_to_info_map(~ctx, ~ancestors)) |> List.split;
    add(union_m(ms));
  };
}
and utpat_to_info_map =
    (~ctx, ~ancestors, {ids, term} as utpat: Term.UTPat.t): (Info.tpat, map) => {
  let cls = Term.UTPat.cls_of_term(term);
  let info = Info.{cls, ancestors, ctx, term: utpat};
  let add = m => (info, add_info(ids, InfoTPat(info), m));
  let ancestors = [utpat_id(utpat)] @ ancestors;
  switch (term) {
  | MultiHole(tms) =>
    let ms =
      tms |> List.map(any_to_info_map(~ctx, ~ancestors)) |> List.split |> snd;
    add(union_m(ms));
  | Invalid(_)
  | EmptyHole
  | Var(_) => add(Id.Map.empty)
  };
};

let mk_map =
  Core.Memo.general(~cache_size_bound=1000, e => {
    uexp_to_info_map(
      ~ctx=Builtins.ctx(Builtins.Pervasives.builtins),
      ~ancestors=[],
      e,
    )
    |> snd
  });
