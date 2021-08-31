open OptUtil.Syntax;
open SuggestionsExp;

let rec mk_ap_iter_seq =
        (f_ty: HTyp.t, hole_ty: HTyp.t): option((HTyp.t, UHExp.seq)) => {
  switch (f_ty) {
  | Arrow(_, out_ty) when HTyp.consistent(out_ty, hole_ty) =>
    Some((out_ty, Seq.wrap(hole_operand)))
  | Arrow(_, out_ty') =>
    let* (out_ty, affix) = mk_ap_iter_seq(out_ty', hole_ty);
    Some((out_ty, ap_seq(hole_operand, affix)));
  | _ => None
  };
};

let mk_ap_iter =
    ({expected_ty, _}: CursorInfo.t, f: Var.t, f_ty: HTyp.t)
    : option((HTyp.t, UHExp.t)) => {
  let+ (output_ty, holes_seq) = mk_ap_iter_seq(f_ty, expected_ty);
  (output_ty, mk_ap(f, holes_seq));
};

/* returns a blank score if there is an error checking a suggestion */
let mk_operand_suggestion' =
    (
      ~ci: CursorInfo.t,
      ~category: Suggestion.category,
      ~result_text: string,
      ~operand: UHExp.operand,
      ~result,
      ~action: Action.t,
    )
    : suggestion => {
  let res_ty = HTyp.relax(Statics_Exp.syn_operand(ci.ctx, operand));
  let score: Suggestion.score =
    switch (SuggestionScore.check_suggestion(action, res_ty, result_text, ci)) {
    | None =>
      Printf.printf(
        "Warning: failed to generate a score for suggested action: %s\n",
        Sexplib.Sexp.to_string_hum(Action.sexp_of_t(action)),
      );
      Suggestion.blank_score;
    | Some(res) => res
    };
  {
    ...Suggestion.mk(~category, ~result_text, ~action, ~result, ~res_ty),
    score,
  };
};

let mk_operand_suggestion =
    (~operand, ~result=UHExp.Block.wrap(operand), ~category, ci) =>
  mk_operand_suggestion'(
    ~ci,
    ~category,
    ~operand,
    ~result_text=UHExp.string_of_operand(operand),
    ~action=ReplaceOperand(operand, None),
    ~result,
  );

let mk_lit_suggestion = mk_operand_suggestion(~category=InsertLit);

// INTRO SUGGESTIONS  ------------------------------------------------------------

let mk_bool_lit_suggestion = (ci: CursorInfo.t, b: bool): suggestion =>
  mk_lit_suggestion(~operand=UHExp.boollit(b), ci);

let mk_int_lit_suggestion = (ci: CursorInfo.t, s: string): suggestion =>
  mk_lit_suggestion(~operand=UHExp.intlit(s), ci);

let mk_float_lit_suggestion = (ci: CursorInfo.t, s: string): suggestion =>
  mk_lit_suggestion(~operand=UHExp.floatlit(s), ci);

let mk_nil_list_suggestion = (ci: CursorInfo.t): suggestion =>
  mk_lit_suggestion(~operand=UHExp.listnil(), ci);

let mk_var_suggestion = (ci: CursorInfo.t, (s: string, _)): suggestion =>
  mk_operand_suggestion(~category=InsertVar, ~operand=UHExp.var(s), ci);

let mk_empty_hole_suggestion = (ci: CursorInfo.t): suggestion =>
  mk_operand_suggestion(~category=Delete, ~operand=hole_operand, ci);

let mk_inj_suggestion = (ci: CursorInfo.t, side: InjSide.t): suggestion =>
  mk_operand_suggestion(
    ~category=InsertConstructor,
    ~operand=mk_inj(side),
    ci,
  );

let mk_case_suggestion = (ci: CursorInfo.t): suggestion =>
  mk_operand_suggestion(~category=InsertElim, ~operand=case_operand, ci);

let mk_lambda_suggestion = (ci: CursorInfo.t): suggestion =>
  mk_operand_suggestion(
    ~category=InsertConstructor,
    ~operand=lambda_operand,
    ci,
  );

let mk_intro_suggestions = (ci: CursorInfo.t): list(suggestion) => [
  mk_empty_hole_suggestion(ci),
  mk_bool_lit_suggestion(ci, true),
  mk_bool_lit_suggestion(ci, false),
  mk_nil_list_suggestion(ci),
  mk_inj_suggestion(ci, L),
  mk_inj_suggestion(ci, R),
  mk_lambda_suggestion(ci),
];

let intro_suggestions =
    ({expected_ty, _} as ci: CursorInfo.t): list(suggestion) =>
  ci
  |> mk_intro_suggestions
  |> List.filter((a: suggestion) => HTyp.consistent(a.res_ty, expected_ty));

// VAR SUGGESTIONS -----------------------------------------------------------

let var_suggestions =
    ({ctx, expected_ty, _} as ci: CursorInfo.t): list(suggestion) =>
  expected_ty
  |> Assistant_common.extract_vars(ctx)
  |> List.map(mk_var_suggestion(ci));

// ELIM SUGGESTIONS ----------------------------------------------------------

let mk_app_suggestion =
    (ci: CursorInfo.t, (name: string, f_ty: HTyp.t)): suggestion => {
  let (_, result) =
    mk_ap_iter(ci, name, f_ty)
    |> OptUtil.get(_ => failwith("mk_app_suggestion"));
  mk_operand_suggestion(
    ~category=InsertApp,
    ~operand=UHExp.Parenthesized(result),
    ~result,
    ci,
  );
};

let app_suggestions =
    ({ctx, expected_ty, _} as ci: CursorInfo.t): list(suggestion) => {
  expected_ty
  |> Assistant_common.fun_vars(ctx)
  |> List.map(mk_app_suggestion(ci));
};

let vars_of_type_matching_str = (ctx: Contexts.t, typ: HTyp.t, str: string) => {
  ctx
  |> Contexts.gamma
  |> VarMap.filter(((name, ty)) =>
       switch (StringUtil.search_forward_opt(Str.regexp(str), name)) {
       | None => false
       | Some(_) => HTyp.consistent(ty, typ)
       // TODO(andrew): return measure of match quality?
       }
     );
};

let get_wrap_operand =
    (
      {ctx, _}: CursorInfo.t,
      arg_ty,
      wrap_name: string,
      cursor_term: CursorInfo.cursor_term,
    ) => {
  switch (cursor_term) {
  | ExpOperand(
      OnText(i),
      (Var(_, InVarHole(_), s) | InvalidText(_, s)) as operand,
    ) =>
    /*
      If we're on an unbound variable or invalidtext, try to interpret
      it as the user attempting to wrap the current operand, splitting
      the cursortext at the cursor and trying to match the wrapper to
      the prefix and find a wrappee matching the suffix
     */
    let (pre, suf) = StringUtil.split_string(i, s);
    switch (StringUtil.search_forward_opt(Str.regexp(pre), wrap_name)) {
    | None => operand
    | Some(_) =>
      let suf_op = UHExp.operand_of_string(suf);
      if (UHExp.is_atomic_operand(suf_op) && UHExp.is_literal_operand(suf_op)) {
        suf_op;
      } else {
        switch (vars_of_type_matching_str(ctx, arg_ty, suf)) {
        | [] => operand
        | [(name, _), ..._] =>
          // TODO: return best match rather than first
          UHExp.operand_of_string(name)
        };
      };
    };
  | ExpOperand(
      _,
      (
        Var(NotInHole, NotInVarHole, _) |
        Var(InHole(TypeInconsistent, _), _, _)
      ) as operand,
    ) =>
    // TODO: consider bound variables which are coincidentally wraps
    operand
  | ExpOperand(_, operand) => operand
  | _ => failwith("get_wrap_operand impossible")
  };
};

let mk_wrap_case_suggestion =
    ({cursor_term, _} as ci: CursorInfo.t): suggestion => {
  let operand =
    cursor_term
    |> get_wrap_operand(ci, HTyp.Hole, "case")
    |> UHExp.Block.wrap
    |> mk_case;
  mk_operand_suggestion(~category=Wrap, ~operand, ci);
};

let elim_suggestions = (ci: CursorInfo.t): list(suggestion) =>
  app_suggestions(ci) @ [mk_wrap_case_suggestion(ci)];

let mk_operand_wrap_suggestion = (~ci: CursorInfo.t, ~category, ~operand) =>
  mk_operand_suggestion'(
    ~ci,
    ~category,
    ~operand,
    ~result_text=UHExp.string_of_operand(operand),
    ~action=ReplaceOperand(operand, None),
  );

