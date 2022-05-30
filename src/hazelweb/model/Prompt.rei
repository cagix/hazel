// General type for a single example
type quest = {
  idz: string,
  expressionz: UHExp.t,
  caption: string,
  rankz: int,
  result: DHExp.t,
};

type explain = {
  id: string,
  expression: string,
  rank: int,
};

/*
 TODO: The centralized model should keep track of all of the different prompts (so add to the type t at the top of Model.re something like prompt: list(Prompt.t)
  */

type t = {
  key: string,
  program: ZExp.t,
  prompt_message: string,
  explanation: list(explain),
  examples: list(quest),
  explanation_text_box: string,
  example_text_box: string,
  syntactic_form_level: int,
  hovered_over_example: list(int),
};

let prompts: list(t);
let print_to_console: list(t) => string;
let update_explanation_rank: (t, int, int) => t;
let update_example_rank: (t, int, int) => t;
let update_explanation_text: (t, string) => t;
let update_example_text: (t, string) => t;
let update_example_hover: (t, int) => t;
