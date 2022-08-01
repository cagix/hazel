include Ident0;

module Map = {
  open Sexplib.Std;

  include Map.Make(Ident0);

  [@deriving sexp]
  type binding('v) = (Ident0.t, 'v);

  let sexp_of_t =
      (sexp_of_v: 'v => Sexplib.Sexp.t, map: t('v)): Sexplib.Sexp.t =>
    map |> bindings |> sexp_of_list(sexp_of_binding(sexp_of_v));
  let t_of_sexp =
      (v_of_sexp: Sexplib.Sexp.t => 'v, sexp: Sexplib.Sexp.t): t('v) =>
    sexp |> list_of_sexp(binding_of_sexp(v_of_sexp)) |> List.to_seq |> of_seq;
};
