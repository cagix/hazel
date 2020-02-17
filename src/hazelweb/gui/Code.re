module Js = Js_of_ocaml.Js;
module Dom = Js_of_ocaml.Dom;
module Dom_html = Js_of_ocaml.Dom_html;
module Vdom = Virtual_dom.Vdom;
open ViewUtil;

type annot = TermAnnot.t;

let contenteditable_false = Vdom.Attr.create("contenteditable", "false");

let clss_of_err: ErrStatus.t => list(cls) =
  fun
  | NotInHole => []
  | InHole(_) => ["InHole"];

let clss_of_verr: VarErrStatus.t => list(cls) =
  fun
  | NotInVarHole => []
  | InVarHole(_) => ["InVarHole"];

let cursor_clss = (has_cursor: bool): list(cls) =>
  has_cursor ? ["Cursor"] : [];

let family_clss: TermFamily.t => list(cls) =
  fun
  | Typ => ["Typ"]
  | Pat => ["Pat"]
  | Exp => ["Exp"];

let shape_clss: TermShape.t => list(cls) =
  fun
  | Rule => ["Rule"]
  | Case({err}) => ["Case", ...clss_of_err(err)]
  | Var({err, verr, show_use}) =>
    ["Operand", "Var", ...clss_of_err(err)]
    @ clss_of_verr(verr)
    @ (show_use ? ["show-use"] : [])
  | Operand({err}) => ["Operand", ...clss_of_err(err)]
  | BinOp({err, op_index: _}) => ["BinOp", ...clss_of_err(err)]
  | NTuple({err, comma_indices: _}) => ["NTuple", ...clss_of_err(err)]
  | SubBlock(_) => ["SubBlock"];

let open_child_clss = (has_inline_OpenChild: bool, has_para_OpenChild: bool) =>
  List.concat([
    has_inline_OpenChild ? ["has-Inline-OpenChild"] : [],
    has_para_OpenChild ? ["has-Para-OpenChild"] : [],
  ]);

let has_child_clss = (has_child: bool) =>
  has_child ? ["has-child"] : ["no-children"];

let caret_from_left = (from_left: float): Vdom.Node.t => {
  assert(0.0 <= from_left && from_left <= 100.0);
  let left_attr =
    Vdom.Attr.create(
      "style",
      "left: " ++ string_of_float(from_left) ++ "0%;",
    );
  Vdom.Node.span(
    [Vdom.Attr.id("caret"), contenteditable_false, left_attr],
    [],
  );
};

let caret_of_side: Side.t => Vdom.Node.t =
  fun
  | Before => caret_from_left(0.0)
  | After => caret_from_left(100.0);