let mk_wrap_suggestion =
    ({cursor_term, _} as ci: CursorInfo.t, (name: string, _)) => {
  let get_arg_type_somehow_TODO = HTyp.Hole;
  let result =
    mk_ap(
      name,
      S(
        get_wrap_operand(ci, get_arg_type_somehow_TODO, name, cursor_term),
        E,
      ),
    );
  mk_operand_suggestion(
    ~category=Wrap,
    ~operand=UHExp.Parenthesized(result),
    ~result,
    ci,
  );
};

let wrap_suggestions =
    ({ctx, expected_ty, actual_ty, cursor_term, _} as ci: CursorInfo.t) =>
  switch (cursor_term) {
  | ExpOperand(_, EmptyHole(_)) =>
    /* Wrapping empty holes is redundant to ap. Revisit when there's
       a mechanism to eliminate duplicate suggestions */
    []
  | _ =>
    let arrow_consistent = ((_, f_ty)) =>
      HTyp.consistent(f_ty, HTyp.Arrow(HTyp.relax(actual_ty), expected_ty));
    Assistant_common.fun_vars(ctx, expected_ty)
    |> List.filter(arrow_consistent)
    |> List.map(mk_wrap_suggestion(ci));
  };

let str_float_to_int = s =>
  s |> float_of_string |> Float.to_int |> string_of_int;
