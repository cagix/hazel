open Sexplib.Std;
open Util;

module Atom = {
  [@deriving (show({with_path: false}), sexp, yojson, ord)]
  type t('sort) =
    | Tok(Token.Shape.t)
    | Kid('sort);
};

module Extremity = {
  [@deriving (show({with_path: false}), sexp, yojson, ord)]
  type t('sort) = option(Atom.t('sort));
  // across alternatives
  [@deriving (show({with_path: false}), sexp, yojson, ord)]
  type s('sort) = list(t('sort));
};

// maybe want to generalize over atoms
[@deriving (show({with_path: false}), sexp, yojson, ord)]
type t('sort) =
  | Atom(Atom.t('sort))
  | Star(t('sort))
  | Seq(list(t('sort)))
  | Alt(list(t('sort)));

// simplifying assumption, not fundamentally necessary
exception No_consecutive_tokens;

let tok_shape = (t: Token.Shape.t) => Atom(Tok(t));
let tok = (s: string) => tok_shape(Const(s));

let kid = s => Atom(Kid(s));

let seq = gs => Seq(gs);
let alt = gs => Alt(gs);

let eps = Seq([]);

let opt = g => Alt([eps, g]);

let rec map_atoms = f =>
  fun
  | Atom(a) => Atom(f(a))
  | Star(g) => Star(map_atoms(f, g))
  | Seq(gs) => Seq(List.map(map_atoms(f), gs))
  | Alt(gs) => Alt(List.map(map_atoms(f), gs));

let rec nullable =
  fun
  // assuming all sorts are non-nullable
  // but could change this in future
  | Atom(_) => false
  | Star(_) => true
  | Seq(gs) => List.for_all(nullable, gs)
  | Alt(gs) => List.exists(nullable, gs);

let rec root_token =
  fun
  | Atom(Kid(_)) => false
  | Atom(Tok(_)) => true
  | Star(_) => false
  | Seq(gs) => List.exists(root_token, gs)
  | Alt(gs) => List.for_all(root_token, gs);

// get exterior of head
let exterior_seq =
    (exterior: t(_) => Extremity.s(_), gs: list(t(_))): Extremity.s(_) => {
  let (nullable, rest) = ListUtil.take_while(nullable, gs);
  let nullable = List.concat_map(exterior, nullable);
  let rest =
    switch (rest) {
    | [] => [None]
    | [hd, ..._] => exterior(hd)
    };
  nullable @ rest;
};

let edge = _ => failwith("todo edge");

let rec exterior = (d: Dir.t, g: t(_)): Extremity.s(_) =>
  switch (g) {
  | Atom(a) => [Some(a)]
  | Star(g) => exterior(d, g)
  | Alt(gs) => List.concat_map(exterior(d), gs)
  | Seq(gs) => exterior_seq(edge(d), ListUtil.rev_if(d == R, gs))
  };

module Frame = {
  [@deriving (show({with_path: false}), sexp, yojson, ord)]
  type g('sort) = t('sort);
  [@deriving (show({with_path: false}), sexp, yojson, ord)]
  type t('sort) =
    | Star_
    | Seq_(list(g('sort)), list(g('sort)))
    | Alt_(list(g('sort)), list(g('sort)));
  [@deriving (show({with_path: false}), sexp, yojson, ord)]
  type s('sort) = list(t('sort));

  let empty = [];

  let rec interior = (d: Dir.t): (s(_) => Extremity.s(_)) =>
    fun
    | [] => [None]
    | [f, ...fs] =>
      switch (d, f) {
      | (_, Star_ | Alt_(_))
      | (L, Seq_([], _))
      | (R, Seq_(_, [])) => interior(d, fs)
      | (L, Seq_([_, ..._] as gs_l, _)) =>
        exterior_seq(exterior(R), gs_l)
        @ (List.for_all(nullable, gs_l) ? interior(d, fs) : [])
      | (R, Seq_(_, [_, ..._] as gs_r)) =>
        exterior_seq(exterior(L), gs_r)
        @ (List.for_all(nullable, gs_r) ? interior(d, fs) : [])
      };

  let cons = (~onto: Dir.t, g: g(_), fs: s(_)): s(_) =>
    switch (onto, fs) {
    | (L, [Seq_(l, r), ...fs]) => [Seq_(l @ [g], r), ...fs]
    | (R, [Seq_(l, r), ...fs]) => [Seq_(l, [g, ...r]), ...fs]
    | (L, _) => [Seq_([g], []), ...fs]
    | (R, _) => [Seq_([], [g]), ...fs]
    };
  let cons_seq = (~onto: Dir.t, gs: list(g(_)), fs: s(_)) => {
    let cons = cons(~onto);
    switch (onto) {
    | L => List.fold_left(Fun.flip(cons), fs, gs)
    | R => List.fold_right(cons, gs, fs)
    };
  };

  let nullable = (d: Dir.t): (s(_) => bool) =>
    List.for_all(f =>
      switch (d, f) {
      | (_, Star_ | Alt_(_)) => true
      | (L, Seq_(gs_l, _)) => nullable(Seq(gs_l))
      | (R, Seq_(_, gs_r)) => nullable(Seq(gs_r))
      }
    );

  let rec must_match = (d: Dir.t, fs: s(_)): bool =>
    switch (fs) {
    | [] => false
    | [f, ...fs] =>
      switch (d, f) {
      | (_, Star_ | Alt_(_))
      | (L, Seq_([], _))
      | (R, Seq_(_, [])) => must_match(d, fs)
      | (L, Seq_([_, ..._] as gs_l, _)) =>
        List.exists(root_token, gs_l) || must_match(d, fs)
      | (R, Seq_(_, [_, ..._] as gs_r)) =>
        List.exists(root_token, gs_r) || must_match(d, fs)
      }
    };
};

module Zipper = {
  type g('s) = t('s);
  // todo: make this t
  type a('s) = (Atom.t('s), Frame.s('s));
  type t('s) = (g('s), Frame.s('s));

  let t_of_a = ((a, fs): a(_)) => (Atom(a), fs);

  let zip = (g, f: Frame.t(_)) =>
    switch (f) {
    | Star_ => Star(g)
    | Alt_(l, r) => Alt(List.rev(l) @ [g, ...r])
    | Seq_(l, r) => Seq(List.rev(l) @ [g, ...r])
    };

  let rec enter =
          (~skip_nullable: bool, ~from: Dir.t, (g, fs): t(_)): list(a(_)) => {
    let go = enter(~skip_nullable, ~from);
    switch (g) {
    | Atom(a) => [(a, fs)]
    | Star(g) => skip_nullable ? [] : go((g, [Star_, ...fs]))
    | Alt(gs) =>
      ListUtil.elem_splits(gs)
      |> List.concat_map(((l, g, r)) => go((g, [Alt_(l, r), ...fs])))
    | Seq(gs) =>
      switch (from) {
      | L =>
        switch (gs) {
        | [] => []
        | [hd, ...tl] =>
          let go_hd = go((hd, Frame.cons_seq(~onto=R, gs, fs)));
          let go_tl =
            nullable(hd) ? go((Seq(tl), Frame.cons(~onto=L, hd, fs))) : [];
          go_hd @ go_tl;
        }
      | R =>
        switch (ListUtil.split_last_opt(gs)) {
        | None => []
        | Some((tl, hd)) =>
          let go_hd = go((hd, Frame.cons_seq(~onto=L, tl, fs)));
          let go_tl =
            nullable(hd) ? go((Seq(tl), Frame.cons(~onto=R, hd, fs))) : [];
          go_hd @ go_tl;
        }
      }
    };
  };

  let rec move_t =
          (~skip_nullable: bool, d: Dir.t, (g, fs): t(_)): list(a(_)) => {
    let go = move_t(~skip_nullable, d);
    let enter = enter(~skip_nullable);
    switch (fs) {
    | [] => []
    | [f, ...fs] =>
      switch (d, f) {
      | (_, Star_)
      | (_, Alt_(_)) => go((zip(g, f), fs))
      | (L, Seq_(l, r)) =>
        let enter_l = enter(~from=R, (Seq(l), [Seq_([], r), ...fs]));
        let go_beyond =
          List.for_all(nullable, l) ? go((zip(g, f), fs)) : [];
        enter_l @ go_beyond;
      | (R, Seq_(l, r)) =>
        let enter_r =
          enter(~from=L, (Seq(List.rev(l)), [Seq_([], r), ...fs]));
        let go_beyond =
          List.for_all(nullable, r) ? go((zip(g, f), fs)) : [];
        enter_r @ go_beyond;
      }
    };
  };
  let move = (~skip_nullable=false, d, a) =>
    move_t(~skip_nullable, d, t_of_a(a));

  let rec move_to_tok =
          (~skip_nullable=false, d: Dir.t, (a, fs): a(_)): list(_) => {
    let go = move_to_tok(~skip_nullable);
    let (found_now, found_later) =
      move(~skip_nullable, d, (a, fs))
      |> List.partition_map(((a: Atom.t(_), fs)) =>
           switch (a) {
           | Tok(t) => Left((Atom.Tok(t), fs))
           | Kid(_) => Right(go(d, (a, fs)))
           }
         );
    found_now @ List.concat(found_later);
  };

  let consistent = (_: a(_), _: a(_)) => failwith("todo consistent");
};
