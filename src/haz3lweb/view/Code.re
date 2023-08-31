open Virtual_dom.Vdom;
open Node;
open Haz3lcore;
open Util;
open Util.Web;

let of_delim' =
  Core.Memo.general(
    ~cache_size_bound=10000,
    ((is_in_buffer, sort, is_consistent, is_complete, label, i)) => {
      let cls =
        switch (label) {
        | _ when is_in_buffer => "in-buffer"
        | _ when !is_consistent => "sort-inconsistent"
        | _ when !is_complete => "incomplete"
        | [s] when s == Form.explicit_hole => "explicit-hole"
        | [s] when Form.is_string(s) => "string-lit"
        | _ => "default"
        };
      let plurality = List.length(label) == 1 ? "mono" : "poly";
      [
        span(
          ~attr=
            Attr.classes(["token", cls, Sort.to_string(sort), plurality]),
          [Node.text(List.nth(label, i))],
        ),
      ];
    },
  );
let of_delim =
    (is_in_buffer, is_consistent, t: Piece.tile, i: int): list(Node.t) =>
  of_delim'((
    is_in_buffer,
    t.mold.out,
    is_consistent,
    Tile.is_complete(t),
    t.label,
    i,
  ));

let of_grout = [Node.text(Unicode.nbsp)];

let of_secondary =
  Core.Memo.general(
    ~cache_size_bound=10000, ((secondary_icons, indent, content)) =>
    if (String.equal(Secondary.get_string(content), Form.linebreak)) {
      let str = secondary_icons ? Form.linebreak : "";
      [
        span_c("linebreak", [text(str)]),
        Node.br(),
        Node.text(StringUtil.repeat(indent, Unicode.nbsp)),
      ];
    } else if (String.equal(Secondary.get_string(content), Form.space)) {
      let str = secondary_icons ? "·" : Unicode.nbsp;
      [span_c("secondary", [text(str)])];
    } else if (Secondary.content_is_comment(content)) {
      [span_c("comment", [Node.text(Secondary.get_string(content))])];
    } else {
      [span_c("secondary", [Node.text(Secondary.get_string(content))])];
    }
  );

/* PERF: Tile memoization makes a >2X difference. I've left
   the memoization in place for delims and secondary above as it still
   seems like a marginal positive (5-10% difference).

   WARNING: Note that this the table is stored outside the Text functor.
   This means that if there are data dependencies on the functor argument
   values, they will need to be explictly encoded in the key.

   TODO: Consider setting a limit for the hashtbl size  */
let piece_hash:
  Hashtbl.t(
    (Piece.t, list(Uuidm.t), Id.Map.t(int), Sort.t, bool),
    list(t),
  ) =
  Hashtbl.create(10000);

