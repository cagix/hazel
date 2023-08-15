open Sexplib.Std;

[@deriving (show({with_path: false}), sexp, yojson)]
type all = {
  settings: string,
  explainThisModel: string,
  scratch: string,
  exercise: string,
  examples: string,
  log: string,
};

// fallback for saved state prior to release of lang doc in 490F22
[@deriving (show({with_path: false}), sexp, yojson)]
type all_f22 = {
  settings: string,
  scratch: string,
  exercise: string,
  log: string,
};

let mk_all = (~instructor_mode): all => {
  print_endline("Mk all");
  let settings = Store.Settings.export();
  print_endline("Settings OK");
  let explainThisModel = Store.ExplainThisModel.export();
  print_endline("ExplainThisModel OK");
  let scratch = Store.Scratch.export();
  print_endline("Scratch OK");
  let examples = Store.Examples.export();
  print_endline("Examples OK");
  let exercise =
    Store.Exercise.export(
      ~specs=ExerciseSettings.exercises,
      ~instructor_mode,
    );
  print_endline("Exercise OK");
  let log = Log.export();
  {settings, explainThisModel, scratch, examples, exercise, log};
};

let export_all = (~instructor_mode) => {
  mk_all(~instructor_mode) |> yojson_of_all;
};

let import_all = (data, ~specs) => {
  let all =
    try(data |> Yojson.Safe.from_string |> all_of_yojson) {
    | _ =>
      let all_f22 = data |> Yojson.Safe.from_string |> all_f22_of_yojson;
      {
        settings: all_f22.settings,
        scratch: all_f22.scratch,
        examples: "",
        exercise: all_f22.exercise,
        log: all_f22.log,
        explainThisModel: "",
      };
    };
  let settings = Store.Settings.import(all.settings);
  Store.ExplainThisModel.import(all.explainThisModel);
  let instructor_mode = settings.instructor_mode;
  Store.Scratch.import(all.scratch);
  Store.Exercise.import(all.exercise, ~specs, ~instructor_mode);
  Log.import(all.log);
};
