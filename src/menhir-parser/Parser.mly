%{
open AST
%}



%token HOLE
%token FREE
%token FIX
%token <string> IDENT
%token <string> STRING
%token TRUE 
%token FALSE
%token <int> INT
%token <float> FLOAT
%token LET
%token FUN
%token CASE
%token AS
%token OPEN_BRACKET
%token CLOSE_BRACKET
%token OPEN_SQUARE_BRACKET
%token CLOSE_SQUARE_BRACKET
%token OPEN_PAREN
%token CLOSE_PAREN
%token DASH_ARROW
%token EQUAL_ARROW
%token SINGLE_EQUAL
%token TURNSTILE

(* Int ops *)
%token DOUBLE_EQUAL
%token NOT_EQUAL
%token PLUS
%token MINUS
%token DIVIDE
%token POWER
%token TIMES
%token LESS_THAN
%token LESS_THAN_EQUAL
%token GREATER_THAN
%token GREATER_THAN_EQUAL
(* Float ops *)
%token DOUBLE_EQUAL_FLOAT
%token NOT_EQUAL_FLOAT
%token PLUS_FLOAT
%token MINUS_FLOAT
%token DIVIDE_FLOAT
%token POWER_FLOAT
%token TIMES_FLOAT
%token LESS_THAN_FLOAT
%token LESS_THAN_EQUAL_FLOAT
%token GREATER_THAN_FLOAT
%token GREATER_THAN_EQUAL_FLOAT
(*logical ops*)
%token L_AND
%token L_OR
%token L_NOT
(*bitwise ops*)
%token B_AND

%token COMMA
%token COLON
%token EOF
%token IN
%token UNIT
%token END

(* type tokens *)
%token INT_TYPE
%token FLOAT_TYPE
%token BOOL_TYPE
%token STRING_TYPE
%token UNKNOWN
%token INTERNAL

%token IF
%token THEN
%token ELSE

%type <AST.exp> exp

%start <AST.exp> program

%%

program:
    | e = exp; EOF {e}

intOp:
    | PLUS { IntOp(Plus) }
    | MINUS { IntOp(Minus) }
    | TIMES { IntOp(Times) }
    | POWER { IntOp(Power) }
    | DIVIDE { IntOp(Divide) }
    | DOUBLE_EQUAL { IntOp(Equals) }
    | NOT_EQUAL { IntOp(NotEquals) }
    | LESS_THAN { IntOp(LessThan) }
    | LESS_THAN_EQUAL { IntOp(LessThanOrEqual) }
    | GREATER_THAN { IntOp(GreaterThan) }
    | GREATER_THAN_EQUAL { IntOp(GreaterThanOrEqual) }


floatOp:
    | PLUS_FLOAT { FloatOp(Plus) }
    | MINUS_FLOAT { FloatOp(Minus) }
    | TIMES_FLOAT { FloatOp(Times) }
    | POWER_FLOAT { FloatOp(Power) }
    | DIVIDE_FLOAT { FloatOp(Divide) }
    | DOUBLE_EQUAL_FLOAT { FloatOp(Equals) }
    | NOT_EQUAL_FLOAT { FloatOp(NotEquals) }
    | LESS_THAN_FLOAT { FloatOp(LessThan) }
    | LESS_THAN_EQUAL_FLOAT { FloatOp(LessThanOrEqual) }
    | GREATER_THAN_FLOAT { FloatOp(GreaterThan) }
    | GREATER_THAN_EQUAL_FLOAT { FloatOp(GreaterThanOrEqual) }

boolOp:
    | L_AND { BoolOp(And) }
    | L_OR { BoolOp(Or) }

binOp:
    | i = intOp { i }
    | f = floatOp { f }
    | b = boolOp { b }

binExp:
    | e1 = exp; b = binOp; e2 = exp { BinExp (e1, b, e2) }

