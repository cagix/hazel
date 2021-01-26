module Vdom = Virtual_dom.Vdom;

let get_cursor_term_sort = (cursor_term: CursorInfo.cursor_term): TermSort.t => {
  switch (cursor_term) {
  | Exp(_, _)
  | ExpOp(_, _)
  | Line(_, _)
  | Rule(_, _) => Exp
  | Pat(_, _)
  | PatOp(_, _) => Pat
  | Typ(_, _)
  | TypOp(_, _) => Typ
  };
};

let term_tag_view =
    (tag: TermSort.t, ~show_tooltip: bool=false, add_classes: list(string))
    : Vdom.Node.t => {
  switch (tag) {
  | Exp =>
    let classes =
      Vdom.Attr.classes(["term-tag", "term-tag-exp", ...add_classes]);
    let attrs =
      show_tooltip
        ? [Vdom.Attr.create("title", "Expression"), classes] : [classes];
    Vdom.(Node.div(attrs, [Node.text("EXP")]));
  | Pat =>
    let classes =
      Vdom.Attr.classes(["term-tag", "term-tag-pat", ...add_classes]);
    let attrs =
      show_tooltip
        ? [Vdom.Attr.create("title", "Pattern"), classes] : [classes];
    Vdom.(Node.div(attrs, [Node.text("PAT")]));
  | Typ =>
    let classes =
      Vdom.Attr.classes(["term-tag", "term-tag-typ", ...add_classes]);
    let attrs =
      show_tooltip
        ? [Vdom.Attr.create("title", "Type"), classes] : [classes];
    Vdom.(Node.div(attrs, [Node.text("TYP")]));
  };
};
