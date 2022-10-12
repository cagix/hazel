type t = Hashtbl.t(string, TermForm.t);

module P = Precedence;
module Exp = {
  open TermForm;

  let pre = pre(~sort=Exp);
  let bin = bin(~sort=Exp);

  let let_ = pre(~m=Kid.[pat(), exp()], ["let", "=", "in"], P.let_);

  let plus = bin(["+"], P.plus);
  let minus = bin(["-"], P.plus);
  let times = bin(["*"], P.mult);
  let divide = bin(["/"], P.mult);

  let lt = bin(["<"], P.eqs); //TODO: precedence
  let gt = bin([">"], P.eqs); //TODO: precedence
  let gte = bin(["<="], P.eqs);
  let lte = bin([">="], P.eqs);

  let fplus = bin(["+."], P.plus);
  let fminus = bin(["-."], P.plus);
  let ftimes = bin(["*."], P.mult);
  let fdivide = bin(["/."], P.mult);

  let flt = bin(["<."], 5); //TODO: precedence
  let fgt = bin([">."], 5); //TODO: precedence
  let fgte = bin(["<=."], P.eqs);
  let flte = bin([">=."], P.eqs);

  let eq_str = bin(["$=="], P.eqs);

  let all = [
    plus,
    minus,
    times,
    divide,
    lt,
    gt,
    gte,
    lte,
    fplus,
    fminus,
    ftimes,
    fdivide,
    flt,
    fgt,
    fgte,
    flte,
    let_,
  ];
};

// TODO turn this into a hash table
let tiles: list(list(ShardForm.t)) =
  [Exp.all] |> List.concat_map(((_name, form)) => TermForm.to_shards(form));

type ishard = (int, ShardForm.t);
let backpacks: Hashtbl.t(list(ShardForm.t), Backpack.t) = {
  let bps = Hashtbl.empty;
  let rec go = (ishards: list(ishard), ibp: Backpack.t(ishard)) => {
    let shards = List.map(snd, ishards);
    let bp = Backpack.map(snd, ibp);
    Hashtbl.add(bps, shards, bp);
    ListUtil.elem_splits(ishards)
    |> List.iter(((pre, (j, s), suf)) =>
         switch (ListUtil.split_last_opt(pre), suf) {
         | (None, []) => ()
         | (None, [(k, _), ..._]) =>
           let (l, m) = ListUtil.take_while(((k', _)) => k' < k, ibp.m);
           go(pre, {...bp, m, l: bp.l @ [(j, s), ...l]});
         | (Some((_, (i, _))), []) =>
           let (m_rev, r_rev) =
             List.rev(ibp.m) |> ListUtil.take_while(((i', _)) => i' > i);
           let m = List.rev(m_rev);
           let r = List.rev(r_rev);
           go(suf, {...bp, m, r: r @ [(j, s), ...bp.r]});
         | (Some(_), [_, ..._]) =>
           let m =
             [(j, s), ...bp.m]
             |> List.sort(((j, _), (j', _)) => Int.compare(j, j'));
           go(pre @ suf, {...bp, m});
         }
       );
  };

  tiles
  |> List.rev_map(List.mapi((i, s) => (i, s)))
  |> List.iter(ishards => go(ishards, Backpack.empty));
  bps;
};

let backpack = (t: list(ShardForm.t)): Backpack.t =>
  Hashtbl.find(backpacks, t);

let is_complete = (t: list(ShardForm.t)): bool =>
  Backpack.is_empty(backpack(t));
