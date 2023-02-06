open Sexplib.Std;
open Util.OptUtil.Syntax;

/* The ids of a term's ancestors in the AST */
[@deriving (show({with_path: false}), sexp, yojson)]
type ancestors = list(Id.t);

/* SOURCE: Hazel type annotated with a relevant source location.
   Currently used to track match branches for inconsistent
   branches errors, but could perhaps be used more broadly
   for type debugging UI. */
[@deriving (show({with_path: false}), sexp, yojson)]
type source = {
  id: int,
  ty: Typ.t,
};

/* Self: The (synthetic) type information derivable from an expression
   of pattern term in isolation, using the typing context but not
   the syntactic context i.e. typing mode */
[@deriving (show({with_path: false}), sexp, yojson)]
type self =
  | Just(Typ.t) /* Just a regular type */
  | FreeVar //TODO: make exp-only; doesn't apply to pats
  | NoJoin(list(source)) /* inconsistent types for e.g match, listlits */
  | BadToken(Token.t) /* Invalid expression token, treated as hole */
  | IsMulti /* Multihole, treated as hole */
  | IsTag(Token.t, option(Typ.t)); /* Tags have special ana logic */

[@deriving (show({with_path: false}), sexp, yojson)]
type error_common =
  | BadToken(Token.t)
  | NoFun(Typ.t)
  | FreeVar
  | FreeTag
  | SynInconsistentBranches(list(Typ.t))
  | TypeInconsistent({
      ana: Typ.t,
      syn: Typ.t,
    });

/* Statics non-error classes */
[@deriving (show({with_path: false}), sexp, yojson)]
type ok_common =
  | SynConsistent(Typ.t)
  | AnaConsistent({
      ana: Typ.t,
      syn: Typ.t,
      join: Typ.t,
    })
  | AnaInternalInconsistent({
      ana: Typ.t,
      nojoin: list(Typ.t),
    });

[@deriving (show({with_path: false}), sexp, yojson)]
type status_common =
  | InHole(error_common)
  | NotInHole(ok_common);

/* Info for expression terms */
[@deriving (show({with_path: false}), sexp, yojson)]
type exp = {
  cls: Term.UExp.cls,
  term: Term.UExp.t,
  ancestors,
  ctx: Ctx.t,
  mode: Typ.mode,
  self,
  free: Ctx.co, /* _Locally_ unbound variables */
  ty: Typ.t,
};

/* Info for pattern terms */
[@deriving (show({with_path: false}), sexp, yojson)]
type pat = {
  cls: Term.UPat.cls,
  term: Term.UPat.t,
  ancestors,
  ctx: Ctx.t,
  mode: Typ.mode,
  self,
  ty: Typ.t,
};

[@deriving (show({with_path: false}), sexp, yojson)]
type status_variant =
  | Unique
  | Duplicate;

[@deriving (show({with_path: false}), sexp, yojson)]
type typ_mode =
  | TypeExpected
  | TagExpected(status_variant, Typ.t)
  | VariantExpected(status_variant, Typ.t);

/* A type can be either valid, a free type variable,
   or a duplicate tag. TODO: The additional errors statuses
   are fundamentally syntactic and should eventually
   be reimplemeted via a seperate sort */
[@deriving (show({with_path: false}), sexp, yojson)]
type error_typ =
  | BadToken(Token.t) /* Invalid token, treated as type hole */
  | FreeTypeVar(Token.t) /* Free type variable */
  | DuplicateTag(Token.t) /* Duplicate tag in same sum */
  | WantTypeFoundAp
  | WantTagFoundType(Typ.t)
  | WantTagFoundAp;

[@deriving (show({with_path: false}), sexp, yojson)]
type ok_typ =
  | Variant(Token.t, Typ.t)
  | VariantIncomplete(Typ.t)
  | TypeAlias(Token.t)
  | Type;

[@deriving (show({with_path: false}), sexp, yojson)]
type status_typ =
  | InHole(error_typ)
  | NotInHole(ok_typ);

[@deriving (show({with_path: false}), sexp, yojson)]
type typ = {
  cls: Term.UTyp.cls,
  term: Term.UTyp.t,
  ancestors,
  ctx: Ctx.t,
  mode: typ_mode,
  ty: Typ.t,
};

[@deriving (show({with_path: false}), sexp, yojson)]
type error_tpat =
  | NotAVar;

