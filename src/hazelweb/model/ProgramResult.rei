/* The result of a program evaluation. Includes the
   final `DHExp.t`, the tracked hole closure information,
   and the evaluation state. Generated by Program.get_result.

   Not to be confused with `Evaluator.result`, which is a
   wrapper around `DHExp.t` that indicates what kind of
   final result it is (BoxedValue/Indet).

   The `EvaluatorState.t` component includes `EvaluatorStats.t`, which
   may contain evaluation statistics (e.g., number of evaluation
   steps) and may serve as an initial state to resume evaluation
   from a previous evaluation result in fill-and-resume.
   */
[@deriving sexp]
type t = (EvaluatorResult.t, EvaluatorState.t, HoleInstanceInfo.t);

let get_dhexp: t => DHExp.t;
let get_hole_instance_info: t => HoleInstanceInfo.t;
let get_state: t => EvaluatorState.t;

/**
  [fast_equal (r1, hii1, _, _) (r2, hii2, _, _) ] is checks if [hii1] and
  [hii2] are equal and computes [EvaluatorResult.fast_equal r1 r2].
 */
let fast_equal: (t, t) => bool;