typ:
    | INT_TYPE { IntType }
    | FLOAT_TYPE { FloatType }
    | BOOL_TYPE { BoolType }
    | STRING_TYPE { StringType }
    | UNKNOWN; INTERNAL { UnknownType(Internal) }
    | UNIT { UnitType }
    | OPEN_PAREN; types = separated_list(COMMA, typ); CLOSE_PAREN { TupleType(types) }
    | OPEN_SQUARE_BRACKET; t = typ; CLOSE_SQUARE_BRACKET { ArrayType(t) }
    | t1 = typ; DASH_ARROW; t2 = typ { ArrowType(t1, t2) }

varPat:
    | i = IDENT { VarPat (i) }

pat: 
    | p = varPat {p}
    | t = patTuple { t }
    | t = typeAnn { t }
    | i = INT { IntPat i }
    | f = FLOAT { FloatPat f }
    | s = STRING { StringPat s}
    | TRUE { BoolPat true}
    | FALSE {BoolPat false}
    (* | p1 = pat; AS; p2 = pat; { AsPat(p1, p2) } *)
    | f = pat; OPEN_PAREN; a = pat; CLOSE_PAREN { ApPat(f, a) }


patTuple: 
    | OPEN_PAREN; pats = separated_list(COMMA, pat); CLOSE_PAREN { TuplePat(pats) }

typeAnn: 
    | p = pat; COLON; t = typ { TypeAnn(p, t) }

rul:
    | TURNSTILE; p = pat; EQUAL_ARROW; e = exp; { (p, e) }

case:
    | CASE; e = exp; l = list(rul); END; { CaseExp(e, l) }
    | OPEN_PAREN; CASE; e = exp; l = list(rul); END; CLOSE_PAREN { InconsistentCaseExp(e, l)}

funExp: 
    | FUN; COLON; t = typ; p = pat; DASH_ARROW; e1 = exp;  { Fun (t, p, e1, None) }
    | FUN; COLON; t = typ; p = pat; DASH_ARROW; e1 = exp; name = IDENT { Fun (t, p, e1, Some(name)) }


ifExp:
    | IF; e1 = exp; THEN; e2 = exp; ELSE; e3 = exp { If (Consistent, e1, e2, e3) }
    | HOLE; IF; e1 = exp; THEN; e2 = exp; ELSE; e3 = exp { If (Inconsistent, e1, e2, e3) }

exp:
    | i = INT { Int i }
    | f = FLOAT { Float f }
    | v = IDENT { Var v }
    | FREE; v = IDENT { FreeVar v }
    | s = STRING { String s}
    | b = binExp { b }
    | OPEN_PAREN; e = exp; CLOSE_PAREN { e }
    | OPEN_PAREN; l = separated_list(COMMA, exp) ; CLOSE_PAREN { TupleExp(l)}
    | c = case { c }
    | OPEN_SQUARE_BRACKET; e = separated_list(COMMA, exp); CLOSE_SQUARE_BRACKET { ArrayExp(e) }
    | f = exp; OPEN_PAREN; a = exp; CLOSE_PAREN { ApExp(f, a) }
    | LET; i = pat; SINGLE_EQUAL; e1 = exp; IN; e2 = exp { Let (i, e1, e2) }
    | i = ifExp { i}
    | e1 = exp; LESS_THAN; t1 = typ; EQUAL_ARROW; t2 = typ; GREATER_THAN { Cast(e1, t1, t2) }
    | TRUE { Bool true }
    | FIX; s = IDENT; t = typ; f = funExp { FixF(s, t, f) }
    | f = funExp {f}
    | FALSE { Bool false }
    | OPEN_PAREN; HOLE; e = exp; CLOSE_PAREN {NonEmptyHole e}
    | HOLE { EmptyHole }
    (* | OPEN_PAREN; e = exp; CLOSE_PAREN { NonEmptyHole(e) } *)
    | UNIT { EmptyHole }
