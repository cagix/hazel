open Virtual_dom.Vdom;
open Node;
open Util.Web;
open Haz3lcore;

<<<<<<< HEAD
let rec view =
        (
          ~font_metrics: option(FontMetrics.t)=None,
          ~with_cls: bool=true,
          ~is_left_child: bool=false,
          ty: Haz3lcore.Typ.t,
        )
        : Node.t => {
=======
let ty_view = (cls: string, s: string): Node.t =>
  div(~attr=clss(["typ-view", cls]), [text(s)]);

let alias_view = (s: string): Node.t =>
  div(~attr=clss(["typ-alias-view"]), [text(s)]);

let prov_view: Typ.type_provenance => Node.t =
  fun
  | Internal => div([])
  | Free(name) =>
    div(~attr=clss(["typ-mod", "free-type-var"]), [text(name)])
  | TypeHole => div(~attr=clss(["typ-mod", "type-hole"]), [text("𝜏")])
  | SynSwitch => div(~attr=clss(["typ-mod", "syn-switch"]), [text("⇒")]);

let rec view_ty = (ty: Haz3lcore.Typ.t): Node.t =>
>>>>>>> dev
  //TODO: parens on ops when ambiguous
  let parenthesize_if_left_child = (n): Node.t =>
    (is_left_child ? [Node.text("("), ...n] @ [Node.text(")")] : n) |> span;
  let div = (~attr, nodes) => with_cls ? div(~attr, nodes) : span(nodes);
  let ty_view = (cls: string, s: string): Node.t =>
    div(~attr=clss(["typ-view", cls]), [text(s)]);
  switch (ty) {
<<<<<<< HEAD
  | Unknown(_) =>
    switch (font_metrics) {
    | Some(font_metrics) =>
      div(
        ~attr=clss(["typ-view", "atom", "unknown"]),
        [
          EmptyHoleDec.relative_view(
            ~font_metrics,
            false,
            Haz3lcore.InferenceResult.hole_mold,
          ),
        ],
      )
    | _ => div(~attr=clss(["typ-view", "atom", "unknown"]), [text("")])
    }
=======
  | Unknown(prov) =>
    div(
      ~attr=
        Attr.many([
          clss(["typ-view", "atom", "unknown"]),
          Attr.title(Typ.show_type_provenance(prov)),
        ]),
      [text("?") /*, prov_view(prov)*/],
    )
>>>>>>> dev
  | Int => ty_view("Int", "Int")
  | Float => ty_view("Float", "Float")
  | String => ty_view("String", "String")
  | Bool => ty_view("Bool", "Bool")
  | Var(name) => ty_view("Var", name)
  | Rec(x, t) =>
    div(
      ~attr=clss(["typ-view", "Rec"]),
      [text("Rec " ++ x ++ ". "), view_ty(t)],
    )
  | List(t) =>
    div(
      ~attr=clss(["typ-view", "atom", "List"]),
<<<<<<< HEAD
      [text("["), view(~font_metrics, ~with_cls, t), text("]")],
    )
  | Arrow(t1, t2) =>
    [
      div(
        ~attr=clss(["typ-view", "Arrow"]),
        [
          view(~font_metrics, ~with_cls, ~is_left_child=true, t1),
          text(" -> "),
          view(~font_metrics, ~with_cls, t2),
        ],
      ),
    ]
    |> parenthesize_if_left_child
=======
      [text("["), view_ty(t), text("]")],
    )
  | Arrow(t1, t2) =>
    div(
      ~attr=clss(["typ-view", "Arrow"]),
      [view_ty(t1), text(" -> "), view_ty(t2)],
    )
>>>>>>> dev
  | Prod([]) => div(~attr=clss(["typ-view", "Prod"]), [text("()")])
  | Prod([_]) =>
    div(~attr=clss(["typ-view", "Prod"]), [text("Singleton Product")])
  | Prod([t0, ...ts]) =>
    div(
      ~attr=clss(["typ-view", "atom", "Prod"]),
      [
        text("("),
        div(
          ~attr=clss(["typ-view", "Prod"]),
<<<<<<< HEAD
          [view(~font_metrics, ~with_cls, t0)]
          @ (
            List.map(
              t => [text(", "), view(~font_metrics, ~with_cls, t)],
              ts,
            )
            |> List.flatten
          ),
=======
          [view_ty(t0)]
          @ (List.map(t => [text(", "), view_ty(t)], ts) |> List.flatten),
>>>>>>> dev
        ),
        text(")"),
      ],
    )
<<<<<<< HEAD
  | Sum(t1, t2) =>
    [
      div(
        ~attr=clss(["typ-view", "Sum"]),
        [
          view(~font_metrics, ~with_cls, ~is_left_child=true, t1),
          text(" + "),
          view(~font_metrics, ~with_cls, t2),
        ],
      ),
    ]
    |> parenthesize_if_left_child
  };
};
// }
// and view_of_potential_typ_set =
//     (
//       ~font_metrics,
//       ~with_cls,
//       outermost,
//       potential_typ_set: PotentialTypeSet.t,
//     )
//     : Node.t => {
//   let div = (~attr, nodes) => with_cls ? div(~attr, nodes) : span(nodes);
//   switch (potential_typ_set) {
//   | [] =>
//     view(
//       ~font_metrics=Some(font_metrics),
//       ~with_cls,
//       Typ.Unknown(NoProvenance),
//     )
//   | [hd] => view_of_potential_typ(~font_metrics, ~with_cls, outermost, hd)
//   | _ =>
//     div(
//       ~attr=clss(["typ-view", "atom", "unknown"]),
//       [
//         EmptyHoleDec.relative_view(
//           ~font_metrics,
//           true,
//           Haz3lcore.InferenceResult.hole_mold,
//         ),
//       ],
//     )
//   };
// }
// and view_of_potential_typ =
//     (
//       ~font_metrics,
//       ~with_cls: bool,
//       is_left_child: bool,
//       potential_typ: PotentialTypeSet.potential_typ,
//     )
//     : Node.t => {
//   let div = (~attr, nodes) => with_cls ? div(~attr, nodes) : span(nodes);
//   switch (potential_typ) {
//   | Base(btyp) => view_of_base_typ(~font_metrics, ~with_cls, btyp)
//   | Binary(ctor, potential_typ_set_lt, potential_typ_set_rt) =>
//     let (ctor_start, ctor_string, ctor_end, cls) =
//       switch (ctor) {
//       | CArrow =>
//         is_left_child
//           ? ("(", " -> ", ")", ["typ-view", "Arrow"])
//           : ("", " -> ", "", ["typ-view", "Arrow"])
//       | CProd => ("(", ", ", ")", ["typ-view", "Sum"])
//       | CSum =>
//         is_left_child
//           ? ("(", " + ", ")", ["typ-view", "Sum"])
//           : ("", " + ", "", ["typ-view", "Sum"])
//       };
//     div(
//       ~attr=clss(cls),
//       [
//         text(ctor_start),
//         view_of_potential_typ_set(
//           ~font_metrics,
//           ~with_cls,
//           false,
//           potential_typ_set_lt,
//         ),
//         text(ctor_string),
//         view_of_potential_typ_set(
//           ~font_metrics,
//           ~with_cls,
//           false,
//           potential_typ_set_rt,
//         ),
//         text(ctor_end),
//       ],
//     );
//   | Unary(ctor, potential_typ_set) =>
//     let (start_text, end_text, cls) =
//       switch (ctor) {
//       | CList => ("[", "]", ["typ-view", "atom", "List"])
//       };
//     div(
//       ~attr=clss(cls),
//       [
//         text(start_text),
//         view_of_potential_typ_set(
//           ~font_metrics,
//           ~with_cls,
//           false,
//           potential_typ_set,
//         ),
//         text(end_text),
//       ],
//     );
//   };
// }
// and view_of_base_typ =
//     (~font_metrics, ~with_cls, btyp: PotentialTypeSet.base_typ): Node.t => {
//   btyp
//   |> PotentialTypeSet.base_typ_to_ityp
//   |> ITyp.ityp_to_typ
//   |> view(~font_metrics=Some(font_metrics), ~with_cls);
// };
=======
  | Sum(ts) =>
    div(
      ~attr=clss(["typ-view", "Sum"]),
      switch (ts) {
      | [] => [text("Nullary Sum")]
      | [t0] => [text("+")] @ ctr_view(t0)
      | [t0, ...ts] =>
        let ts_views =
          List.map(t => [text(" + ")] @ ctr_view(t), ts) |> List.flatten;
        ctr_view(t0) @ ts_views;
      },
    )
  }
and ctr_view = ((ctr, typ)) =>
  switch (typ) {
  | None => [text(ctr)]
  | Some(typ) => [text(ctr ++ "("), view_ty(typ), text(")")]
  };

let view = (ty: Haz3lcore.Typ.t): Node.t =>
  div_c("typ-wrapper", [view_ty(ty)]);
>>>>>>> dev
