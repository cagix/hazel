include Ident.Map;

[@deriving sexp]
type t = Ident.Map.t(Typ.t);
