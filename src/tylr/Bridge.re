open Util;

// constituent terraces are the bridge's "base"
[@deriving (show({with_path: false}), sexp, yojson)]
type t = (Terrace.R.t, Terrace.L.t);
// <let< x >=< _kid_ >in< 1
// -----------       ------

exception Convex_inner_tips;

let uncons = (~from_l, ~from_r, ~from: Dir.t, (l, r): t) =>
  switch (from) {
  | L =>
    let (l, s, a) = from_l(l);
    (a, Slope.(Dn.mk(~s, l), Up.of_terr(r)));
  | R =>
    let (a, s, r) = from_r(r);
    (a, Slope.(Dn.of_terr(l), Up.mk(~s, r)));
  };
let uncons_char =
  uncons(
    ~from_l=Terrace.R.unsnoc_lexeme(~char=true),
    ~from_r=Terrace.L.uncons_lexeme(~char=true),
  );
let uncons_lexeme =
  uncons(
    ~from_l=Terrace.R.unsnoc_lexeme(~char=false),
    ~from_r=Terrace.L.uncons_lexeme(~char=false),
  );

let mold = (~kid=?, t, (l, _)) => Terrace.R.mold(l, ~kid?, t);

let zip = ((l, r): t, kid: Meld.t) => Option.get(Terrace.eq(l, ~kid, r));

let unzip = (n, mel: Meld.t) => {
  open OptUtil.Syntax;
  let* (mel_l, wal, mel_r) = Wald.mk(mel);
  let+ (wal_l, kid, wal_r) =
    switch (Chain.split_nth_link(n, wal)) {
    | r => Some(r)
    | exception _ => None
    };
  let l = Terrace.{mel: mel_l, wal: wal_l};
  let r = Terrace.{wal: wal_r, mel: mel_r};
  (kid, (l, r));
};
