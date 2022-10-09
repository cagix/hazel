open Sexplib.Std;
open Util;

module Kid = {
  [@deriving (show({with_path: false}), sexp, yojson)]
  type t = {
    pad: bool,
    sort: Sort.t,
  };

  let pat = (~pad=true, ()) => {pad, sort: Pat};

  let exp = (~pad=true, ()) => {pad, sort: Exp};
};

// module Kids = {
//   [@deriving (show({with_path: false}), sexp, yojson)]
//   type t = (option(Kid.t), list(Kid.t), option(Kid.t));

//   let m_length = ((_, m, _)) => List.length(bi);
// };

module Mold = {
  [@deriving (show({with_path: false}), sexp, yojson)]
  type t = {
    prec: Precedence.t,
    sort: Sort.t,
    kids: (option(Kid.t), list(Kid.t), option(Kid.t)),
  };

  let mk =
      (
        ~l: option(Kid.t)=?,
        ~m: list(Kid.t)=[],
        ~r: option(Kid.t)=?,
        sort: Sort.t,
        prec: Precedence.t,
      ) => {
    prec,
    sort,
    kids: (l, m, r),
  };

  let op = (~m=[], sort) => mk(~m, sort);
  let pre = (~m=[], r, sort) => mk(~m, ~r, sort);
  let post = (l, ~m=[], sort) => mk(~l, ~m, sort);
  let bin = (l, ~m=[], r) => mk(~l, ~m, ~r);
};

[@deriving (show({with_path: false}), sexp, yojson)]
type t = {
  // invariant: List.length(label) >= 1
  label: list(Token.t),
  // invariant: List.length(label) == Kids.m_length(mold.kids) + 1
  mold: Mold.t,
};

let pre =
    (~sort: Sort.t, ~label: list(Token.t), ~bi: list(Kid.t)=[], r: Kid.t) => {
  assert(List.length(label) == List.length(bi) + 1);
  {sort, label, bi_kids: bi, uni_kids: (None, Some(r))};
};

let post =
    (~sort: Sort.t, ~label: list(Token.t), ~bi: list(Kid.t)=[], l: Kid.t) => {
  assert(List.length(label) == List.length(bi) + 1);
  {sort, label, bi_kids: bi, uni_kids: (Some(l), None)};
};

let let_ =
  pre(~sort=Exp, ~label=["let", "=", "in"], ~bi=Kid.[pat(), exp()], exp());

let all = [let_];
