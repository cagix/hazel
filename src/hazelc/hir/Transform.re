exception FixFError;
exception FreeVarError;
exception WrongTypeError;

let rec transform_exp = (ctx: Contexts.t, d: DHExp.t): (Expr.t, HTyp.t) => {
  switch (d) {
  | EmptyHole(u, i, sigma) =>
    let sigma = transform_var_map(ctx, sigma);
    ({kind: EEmptyHole(u, i, sigma)}, Hole);

  | NonEmptyHole(reason, u, i, sigma, d') =>
    let sigma = transform_var_map(ctx, sigma);
    let (d', _) = transform_exp(ctx, d');
    ({kind: ENonEmptyHole(reason, u, i, sigma, d')}, Hole);

  | ExpandingKeyword(u, i, sigma, k) =>
    let sigma = transform_var_map(ctx, sigma);
    ({kind: EKeyword(u, i, sigma, k)}, Hole);

  | FreeVar(u, i, sigma, k) =>
    let sigma = transform_var_map(ctx, sigma);
    ({kind: EFreeVar(u, i, sigma, k)}, Hole);

  | InvalidText(u, i, sigma, text) =>
    let sigma = transform_var_map(ctx, sigma);
    ({kind: EInvalidText(u, i, sigma, text)}, Hole);

  | BoundVar(x) =>
    switch (VarMap.lookup(Contexts.gamma(ctx), x)) {
    | Some(ty) => ({kind: EBoundVar(ty, x)}, ty)
    | None => raise(FreeVarError)
    }

  | FixF(_) => raise(FixFError)
  | Let(Var(_), FixF(x, ty, Fun(dp, dp_ty, d3)), body) =>
    // TODO: Not really sure if any of this recursive function handling is right...
    let (dp, ctx) = transform_pat(ctx, dp, ty);
    let ctx = VarMap.extend(ctx, (x, ty));

    let (d3, _) = transform_exp(ctx, d3);
    let (body, body_ty) = transform_exp(ctx, body);
    ({kind: ELetRec(x, dp, dp_ty, d3, body)}, body_ty);

  | Let(dp, d', body) =>
    let (d', d'_ty) = transform_exp(ctx, d');
    let (dp, body_ctx) = transform_pat(ctx, dp, d'_ty);
    let (body, body_ty) = transform_exp(body_ctx, body);
    ({kind: ELet(dp, d', body)}, body_ty);

  | Fun(dp, dp_ty, body) =>
    // TODO: Can't assume anything about indet-ness of argument when called?
    let (dp, body_ctx) = transform_pat(ctx, dp, dp_ty);
    let (body, body_ty) = transform_exp(body_ctx, body);
    ({kind: EFun(dp, dp_ty, body)}, Arrow(dp_ty, body_ty));

  | Ap(fn, arg) =>
    let (fn, fn_ty) = transform_exp(ctx, fn);
    let (arg, _) = transform_exp(ctx, arg);
    switch (fn.kind) {
    // TODO: expand arrow casts and do transform_exp recursively here
    | ECast(_fn, _ty1, _ty2) => failwith("FnCastExpansion")
    | EFun(_, _, _) =>
      switch (fn_ty) {
      | Arrow(_, ty') => ({kind: EAp(fn, arg)}, ty')
      | _ => raise(WrongTypeError)
      }
    | _ => failwith("NotImplemented")
    };

  | ApBuiltin(name, args) =>
    let args =
      args
      |> List.map(arg =>
           switch (transform_exp(ctx, arg)) {
           | (arg, _) => arg
           }
         );

    switch (VarMap.lookup(Contexts.gamma(ctx), name)) {
    | Some(Arrow(_, ty')) => ({kind: EApBuiltin(name, args)}, ty')
    | _ => raise(WrongTypeError)
    };

  | BinBoolOp(op, d1, d2) =>
    let (d1, _) = transform_exp(ctx, d1);
    let (d2, _) = transform_exp(ctx, d2);
    let op = transform_bool_op(op);
    ({kind: EBinBoolOp(op, d1, d2)}, Bool);

  | BinIntOp(op, d1, d2) =>
    let (d1, _) = transform_exp(ctx, d1);
    let (d2, _) = transform_exp(ctx, d2);
    let op = transform_int_op(op);
    ({kind: EBinIntOp(op, d1, d2)}, Int);

  | BinFloatOp(op, d1, d2) =>
    let (d1, _) = transform_exp(ctx, d1);
    let (d2, _) = transform_exp(ctx, d2);
    let op = transform_float_op(op);
    ({kind: EBinFloatOp(op, d1, d2)}, Float);

  | Pair(d1, d2) =>
    let (d1, d1_ty) = transform_exp(ctx, d1);
    let (d2, d2_ty) = transform_exp(ctx, d2);
    ({kind: EPair(d1, d2)}, Prod([d1_ty, d2_ty]));

  | Cons(d1, d2) =>
    let (d1, _) = transform_exp(ctx, d1);
    let (d2, d2_ty) = transform_exp(ctx, d2);
    ({kind: ECons(d1, d2)}, d2_ty);

  | Inj(other_ty, side, d') =>
    let (d', d'_ty) = transform_exp(ctx, d');
    let ty: HTyp.t =
      switch (side) {
      | L => Sum(d'_ty, other_ty)
      | R => Sum(other_ty, d'_ty)
      };
    ({kind: EInj(ty, side, d')}, ty);

  | BoolLit(b) => ({kind: EBoolLit(b)}, Bool)

  | IntLit(i) => ({kind: EIntLit(i)}, Int)

  | FloatLit(f) => ({kind: EFloatLit(f)}, Float)

  | ListNil(ty) => ({kind: ENil(ty)}, List(ty))

  | Triv => ({kind: ETriv}, Prod([]))

  | ConsistentCase(case) =>
    let (case, case_ty) = transform_case(ctx, case);
    ({kind: EConsistentCase(case)}, case_ty);

  | InconsistentBranches(u, i, sigma, case) =>
    let sigma = transform_var_map(ctx, sigma);
    let (case, _) = transform_case(ctx, case);
    ({kind: EInconsistentBranches(u, i, sigma, case)}, Hole);

  | Cast(d', ty, ty') =>
    // FIXME: default implementation of Cast
    // let (d', _) = transform_exp(ctx, d');
    // ({kind: ECast(d', ty, ty')}, ty');
    switch (HTyp.ground_cases_of(ty), HTyp.ground_cases_of(ty')) {
    | (GNotGroundOrHole(_), GNotGroundOrHole(_)) =>
      if (HTyp.eq(ty, ty')) {
        transform_exp(ctx, d');
      } else {
        let (d', _) = transform_exp(ctx, d');
        ({kind: ECast(d', ty, ty')}, ty');
      }
    | _ =>
      let (d', _) = transform_exp(ctx, d);
      ({kind: ECast(d', ty, ty')}, ty');
    }

  | FailedCast(d', ty, ty') =>
    let (d', _) = transform_exp(ctx, d');
    ({kind: EFailedCast(d', ty, ty')}, ty');

  | InvalidOperation(d', err) =>
    let (d', d'_ty) = transform_exp(ctx, d');
    ({kind: EInvalidOperation(d', err)}, d'_ty);
  };
}

and transform_bool_op = (op: DHExp.BinBoolOp.t): Expr.bin_bool_op => {
  switch (op) {
  | And => Expr.OpAnd
  | Or => Expr.OpOr
  };
}

and transform_int_op = (op: DHExp.BinIntOp.t): Expr.bin_int_op => {
  switch (op) {
  | Minus => Expr.OpMinus
  | Plus => Expr.OpPlus
  | Times => Expr.OpTimes
  | Divide => Expr.OpDivide
  | LessThan => Expr.OpLessThan
  | GreaterThan => Expr.OpGreaterThan
  | Equals => Expr.OpEquals
  };
}

and transform_float_op = (op: DHExp.BinFloatOp.t): Expr.bin_float_op => {
  switch (op) {
  | FMinus => Expr.OpFMinus
  | FPlus => Expr.OpFPlus
  | FTimes => Expr.OpFTimes
  | FDivide => Expr.OpFDivide
  | FLessThan => Expr.OpFLessThan
  | FGreaterThan => Expr.OpFGreaterThan
  | FEquals => Expr.OpFEquals
  };
}

and transform_case = (ctx: Contexts.t, case: DHExp.case): (Expr.case, HTyp.t) => {
  switch (case) {
  // TODO: Check that all rules have same type.
  | Case(scrut, rules, i) =>
    let (scrut, scrut_ty) = transform_exp(ctx, scrut);
    let (rules_rev, rules_ty) =
      rules
      |> List.fold_left(
           ((rules, _), rule) =>
             switch (transform_rule(ctx, rule, scrut_ty)) {
             | (rule, rule_ty) => ([rule, ...rules], rule_ty)
             },
           ([], HTyp.Hole),
         );
    let rules = List.rev(rules_rev);

    ({case_kind: ECase(scrut, rules, i)}, rules_ty);
  };
}

and transform_rule =
    (ctx: Contexts.t, rule: DHExp.rule, scrut_ty: HTyp.t)
    : (Expr.rule, HTyp.t) => {
  switch (rule) {
  | Rule(dp, d) =>
    let (dp, ctx') = transform_pat(ctx, dp, scrut_ty);
    let (d, d_ty) = transform_exp(ctx', d);
    ({rule_kind: ERule(dp, d)}, d_ty);
  };
}

and transform_var_map =
    (ctx: Contexts.t, sigma: VarMap.t_(DHExp.t)): VarMap.t_(Expr.t) =>
  sigma
  |> List.map(((x, d)) => {
       let (d, _) = transform_exp(ctx, d);
       (x, d);
     })

and transform_pat =
    (ctx: Contexts.t, dp: DHPat.t, ty: HTyp.t): (Pat.t, Contexts.t) => {
  switch (dp) {
  | EmptyHole(u, i) => ({kind: PEmptyHole(u, i)}, ctx)

  | NonEmptyHole(reason, u, i, dp) =>
    let (dp, ctx) = transform_pat(ctx, dp, ty);
    ({kind: PNonEmptyHole(reason, u, i, dp)}, ctx);

  | ExpandingKeyword(u, i, k) => ({kind: PKeyword(u, i, k)}, ctx)

  | InvalidText(u, i, t) => ({kind: PInvalidText(u, i, t)}, ctx)

  | Wild => ({kind: PWild}, ctx)

  | Ap(dp1, dp2) =>
    /* FIXME: Hole type scrutinee? */
    switch (ty) {
    | Arrow(dp1_ty, dp2_ty) =>
      let (dp1, ctx) = transform_pat(ctx, dp1, dp1_ty);
      let (dp2, ctx) = transform_pat(ctx, dp2, dp2_ty);
      ({kind: PAp(dp1, dp2)}, ctx);
    | _ => raise(WrongTypeError)
    }

  | Pair(dp1, dp2) =>
    switch (ty) {
    | Prod([dp1_ty, dp2_ty]) =>
      let (dp1, ctx) = transform_pat(ctx, dp1, dp1_ty);
      let (dp2, ctx) = transform_pat(ctx, dp2, dp2_ty);
      ({kind: PPair(dp1, dp2)}, ctx);
    | _ => raise(WrongTypeError)
    }

  | Cons(dp, dps) =>
    switch (ty) {
    | List(ty') =>
      let (dp, ctx) = transform_pat(ctx, dp, ty');
      let (dps, ctx) = transform_pat(ctx, dps, ty);
      ({kind: PCons(dp, dps)}, ctx);
    | _ => raise(WrongTypeError)
    }

  | Var(x) =>
    let gamma' = VarMap.extend(Contexts.gamma(ctx), (x, ty));
    ({kind: PVar(x)}, gamma');

  | IntLit(i) => ({kind: PIntLit(i)}, ctx)

  | FloatLit(f) => ({kind: PFloatLit(f)}, ctx)

  | BoolLit(b) => ({kind: PBoolLit(b)}, ctx)

  | Inj(side, dp') =>
    switch (side, ty) {
    | (L, Sum(ty, _))
    | (R, Sum(_, ty)) =>
      let (dp', ctx) = transform_pat(ctx, dp', ty);
      ({kind: PInj(side, dp')}, ctx);
    | _ => raise(WrongTypeError)
    }

  | ListNil => ({kind: PNil}, ctx)

  | Triv => ({kind: PTriv}, ctx)
  };
};

let transform = (ctx: Contexts.t, d: DHExp.t): Expr.t => {
  let (d, _) = transform_exp(ctx, d);
  d;
};