[@deriving (show({with_path: false}), sexp, yojson)]
type ok_tpat =
  | Empty
  | Var(Token.t);

[@deriving (show({with_path: false}), sexp, yojson)]
type status_tpat =
  | NotInHole(ok_tpat)
  | InHole(error_tpat);

[@deriving (show({with_path: false}), sexp, yojson)]
type tpat = {
  cls: Term.UTPat.cls,
  term: Term.UTPat.t,
  ancestors,
  ctx: Ctx.t,
};

/* The Info aka Cursorinfo assigned to each subterm. */
[@deriving (show({with_path: false}), sexp, yojson)]
type t =
  | InfoExp(exp)
  | InfoPat(pat)
  | InfoTyp(typ)
  | InfoTPat(tpat);

let ctx_of: t => Ctx.t =
  fun
  | InfoExp({ctx, _}) => ctx
  | InfoPat({ctx, _}) => ctx
  | InfoTyp({ctx, _}) => ctx
  | InfoTPat({ctx, _}) => ctx;

let exp_free: exp => Ctx.co = ({free, _}) => free;
let exp_ty: exp => Typ.t = ({ty, _}) => ty;
let pat_ctx: pat => Ctx.t = ({ctx, _}) => ctx;
let pat_ty: pat => Typ.t = ({ty, _}) => ty;

/* Strip location information from a list of sources */
let source_tys = List.map((source: source) => source.ty);

/* Determines whether an expression or pattern is in an error hole,
   depending on the mode, which represents the expectations of the
   surrounding syntactic context, and the self which represents the
   makeup of the expression / pattern itself. */
let rec status_common =
        (ctx: Ctx.t, mode: Typ.mode, self: self): status_common =>
  switch (self, mode) {
  | (BadToken(name), Syn | SynFun | Ana(_)) => InHole(BadToken(name))
  | (IsMulti, Syn | SynFun | Ana(_)) =>
    NotInHole(SynConsistent(Unknown(Internal)))
  | (Just(ty), Syn) => NotInHole(SynConsistent(ty))
  | (Just(ty), SynFun) =>
    switch (Typ.join(ctx, Arrow(Unknown(Internal), Unknown(Internal)), ty)) {
    | Some(_) => NotInHole(SynConsistent(ty))
    | None => InHole(NoFun(ty))
    }
  | (Just(syn), Ana(ana)) =>
    switch (Typ.join(ctx, ana, syn)) {
    | None => InHole(TypeInconsistent({syn, ana}))
    | Some(join) => NotInHole(AnaConsistent({ana, syn, join}))
    }
  | (FreeVar, _) => InHole(FreeVar)
  | (IsTag(name, syn_ty), _) =>
    /* If a tag is being analyzed against (an arrow type returning)
       a sum type having that tag as a variant, its self type is
       considered to be determined by the sum type; otherwise,
       check the context for the tag's type */
    switch (Typ.tag_ana_typ(ctx, mode, name), syn_ty) {
    | (Some(ana_ty), _) => status_common(ctx, mode, Just(ana_ty))
    | (_, Some(syn_ty)) => status_common(ctx, mode, Just(syn_ty))
    | _ => InHole(FreeTag)
    }
  | (NoJoin(tys), Syn | SynFun) =>
    InHole(SynInconsistentBranches(source_tys(tys)))
  | (NoJoin(tys), Ana(ana)) =>
    NotInHole(AnaInternalInconsistent({ana, nojoin: source_tys(tys)}))
  };

/* This logic determines whether a type should be put
   in a hole or not. It's mostly syntactic, determining
   the proper placement of sum type variants and tags;
   this should be reimplemented in the future as a
   separate sort. It also determines semantic properties
   such as whether or not a type variable reference is
   free, and whether a tag name is a dupe. */
