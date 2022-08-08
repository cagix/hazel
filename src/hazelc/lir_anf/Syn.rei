[@deriving sexp]
type syn_types = ExprLabel.Map.t(Typ.t);

[@deriving sexp]
type syn_ok = {types: syn_types};

[@deriving sexp]
type syn_error =
  | /** Case with no rules. */
    CaseEmptyRules(ExprLabel.t)
  | /** Unbound variable. */
    UnboundVar(ExprLabel.t)
  | /** Synthesized type is not necessarily complete. */
    TypeNotNecessarilyComplete(
      ExprLabel.t,
      Typ.t,
    )
  | /** Synthesized type is not indeterminately incomplete. */
    TypeNotIndeterminatelyIncomplete(
      ExprLabel.t,
      Typ.t,
    )
  | /** Synthesized type is not necessarily incomplete. */
    TypeNotNecessarilyIncomplete(
      ExprLabel.t,
      Typ.t,
    )
  | /** Synthesized type does not equal to expected. */
    TypesNotEqual(
      ExprLabel.t,
      Typ.t,
      Typ.t,
    )
  | /** Synthesized base type does not equal to expected. */
    BaseTypesNotEqual(
      ExprLabel.t,
      Typ.t_,
      Typ.t_,
    )
  | /** Synthesized base type is equal to a type to which it should not have
        been equal. */
    BaseTypesEqual(
      ExprLabel.t,
      Typ.t_,
      Typ.t_,
    )
  | /** Synthesized base type is not consistent with expected. */
    BaseTypesInconsistent(
      ExprLabel.t,
      Typ.t_,
      Typ.t_,
    )
  | /** Synthesized scrutinee type is not equal to expected pattern type.  */
    PatScrutTypesNotEqual(
      PatLabel.t,
      Typ.t_,
      Typ.t_,
    );

[@deriving sexp]
type syn_result = result(syn_ok, syn_error);

let syn: (TypContext.t, Delta.t, Anf.block) => syn_result;