let contenteditable_of_layout =
    (
      ~inject: Update.Action.t => Vdom.Event.t,
      ~show_contenteditable: bool,
      ~width: int,
      l: TermLayout.t,
    )
    : Vdom.Node.t => {
  open Vdom;

  let on_click_noneditable =
      (
        ~rev_steps: CursorPath.rev_steps,
        ~cursor_before: CursorPosition.t,
        ~cursor_after: CursorPosition.t,
        evt,
      )
      : Vdom.Event.t => {
    let steps = rev_steps |> List.rev;
    let path_before = (steps, cursor_before);
    let path_after = (steps, cursor_after);
    switch (Js.Opt.to_option(evt##.target)) {
    | None => inject(Update.Action.EditAction(MoveTo(path_before)))
    | Some(target) =>
      let from_left =
        float_of_int(evt##.clientX) -. target##getBoundingClientRect##.left;
      let from_right =
        target##getBoundingClientRect##.right -. float_of_int(evt##.clientX);
      let path = from_left <= from_right ? path_before : path_after;
      inject(Update.Action.EditAction(MoveTo(path)));
    };
  };

  let on_click_text =
      (~rev_steps: CursorPath.rev_steps, ~length: int, evt): Vdom.Event.t => {
    let steps = rev_steps |> List.rev;
    switch (Js.Opt.to_option(evt##.target)) {
    | None => inject(Update.Action.EditAction(MoveToBefore(steps)))
    | Some(target) =>
      let from_left =
        float_of_int(evt##.clientX) -. target##getBoundingClientRect##.left;
      let from_right =
        target##getBoundingClientRect##.right -. float_of_int(evt##.clientX);
      let char_index =
        floor(
          from_left
          /. (from_left +. from_right)
          *. float_of_int(length)
          +. 0.5,
        )
        |> int_of_float;
      inject(
        Update.Action.EditAction(MoveTo((steps, OnText(char_index)))),
      );
    };
  };

  let caret_position = (rev_steps: CursorPath.steps, cursor: CursorPosition.t) =>
    Node.span(
      [
        Attr.on("move_caret_here", _ => {
          let path = (rev_steps |> List.rev, cursor);
          inject(Update.Action.EditAction(MoveTo(path)));
        }),
      ],
      // TODO: Once we figure out contenteditable cursor use `Node.text("")`
      [Node.text(UnicodeConstants.zwsp)],
    );

  let whitespace =
      (
        ~on_click_target as (
          rev_steps: CursorPath.rev_steps,
          cursor: CursorPosition.t,
        ),
        n: int,
      ) => {
    let path = (rev_steps |> List.rev, cursor);
    Node.span(
      [
        contenteditable_false,
        Attr.on_click(_ => inject(Update.Action.EditAction(MoveTo(path)))),
      ],
      [Node.text(UnicodeConstants.nbsp |> StringUtil.replicat(n))],
    );
  };

  // whether `go` encounters a Layout.Linebreak
  let found_linebreak = ref(false);
  // the current column at the start of `l` on each call to `go`
  let column = ref(0);

  let mk_linebreak = (~rev_path_before, ~indent) => {
    found_linebreak := true;
    let last_col = column^;
    column := indent;
    [
      whitespace(~on_click_target=rev_path_before, width - last_col),
      Node.br([]),
    ];
  };

  let rec go =
          (~indent: int, ~rev_steps: CursorPath.rev_steps, l: TermLayout.t)
          : list(Vdom.Node.t) =>
    /* All DOM text nodes are expected to be wrapped in an
     * element either with contenteditable set to false or
     * annotated with the appropriate path-related metadata.
     * cf SelectionChange clause in Update.apply_action
     */
    switch (l) {
    | Text(s) =>
      column := column^ + StringUtil.utf8_length(s);
      [Node.text(s)];
    | Cat(Cat(l1, Linebreak), l2)
    | Cat(l1, Cat(Linebreak, l2)) =>
      let vs1 = l1 |> go(~indent, ~rev_steps);
      let vlinebreak = {
        let rev_path_before =
          l1
          |> TermLayout.rev_path_after
          |> OptUtil.get(() =>
               failwith(__LOC__ ++ ": expected to find path before Linebreak")
             );
        mk_linebreak(~rev_path_before, ~indent);
      };
      let vs2 = l2 |> go(~indent, ~rev_steps);
      List.concat([vs1, vlinebreak, vs2]);
    | Linebreak =>
      failwith(__LOC__ ++ ": expected to find a path near Linebreak")
    | Cat(l1, l2) =>
      let vs1 = l1 |> go(~indent, ~rev_steps);
      let vs2 = l2 |> go(~indent, ~rev_steps);
      vs1 @ vs2;
    | Align(l) => l |> go(~indent=column^, ~rev_steps)
    | Annot(Step(step), l) =>
      l |> go(~indent, ~rev_steps=[step, ...rev_steps])
    | Annot(annot, l) =>
      let vs = l |> go(~indent, ~rev_steps);
      switch (annot) {
      | Step(_) => assert(false)
      | Delim({index, _}) => [
          caret_position(rev_steps, OnDelim(index, Before)),
          Node.span(
            [
              Attr.classes(["code-delim"]),
              contenteditable_false,
              Attr.on_click(
                on_click_noneditable(
                  ~rev_steps,
                  ~cursor_before=OnDelim(index, Before),
                  ~cursor_after=OnDelim(index, After),
                ),
              ),
            ],
            vs,
          ),
          caret_position(rev_steps, OnDelim(index, After)),
        ]
      | Op(_) => [
          caret_position(rev_steps, OnOp(Before)),
          Node.span([contenteditable_false], vs),
          caret_position(rev_steps, OnOp(After)),
        ]
      | EmptyLine => [Node.span([Attr.classes(["EmptyLine"])], vs)]
      | SpaceOp => [
          Node.span([contenteditable_false, Attr.classes(["SpaceOp"])], vs),
        ]
      | Text({length, _}) => [
          Node.span(
            [
              Attr.on("move_caret_here", evt => {
                let path = (
                  rev_steps |> List.rev,
                  CursorPosition.OnText(Js.parseInt(evt##.on_text)),
                );
                inject(Update.Action.EditAction(MoveTo(path)));
              }),
              Attr.on_click(on_click_text(~rev_steps, ~length)),
            ],
            vs,
          ),
        ]
      | Padding => [
          Node.span([contenteditable_false, Attr.classes(["Padding"])], vs),
        ]
      | Indent => [
          Node.span([contenteditable_false, Attr.classes(["Indent"])], vs),
        ]
      | UserNewline => []
      | OpenChild(_)
      | ClosedChild(_)
      | HoleLabel(_)
      | DelimGroup
      | LetLine
      | Term(_) => vs
      };
    };
  let vs = {
    let vs = l |> go(~indent=0, ~rev_steps=[]);
    if (found_linebreak^) {
      vs;
    } else {
      let rev_path_before =
        l
        |> TermLayout.rev_path_after
        |> OptUtil.get(() =>
             failwith(__LOC__ ++ ": expected to find path before Linebreak")
           );
      vs @ [whitespace(~on_click_target=rev_path_before, width - column^)];
    };
  };
  Node.div(
    [
      Attr.id("contenteditable"),
      Attr.classes(
        ["code", "contenteditable"]
        @ (
          if (show_contenteditable) {
            [];
          } else {
            ["hiddencontenteditable"];
          }
        ),
      ),
      Attr.create("contenteditable", "true"),
      Attr.on("drop", _ => Event.Prevent_default),
      Attr.on_focus(_ => inject(Update.Action.FocusCell)),
      Attr.on_blur(_ => inject(Update.Action.BlurCell)),
    ],
    vs,
  );
};

let caret_position_of_path =
    ((steps, cursor) as path: CursorPath.t): (Js.t(Dom.node), int) =>
  switch (cursor) {
  | OnOp(side)
  | OnDelim(_, side) =>
    let anchor_parent = (
      JSUtil.force_get_elem_by_id(path_id(path)): Js.t(Dom_html.element) :>
        Js.t(Dom.node)
    );
    (
      Js.Opt.get(anchor_parent##.firstChild, () =>
        failwith(__LOC__ ++ ": Found caret position without child text")
      ),
      switch (side) {
      | Before => 1
      | After => 0
      },
    );
  | OnText(j) =>
    let anchor_parent = (
      JSUtil.force_get_elem_by_id(text_id(steps)): Js.t(Dom_html.element) :>
        Js.t(Dom.node)
    );
    (
      Js.Opt.get(anchor_parent##.firstChild, () =>
        failwith(__LOC__ ++ ": Found Text node without child text")
      ),
      j,
    );
  };

let presentation_of_layout = (l: TermLayout.t): Vdom.Node.t => {
  open Vdom;

  let rec go = (l: TermLayout.t): list(Node.t) =>
    switch (l) {
    | Text(str) => [Node.text(str)]
    | Cat(l1, l2) => go(l1) @ go(l2)
    | Linebreak => [Node.br([])]
    | Align(l) => [Node.div([Attr.classes(["Align"])], go(l))]

    // TODO adjust width to num digits, use visibility none
    | Annot(HoleLabel(_), l) => [
        Node.span([Attr.classes(["SEmptyHole-num"])], go(l)),
      ]

    | Annot(DelimGroup, l) => [
        Node.span([Attr.classes(["DelimGroup"])], go(l)),
      ]
    | Annot(LetLine, l) => [
        Node.span([Attr.classes(["LetLine"])], go(l)),
      ]
    | Annot(EmptyLine, l) => [
        Node.span([Attr.classes(["EmptyLine"])], go(l)),
      ]
    | Annot(Padding, l) => [
        Node.span(
          [contenteditable_false, Attr.classes(["Padding"])],
          go(l),
        ),
      ]
    | Annot(Indent, l) => [
        Node.span(
          [contenteditable_false, Attr.classes(["Indent"])],
          go(l),
        ),
      ]

    | Annot(UserNewline, l) => [
        Node.span([Attr.classes(["UserNewline"])], go(l)),
      ]

    | Annot(OpenChild({is_inline}), l) => [
        Node.span(
          [Attr.classes(["OpenChild", is_inline ? "Inline" : "Para"])],
          go(l),
        ),
      ]
    | Annot(ClosedChild({is_inline}), l) => [
        Node.span(
          [Attr.classes(["ClosedChild", is_inline ? "Inline" : "Para"])],
          go(l),
        ),
      ]

    | Annot(Delim({caret, _}), l) =>
      let children =
        switch (caret) {
        | None => go(l)
        | Some(side) => [caret_of_side(side), ...go(l)]
        };
      [Node.span([Attr.classes(["code-delim"])], children)];

    | Annot(Op({caret}), l) =>
      let children =
        switch (caret) {
        | None => go(l)
        | Some(side) => [caret_of_side(side), ...go(l)]
        };
      [Node.span([Attr.classes(["code-op"])], children)];

    | Annot(SpaceOp, l) => go(l)

    | Annot(Text({caret, length}), l) =>
      let children =
        switch (caret) {
        | None => go(l)
        | Some(char_index) =>
          let from_left =
            if (length == 0) {
              0.0;
            } else {
              let index = float_of_int(char_index);
              let length = float_of_int(length);
              100.0 *. index /. length;
            };
          [caret_from_left(from_left), ...go(l)];
        };
      [Node.span([Attr.classes(["code-text"])], children)];

    | Annot(Step(_), l) => go(l)

    | Annot(Term({has_cursor, shape, family}), l) => [
        Node.span(
          [
            Attr.classes(
              List.concat([
                ["Term"],
                cursor_clss(has_cursor),
                family_clss(family),
                shape_clss(shape),
                open_child_clss(
                  l |> TermLayout.has_inline_OpenChild,
                  l |> TermLayout.has_para_OpenChild,
                ),
                has_child_clss(l |> TermLayout.has_child),
              ]),
            ),
          ],
          go(l),
        ),
      ]
    };
  Node.div(
    [Attr.classes(["code", "presentation"]), contenteditable_false],
    go(l),
  );
};

let editor_view_of_layout =
    (
      ~inject: Update.Action.t => Vdom.Event.t,
      ~path: option(CursorPath.t)=?,
      ~ci: option(CursorInfo.t)=?,
      ~show_contenteditable: bool,
      ~width: int,
      l: TermLayout.t,
    )
    : (Vdom.Node.t, Vdom.Node.t) => {
  let l =
    switch (path) {
    | None => l
    | Some((steps, _) as path) =>
      switch (l |> TermLayout.find_and_decorate_caret(~path)) {
      | None =>
        JSUtil.log(
          Js.string(Sexplib.Sexp.to_string_hum(TermLayout.sexp_of_t(l))),
        );
        failwith(__LOC__ ++ ": could not find caret");
      | Some(l) =>
        switch (l |> TermLayout.find_and_decorate_cursor(~steps)) {
        | None => failwith(__LOC__ ++ ": could not find cursor")
        | Some(l) => l
        }
      }
    };
  let l =
    switch (ci) {
    | None
    | Some({uses: None, _}) => l
    | Some({uses: Some(uses), _}) =>
      uses
      |> List.fold_left(
           (l, use) =>
             l
             |> TermLayout.find_and_decorate_var_use(~steps=use)
             |> OptUtil.get(() => {
                  failwith(
                    __LOC__
                    ++ ": could not find var use"
                    ++ Sexplib.Sexp.to_string(CursorPath.sexp_of_steps(use)),
                  )
                }),
           l,
         )
    };
  (
    contenteditable_of_layout(~inject, ~width, ~show_contenteditable, l),
    presentation_of_layout(l),
  );
};

let view_of_htyp = (~width=30, ~pos=0, ty: HTyp.t): Vdom.Node.t => {
  let l =
    ty
    |> TermDoc.Typ.mk_htyp(~enforce_inline=false)
    |> LayoutOfDoc.layout_of_doc(~width, ~pos);
  switch (l) {
  | None => failwith("unimplemented: view_of_htyp on layout failure")
  | Some(l) => presentation_of_layout(l)
  };
};

let editor_view_of_exp =
    (
      ~inject: Update.Action.t => Vdom.Event.t,
      ~width=80,
      ~pos=0,
      ~path: option(CursorPath.t)=?,
      ~ci: option(CursorInfo.t)=?,
      ~show_contenteditable: bool,
      e: UHExp.t,
    )
    : (Vdom.Node.t, Vdom.Node.t) => {
  let l =
    e
    |> TermDoc.Exp.mk(~enforce_inline=false)
    |> LayoutOfDoc.layout_of_doc(~width, ~pos);
  switch (l) {
  | None => failwith("unimplemented: view_of_exp on layout failure")
  | Some(l) =>
    editor_view_of_layout(
      ~inject,
      ~width,
      ~path?,
      ~ci?,
      ~show_contenteditable,
      l,
    )
  };
};
