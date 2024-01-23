module Base = {
  [@deriving (show({with_path: false}), sexp, yojson, ord)]
  type t('t) =
    | Space
    | Grout
    | Tile('t);
};
include Base;

let is_space =
  fun
  | Space => true
  | _ => false;

module Label = {
  [@deriving (show({with_path: false}), sexp, yojson, ord)]
  type t = Base.t(Label.t);
  let zip = (l: t, r: t) =>
    switch (l, r) {
    | (Space, Space) => Space
    | (Grout, Grout) => Grout
    | (Tile(l), Tile(r)) => Tile(Label.zip(l, r))
    | _ => raise(Invalid_argument("Mtrl.Label.zip"))
    };
};
module Sort = {
  [@deriving (show({with_path: false}), sexp, yojson, ord)]
  type t = Base.t(Sort.t);
  let root = Tile(Sort.root);
  module Map =
    Map.Make({
      type nonrec t = t;
      let compare = compare;
    });
};

module Sym = {
  [@deriving (show({with_path: false}), sexp, yojson, ord)]
  type t = Sym.t(Label.t, Sort.t);
};
