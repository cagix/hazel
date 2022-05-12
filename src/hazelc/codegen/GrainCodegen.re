let injl_ctor = "L";
let injr_ctor = "R";

let rec codegen_prog = ({prog_body: (stmts, c)}: Anf.prog): GrainIR.block => {
  let stmts = stmts |> List.map(codegen_stmt);
  let c = codegen_comp(c);
  stmts @ [SExpr(c)];
}

and codegen_stmt = (stmt: Anf.stmt): GrainIR.stmt => {
  switch (stmt.stmt_kind) {
  | SLet(x, NoRec, c) => SLet([x], codegen_comp(c))
  | SLet(x, Rec, c) => SLetRec([x], codegen_comp(c))
  };
}

and codegen_comp = (c: Anf.comp): GrainIR.expr => {
  switch (c.comp_kind) {
  | CImm(i) => codegen_imm(i)
  | CBinOp(op, i1, i2) =>
    EBinOp(codegen_op(op), codegen_imm(i1), codegen_imm(i2))
  | CAp(fn, args) =>
    let args = args |> List.map(codegen_imm);
    EAp(codegen_imm(fn), args);
  | CLam(params, body) => ELam(params, EBlock(codegen_prog(body)))
  | CCons(i1, i2) => ECons(codegen_imm(i1), codegen_imm(i2))
  | CPair(i1, i2) => ETuple([codegen_imm(i1), codegen_imm(i2)])
  | CInj(side, i) =>
    let ctor =
      switch (side) {
      | CInjL => injl_ctor
      | CInjR => injr_ctor
      };
    ECtor(ctor, [codegen_imm(i)]);
  };
}

and codegen_op = (op: Anf.bin_op): GrainIR.bin_op => {
  switch (op) {
  | OpAnd => OpAnd
  | OpOr => OpOr
  | OpPlus => OpPlus
  | OpMinus => OpMinus
  | OpTimes => OpTimes
  | OpDivide => OpDivide
  | OpLessThan => OpLessThan
  | OpGreaterThan => OpGreaterThan
  | OpEquals => OpEquals
  | OpFPlus => OpFPlus
  | OpFMinus => OpFMinus
  | OpFTimes => OpFTimes
  | OpFDivide => OpFDivide
  | OpFLessThan => OpFLessThan
  | OpFGreaterThan => OpFGreaterThan
  | OpFEquals => OpFEquals
  };
}

and codegen_imm = (i: Anf.imm): GrainIR.expr => {
  switch (i.imm_kind) {
  | IConst(const) => codegen_const(const)
  | IVar(var) => EVar(var)
  };
}

and codegen_const = (const: Anf.constant): GrainIR.expr => {
  switch (const) {
  | ConstInt(n) => EIntLit(n)
  | ConstFloat(f) => EFloatLit(f)
  | ConstBool(b) => EBoolLit(b)
  | ConstNil => EList([])
  | ConstTriv => ETriv
  };
};

let codegen = (prog: Anf.prog): GrainIR.prog => {
  // TODO: Add necessary top-level statments.
  let tb = [];
  let b = codegen_prog(prog);
  (tb, b);
};