module Text = (M: {
                 let map: Measured.t;
                 let settings: Settings.t;
               }) => {
  let m = p => Measured.find_p(p, M.map);
  let rec of_segment =
          (
            seg: Segment.t,
            ~indent_level=Indentation.level_map(seg),
            buffer_ids,
            no_sorts,
            sort,
          )
          : list(Node.t) => {
    /* note: no_sorts flag is used for backback view;
       otherwise Segment.expected_sorts call crashes for some reason */
    let expected_sorts =
      no_sorts
        ? List.init(List.length(seg), i => (i, Sort.Any))
        : Segment.expected_sorts(sort, seg);
    let sort_of_p_idx = idx =>
      switch (List.assoc_opt(idx, expected_sorts)) {
      | None => Sort.Any
      | Some(sort) => sort
      };
    seg
    |> List.mapi((i, p) => (i, p))
    |> List.concat_map(((i, p)) =>
         of_piece(indent_level, buffer_ids, sort_of_p_idx(i), p)
       );
  }
  and of_piece' =
      (
        (
          p: Piece.t,
          buffer_ids,
          indent_level: Id.Map.t(int),
          expected_sort: Sort.t,
          secondary_icons: bool,
        ),
      )
      : list(Node.t) => {
    switch (p) {
    | Tile(t) => of_tile(indent_level, buffer_ids, expected_sort, t)
    | Grout(_) => of_grout
    | Secondary({content, _} as w) when Secondary.is_linebreak(w) =>
      let indent =
        switch (Id.Map.find_opt(Piece.id(p), indent_level)) {
        | None =>
          print_endline("WARNING: indent level not found");
          m(p).last.col;
        | Some(i) => i
        };
      of_secondary((secondary_icons, indent, content));
    | Secondary({content, _}) => of_secondary((secondary_icons, 0, content))
    };
  }
  and of_piece =
      (indent_level, buffer_ids, expected_sort: Sort.t, p: Piece.t)
      : list(Node.t) => {
    let secondary_icons = M.settings.secondary_icons;
    let arg = (p, buffer_ids, indent_level, expected_sort, secondary_icons);
    try(Hashtbl.find(piece_hash, arg)) {
    | _ =>
      let res = of_piece'(arg);
      Hashtbl.add(piece_hash, arg, res);
      res;
    };
  }
  and of_tile =
      (indent_level, buffer_ids, expected_sort: Sort.t, t: Tile.t)
      : list(Node.t) => {
    let children_and_sorts =
      List.mapi(
        (i, (l, child, r)) =>
          //TODO(andrew): more subtle logic about sort acceptability
          (child, l + 1 == r ? List.nth(t.mold.in_, i) : Sort.Any),
        Aba.aba_triples(Aba.mk(t.shards, t.children)),
      );
    let is_consistent = Sort.consistent(t.mold.out, expected_sort);
    let is_in_buffer = List.mem(t.id, buffer_ids);
    Aba.mk(t.shards, children_and_sorts)
    |> Aba.join(of_delim(is_in_buffer, is_consistent, t), ((seg, sort)) =>
         of_segment(~indent_level, seg, buffer_ids, false, sort)
       )
    |> List.concat;
  };
};

let rec holes =
        (~font_metrics, ~map: Measured.t, seg: Segment.t): list(Node.t) =>
  seg
  |> List.concat_map(
       fun
       | Piece.Secondary(_) => []
       | Tile(t) => List.concat_map(holes(~map, ~font_metrics), t.children)
       | Grout(g) => [
           EmptyHoleDec.view(
             ~font_metrics, // TODO(d) fix sort
             {
               measurement: Measured.find_g(g, map),
               mold: Mold.of_grout(g, Any),
             },
           ),
         ],
     );

let simple_view = (~unselected, ~map, ~settings: Settings.t): Node.t => {
  module Text =
    Text({
      let map = map;
      let settings = settings;
    });
  div(
    ~attr=Attr.class_("code"),
    [span_c("code-text", Text.of_segment(unselected, [], false, Sort.Any))],
  );
};

let view =
    (
      ~sort: Sort.t,
      ~font_metrics: FontMetrics.t,
      ~indent_level: Id.Map.t(int),
      ~buffer_ids: list(Uuidm.t),
      ~segment: Segment.t,
      ~unselected: Segment.t,
      ~measured: Measured.t,
      ~settings: Settings.t,
    )
    : Node.t => {
  module Text =
    Text({
      let map = measured;
      let settings = settings;
    });
  let unselected =
    TimeUtil.measure_time("Code.view/unselected", settings.benchmark, () =>
      Text.of_segment(~indent_level, unselected, buffer_ids, false, sort)
    );
  let holes =
    TimeUtil.measure_time("Code.view/holes", settings.benchmark, () =>
      holes(~map=measured, ~font_metrics, segment)
    );
  div(
    ~attr=Attr.class_("code"),
    [
      span_c("code-text", unselected),
      // TODO restore (already regressed so no loss in commenting atm)
      // span_c("code-text-shards", Text.of_segment(segment)),
      ...holes,
    ],
  );
};
