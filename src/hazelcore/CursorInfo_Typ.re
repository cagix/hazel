type cursor_term = CursorInfo.cursor_term;
type zoperand = CursorInfo_common.zoperand;

let rec extract_cursor_term = (ztyp: ZTyp.t): cursor_term => {
  switch (ztyp) {
  | ZOpSeq(_, zseq) =>
    switch (zseq) {
    | ZOperand(ztyp_operand, _) => extract_from_ztyp_operand(ztyp_operand)
    | ZOperator(ztyp_operator, _) =>
      let (cursor_pos, uop) = ztyp_operator;
      TypOperator(cursor_pos, uop);
    }
  };
}
and extract_from_zseq = (zseq: ZSeq.t(_, _, _, _)): cursor_term => {
  switch (zseq) {
  | ZOperand(ztyp_operand, _) => extract_from_ztyp_operand(ztyp_operand)
  | ZOperator(ztyp_operator, _) =>
    let (cursor_pos, uop) = ztyp_operator;
    TypOperator(cursor_pos, uop);
  };
}
and extract_from_ztyp_operand = (ztyp_operand: ZTyp.zoperand): cursor_term => {
  switch (ztyp_operand) {
  | CursorT(cursor_pos, utyp_operand) => TypOperand(cursor_pos, utyp_operand)
  | ParenthesizedZ(ztyp)
  | ListZ(ztyp) => extract_cursor_term(ztyp)
  };
};

let rec get_zoperand_from_ztyp = (ztyp: ZTyp.t): option(zoperand) => {
  get_zoperand_from_ztyp_opseq(ztyp);
}
and get_zoperand_from_ztyp_opseq = (zopseq: ZTyp.zopseq): option(zoperand) => {
  switch (zopseq) {
  | ZOpSeq(_, zseq) =>
    switch (zseq) {
    | ZOperand(ztyp_operand, _) =>
      get_zoperand_from_ztyp_operand(ztyp_operand)
    | ZOperator(_, _) => None
    }
  };
}
and get_zoperand_from_ztyp_operand =
    (zoperand: ZTyp.zoperand): option(zoperand) => {
  switch (zoperand) {
  | CursorT(_, _) => Some(ZTyp(zoperand))
  | ParenthesizedZ(ztyp)
  | ListZ(ztyp) => get_zoperand_from_ztyp(ztyp)
  };
};

let rec cursor_info =
        (~steps=[], ctx: Contexts.t, zty: ZTyp.t): option(CursorInfo.t) =>
  cursor_info_zopseq(~steps, ctx, zty)
and cursor_info_zopseq =
    (
      ~steps: CursorPath.steps,
      ctx: Contexts.t,
      ZOpSeq(skel, zseq) as zopseq: ZTyp.zopseq,
    )
    : option(CursorInfo.t) => {
  switch (zseq) {
  | ZOperator((_, Prod), _) =>
    // cursor on tuple comma
    switch (Elaborator_Typ.syn_kind(ctx, zopseq |> ZTyp.erase)) {
    | None => None
    | Some(kind) =>
      Some(
        CursorInfo_common.mk(
          OnType(kind),
          ctx,
          extract_cursor_term(zopseq),
        ),
      )
    }
  | _ =>
    // cursor within tuple element
    let skels = skel |> UHTyp.get_prod_elements;
    let cursor_skel =
      skels |> List.find(skel => ZOpSeq.skel_contains_cursor(skel, zseq));
    cursor_info_skel(~steps, ctx, cursor_skel, zseq);
  };
}
and cursor_info_skel =
    (
      ~steps: CursorPath.steps,
      ctx: Contexts.t,
      skel: UHTyp.skel,
      zseq: ZTyp.zseq,
    )
    : option(CursorInfo.t) => {
  let seq = zseq |> ZTyp.erase_zseq;
  if (ZOpSeq.skel_is_rooted_at_cursor(skel, zseq)) {
    // found cursor
    switch (zseq) {
    | ZOperand(zoperand, (prefix, _)) =>
      cursor_info_zoperand(
        ~steps=steps @ [Seq.length_of_affix(prefix)],
        ctx,
        zoperand,
      )
    | ZOperator(_) =>
      switch (Elaborator_Typ.syn_kind_skel(ctx, skel, seq)) {
      | None => None
      | Some(kind) =>
        Some(
          CursorInfo_common.mk(OnType(kind), ctx, extract_from_zseq(zseq)),
        )
      }
    };
  } else {
    // recurse toward cursor
    switch (skel) {
    | Placeholder(_) => None
    | BinOp(_, Prod, _, _) =>
      failwith(
        "Typ.syn_cursor_info_skel: expected commas to be handled at opseq level",
      )
    | BinOp(_, Arrow | Sum, skel1, skel2) =>
      switch (cursor_info_skel(~steps, ctx, skel1, zseq)) {
      | Some(_) as result => result
      | None => cursor_info_skel(~steps, ctx, skel2, zseq)
      }
    };
  };
}
and cursor_info_zoperand =
    (~steps: CursorPath.steps, ctx: Contexts.t, zoperand: ZTyp.zoperand)
    : option(CursorInfo.t) => {
  let cursor_term = extract_from_ztyp_operand(zoperand);
  switch (zoperand) {
  | CursorT(_, Hole(_)) =>
    Some(CursorInfo_common.mk(OnType(Kind.KHole), ctx, cursor_term))
  | CursorT(_, Unit | Int | Float | Bool) =>
    Some(CursorInfo_common.mk(OnType(Kind.Type), ctx, cursor_term))
  | CursorT(_, TyVar(InTyVarHole(Free, _), _)) =>
    Some(CursorInfo_common.mk(TypFree, ctx, cursor_term))
  | CursorT(_, TyVar(InTyVarHole(Keyword(k), _), _)) =>
    Some(CursorInfo_common.mk(TypKeyword(k), ctx, cursor_term))
  | CursorT(_, e) =>
    switch (Elaborator_Typ.syn_kind_operand(ctx, e)) {
    | None => None
    | Some(kind) =>
      Some(CursorInfo_common.mk(OnType(kind), ctx, cursor_term))
    }

  | ParenthesizedZ(zbody)
  | ListZ(zbody) => cursor_info(~steps=steps @ [0], ctx, zbody)
  };
};