let status_typ =
    (ctx: Ctx.t, mode: typ_mode, term: TermBase.UTyp.t): status_typ =>
  switch (term.term) {
  | Invalid(token) => InHole(BadToken(token))
  | EmptyHole => NotInHole(Type)
  | Var(name)
  | Tag(name) =>
    switch (mode) {
    | VariantExpected(Unique, sum_ty)
    | TagExpected(Unique, sum_ty) => NotInHole(Variant(name, sum_ty))
    | VariantExpected(Duplicate, _)
    | TagExpected(Duplicate, _) => InHole(DuplicateTag(name))
    | TypeExpected =>
      switch (Ctx.is_alias(ctx, name)) {
      | false => InHole(FreeTypeVar(name))
      | true => NotInHole(TypeAlias(name))
      }
    }
  | Ap(t1, t2) =>
    let ty_in = Term.UTyp.to_typ(ctx, t2);
    switch (mode) {
    | VariantExpected(status_variant, ty_variant) =>
      switch (status_variant, t1.term) {
      | (Unique, Var(name) | Tag(name)) =>
        NotInHole(Variant(name, Arrow(ty_in, ty_variant)))
      | _ => NotInHole(VariantIncomplete(Arrow(ty_in, ty_variant)))
      }
    | TagExpected(_) => InHole(WantTagFoundAp)
    | TypeExpected => InHole(WantTypeFoundAp)
    };
  | _ =>
    let ty = Term.UTyp.to_typ(ctx, term);
    switch (mode) {
    | TypeExpected => NotInHole(Type)
    | TagExpected(_)
    | VariantExpected(_) => InHole(WantTagFoundType(ty))
    };
  };

let status_tpat = (utpat: Term.UTPat.t): status_tpat =>
  switch (utpat.term) {
  | EmptyHole => NotInHole(Empty)
  | Var(name) => NotInHole(Var(name))
  | Invalid(_)
  | MultiHole(_) => InHole(NotAVar)
  };

/* Determines whether any term is in an error hole. */
let is_error = (ci: t): bool => {
  switch (ci) {
  | InfoExp({mode, self, ctx, _})
  | InfoPat({mode, self, ctx, _}) =>
    switch (status_common(ctx, mode, self)) {
    | InHole(_) => true
    | NotInHole(_) => false
    }
  | InfoTyp({mode, ctx, term, _}) =>
    switch (status_typ(ctx, mode, term)) {
    | InHole(_) => true
    | NotInHole(_) => false
    }
  | InfoTPat({term, _}) =>
    switch (status_tpat(term)) {
    | InHole(_) => true
    | NotInHole(_) => false
    }
  };
};

/* Determined the type of an expression or pattern 'after hole wrapping';
   that is, all ill-typed terms are considered to be 'wrapped in
   non-empty holes', i.e. assigned Unknown type. */
let typ_after_fix = (ctx, mode: Typ.mode, self: self): Typ.t =>
  switch (status_common(ctx, mode, self)) {
  | InHole(_) => Unknown(Internal)
  | NotInHole(SynConsistent(syn)) => syn
  | NotInHole(AnaConsistent({join, _})) => join
  | NotInHole(AnaInternalInconsistent({ana, _})) => ana
  };

let typ_after_fix_opt = (ctx, info: t): option(Typ.t) =>
  switch (info) {
  | InfoExp({mode, self, _})
  | InfoPat({mode, self, _}) => Some(typ_after_fix(ctx, mode, self))
  | InfoTyp(_)
  | InfoTPat(_) => None
  };

let syn_tag_typ = (ctx: Ctx.t, tag: Token.t): option(Typ.t) =>
  switch (Ctx.lookup_tag(ctx, tag)) {
  | None => None
  | Some({typ, _}) => Some(typ)
  };

let typ_of_self: (Ctx.t, self) => option(Typ.t) =
  ctx =>
    fun
    | Just(typ) => Some(typ)
    | IsTag(tag, _) => syn_tag_typ(ctx, tag)
    | FreeVar
    | BadToken(_)
    | IsMulti
    | NoJoin(_) => None;

let typ_of_self_info = (ctx: Ctx.t, info: t): option(Typ.t) =>
  switch (info) {
  | InfoExp({self, _})
  | InfoPat({self, _}) => typ_of_self(ctx, self)
  | InfoTyp(_)
  | InfoTPat(_) => None
  };

/* If the info represents some kind of name binding which
   exists in the context, return the id where the binding occurs */
let get_binding_site = (info: t): option(Id.t) => {
  switch (info) {
  | InfoExp({term: {term: Var(name) | Tag(name), _}, ctx, _})
  | InfoPat({term: {term: Tag(name), _}, ctx, _})
  | InfoTyp({term: {term: Var(name), _}, ctx, _}) =>
    let+ entry = Ctx.lookup(ctx, name);
    Ctx.get_id(entry);
  | _ => None
  };
};