let str_int_to_float = s =>
  s |> int_of_string |> Float.of_int |> string_of_float;

let int_float_suggestions =
    ({cursor_term, expected_ty, _} as ci: CursorInfo.t): list(suggestion) => {
  /* This functions handles the suggestion of both float/int literals
     and repair conversions between the two. These could be seperated
     out once we have a system for identifying duplicate suggestions */
  (
    switch (cursor_term) {
    | ExpOperand(_, IntLit(_, s)) when s != "0" => [
        mk_float_lit_suggestion(ci, s ++ "."),
        mk_int_lit_suggestion(ci, "0"),
      ]
    | ExpOperand(_, IntLit(_, s)) when s == "0" => [
        mk_float_lit_suggestion(ci, str_int_to_float(s)),
      ]
    | ExpOperand(_, FloatLit(_, s)) when float_of_string(s) != 0.0 =>
      s |> float_of_string |> Float.is_integer
        ? [
          mk_int_lit_suggestion(ci, str_float_to_int(s)),
          mk_float_lit_suggestion(ci, "0."),
        ]
        : [mk_int_lit_suggestion(ci, "0")]
    | ExpOperand(_, FloatLit(_, s)) when float_of_string(s) == 0.0 => [
        mk_int_lit_suggestion(ci, "0"),
      ]
    | _ => [
        mk_float_lit_suggestion(ci, "0."),
        mk_int_lit_suggestion(ci, "0"),
      ]
    }
  )
  |> List.filter((a: suggestion) => HTyp.consistent(a.res_ty, expected_ty));
};

let mk = (ci: CursorInfo.t): list(suggestion) =>
  int_float_suggestions(ci)
  @ wrap_suggestions(ci)
  @ intro_suggestions(ci)
  @ var_suggestions(ci)
  @ elim_suggestions(ci);
