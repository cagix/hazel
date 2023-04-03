open Util;
open OptUtil.Syntax;

//TODO(andrew): PERF DANGER!!
let z_to_ci = (~ctx: Ctx.t, z: Zipper.t) => {
  let map =
    z
    |> Move.semantics_push
    |> Zipper.unselect_and_zip(~ignore_selection=true)
    |> MakeTerm.go
    |> fst
    |> Statics.mk_map_ctx(ctx);
  let* index = Indicated.index(z);
  Id.Map.find_opt(index, map);
};

let ctx_candidates = (ci: Info.t): list(string) => {
  let ctx = Info.ctx_of(ci);
  switch (ci) {
  | InfoExp({mode, _}) =>
    ctx |> Ctx.filtered_entries(~return_ty=true, Typ.of_mode(mode))
  | InfoPat({mode, _}) =>
    ctx |> Ctx.filtered_tag_entries(~return_ty=true, Typ.of_mode(mode))
  | InfoTyp(_) => Ctx.get_alias_names(ctx) @ Form.base_typs
  | _ => []
  };
};

let backpack_candidate = (sort: Sort.t, z: Zipper.t) =>
  switch (z.backpack) {
  | [] => []
  | [{content, _}, ..._] =>
    switch (content) {
    | [Tile({label, shards: [idx], mold, _})] when sort == mold.out => [
        List.nth(label, idx),
      ]
    | _ => []
    }
  };

let const_candidates = (ci: Info.t): list(string) =>
  switch (ci) {
  | InfoExp({mode: Ana(Bool | Unknown(_)) | Syn | SynFun, _}) => Form.bools
  | InfoPat({mode: Ana(Bool | Unknown(_)) | Syn | SynFun, _}) => Form.bools
  | InfoTyp(_) => Form.base_typs
  | _ => []
  };

let candidates = (~ctx: Ctx.t, z: Zipper.t): option(list(string)) => {
  let+ ci = z_to_ci(~ctx, z);
  let sort = Info.sort_of(ci);
  backpack_candidate(sort, z)
  @ Molds.leading_delims(sort)
  @ const_candidates(ci)
  @ ctx_candidates(ci);
};

/* Criteria: selection is ephemeral and a single monotile with the caret on the left,
   and the left sibling ends in a monotile, such that appending the two would result
   in a valid token */
let complete_criteria = (z: Zipper.t) =>
  switch (
    z.selection.focus,
    z.selection.ephemeral,
    z.selection.content,
    z.relatives.siblings |> fst |> List.rev,
    z.relatives.siblings |> snd,
  ) {
  | (
      Left,
      true,
      [Tile({label: [completion], _})],
      [Tile({label: [tok_to_left], _}), ..._],
      _,
    )
      when
        Form.is_valid_token(tok_to_left ++ completion)
        || String.sub(completion, String.length(completion) - 1, 1) == "("
        || String.sub(completion, String.length(completion) - 1, 1) == " " =>
    //TODO(andrew): second clause is hack see Ctx.re filtered_entries
    //TODO(andrew): third clause is also hack see Molds.re leading_delims
    Some(completion)
  | _ => None
  };

let left_of_mono = (z: Zipper.t) =>
  switch (
    z.relatives.siblings |> fst |> List.rev,
    z.relatives.siblings |> snd,
  ) {
  | ([Tile({label: [tok_to_left], _}), ..._], _) => Some(tok_to_left)
  | _ => None
  };

let mk_pseudotile = (id_gen: Id.t, z: Zipper.t, t: Token.t): (Id.t, Tile.t) => {
  let (id, id_gen) = IdGen.fresh(id_gen);
  let nibs = Siblings.fit_of(z.relatives.siblings);
  let mold: Mold.t = {out: Any, in_: [], nibs};
  //TODO(andrew): better sort than Any
  (id_gen, {id, label: [t], shards: [0], children: [], mold});
};

let add_ephemeral_selection = (z: Zipper.t, tile): Zipper.t => {
  ...z,
  selection: {
    ...z.selection,
    ephemeral: true,
    content: [Tile(tile)],
  },
};

let suffix_of = (candidate: Token.t, left: Token.t): option(Token.t) => {
  let candidate_suffix =
    String.sub(
      candidate,
      String.length(left),
      String.length(candidate) - String.length(left),
    );
  candidate_suffix == "" ? None : Some(candidate_suffix);
};

let mk_pseudoselection =
    (~ctx: Ctx.t, z: Zipper.t, id_gen: Id.t): option((Zipper.t, Id.t)) => {
  let* tok_to_left = left_of_mono(z);
  let* candidates = candidates(~ctx, z);
  //print_endline("CANDIDATES:\n" ++ (candidates |> String.concat("\n")));
  // a filtered candidate is a prefix match with at least one more char
  //TODO(andrew): need to escape tok_to_left, e.g. dots....
  let filtered_candidates =
    candidates |> List.filter(Form.regexp("^" ++ tok_to_left ++ "."));
  //print_endline("FILT:\n" ++ (filtered_candidates |> String.concat("\n")));
  let* top_candidate = filtered_candidates |> Util.ListUtil.hd_opt;
  let* candidate_suffix = suffix_of(top_candidate, tok_to_left);
  //print_endline("CANDIDATE: " ++ candidate_suffix);
  let (id, tile) = mk_pseudotile(id_gen, z, candidate_suffix);
  let z = add_ephemeral_selection(z, tile);
  Some((z, id));
};
