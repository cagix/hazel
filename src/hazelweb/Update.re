module Js = Js_of_ocaml.Js;
module Dom = Js_of_ocaml.Dom;
module Dom_html = Js_of_ocaml.Dom_html;
module Sexp = Sexplib.Sexp;
open Sexplib.Std;

[@deriving sexp]
type timestamp = {
  year: int,
  month: int,
  day: int,
  hours: int,
  minutes: int,
  seconds: int,
  milliseconds: int,
};

[@deriving sexp]
type timestamped_action = (timestamp, ModelAction.t);

let get_current_timestamp = (): timestamp => {
  let date = {
    %js
    new Js.date_now;
  };
  {
    year: date##getFullYear,
    month: date##getMonth,
    day: date##getDay,
    hours: date##getHours,
    minutes: date##getMinutes,
    seconds: date##getSeconds,
    milliseconds: date##getMilliseconds,
  };
};

let mk_timestamped_action = (a: ModelAction.t) => (
  get_current_timestamp(),
  a,
);

let log_action = (action: ModelAction.t, _: State.t): unit => {
  /* log interesting actions */
  switch (action) {
  | UpdateResult(_)
  | EditAction(_)
  | MoveAction(_)
  | ToggleLeftSidebar
  | ToggleRightSidebar
  | LoadCard(_)
  | LoadCardstack(_)
  | NextCard
  | PrevCard
  | UpdateSettings(_)
  | UpdateCursorInspector(_)
  | SelectHoleInstance(_)
  | SelectCaseBranch(_)
  | FocusCell
  | BlurCell
  | Undo
  | Redo
  | ShiftHistory(_)
  | ToggleHistoryGroup(_)
  | ToggleHiddenHistoryAll
  | TogglePreviewOnHover
  | UpdateFontMetrics(_)
  | SerializeToConsole(_)
  | Import(_)
  | LoadPermalink =>
    Logger.append(
      Sexp.to_string(
        sexp_of_timestamped_action(mk_timestamped_action(action)),
      ),
    )
  };
};

let apply_action =
    (model: Model.t, action: ModelAction.t, state: State.t, ~schedule_action)
    : Model.t => {
  switch (action) {
  | UpdateResult(current) =>
    /* If error, print a message. */
    switch (current) {
    | ResultFail(ProgramEvalError(reason)) =>
      let serialized =
        reason |> EvaluatorError.sexp_of_t |> Sexplib.Sexp.to_string_hum;
      JSUtil.log("[EvaluatorError.Exception(" ++ serialized ++ ")]");
    | ResultFail(ProgramDoesNotElaborate) =>
      JSUtil.log("[Program.DoesNotElaborate]")
    | _ => ()
    };
    /* Update result in model. */
    model |> Model.update_current_result(current);

  | EditAction(a) =>
    /* FIXME: Move movement exceptions catches to UpdateProgram. */
    switch (
      UpdateProgram.perform_edit_action(model, state, a, ~schedule_action)
    ) {
    | new_model => new_model
    | exception Program.FailedAction =>
      JSUtil.log("[Program.FailedAction]");
      model;
    | exception Program.CursorEscaped =>
      JSUtil.log("[Program.CursorEscaped]");
      model;
    | exception Program.MissingCursorInfo =>
      JSUtil.log("[Program.MissingCursorInfo]");
      model;
    }
  | MoveAction(Key(move_key)) =>
    switch (
      UpdateProgram.move_via_key(model, state, move_key, ~schedule_action)
    ) {
    | new_model => new_model
    | exception Program.CursorEscaped =>
      JSUtil.log("[Program.CursorEscaped]");
      model;
    }
  | MoveAction(Click(row_col)) =>
    let new_model =
      UpdateProgram.move_via_click(model, state, row_col, ~schedule_action);
    new_model;
  | ToggleLeftSidebar => Model.toggle_left_sidebar(model)
  | ToggleRightSidebar => Model.toggle_right_sidebar(model)
  | LoadCard(n) => Model.nth_card(n, model)
  | LoadCardstack(idx) => Model.load_cardstack(model, idx)
  | NextCard => Model.next_card(model)
  | PrevCard => Model.prev_card(model)
  | SelectHoleInstance(inst) => model |> Model.select_hole_instance(inst)
  | SelectCaseBranch(path_to_case, branch_index) =>
    let new_model =
      UpdateProgram.select_case_branch(
        model,
        state,
        path_to_case,
        branch_index,
        ~schedule_action,
      );
    new_model;
  | FocusCell => model |> Model.focus_cell
  | BlurCell => model |> Model.blur_cell
  | Undo =>
    let new_history =
      model.undo_history
      |> UndoHistory.shift_to_prev
      |> UndoHistory.update_disable_auto_scrolling(false);
    Model.load_undo_history(model, new_history, ~is_after_move=true);
  | Redo =>
    let new_history =
      model.undo_history
      |> UndoHistory.shift_to_next
      |> UndoHistory.update_disable_auto_scrolling(false);
    Model.load_undo_history(model, new_history, ~is_after_move=true);
  | ShiftHistory(shift_history_info) =>
    /* cshift to the certain entry */
    let new_history =
      model.undo_history
      |> UndoHistory.shift_history(
           shift_history_info.group_id,
           shift_history_info.elt_id,
           shift_history_info.call_by_mouseenter,
         );
    Model.load_undo_history(model, new_history, ~is_after_move=false);
  | ToggleHistoryGroup(toggle_group_id) =>
    let (suc_groups, _, _) = model.undo_history.groups;
    let cur_group_id = List.length(suc_groups);
    /* shift to the toggle-target group and change its expanded state */
    switch (ZList.shift_to(toggle_group_id, model.undo_history.groups)) {
    | None => failwith("Impossible match, because undo_history is non-empty")
    | Some(groups) =>
      let toggle_target_group = ZList.prj_z(groups);
      /* change expanded state of the toggle target group after toggling */
      let after_toggle =
        ZList.replace_z(
          {
            ...toggle_target_group,
            is_expanded: !toggle_target_group.is_expanded,
          },
          groups,
        );

      /*shift back to the current group*/
      switch (ZList.shift_to(cur_group_id, after_toggle)) {
      | None =>
        failwith("Impossible match, because undo_history is non-empty")
      | Some(new_groups) => {
          ...model,
          undo_history: {
            ...model.undo_history,
            groups: new_groups,
          },
        }
      };
    };
  | ToggleHiddenHistoryAll =>
    model
    |> Model.put_undo_history(
         UndoHistory.toggle_all_hidden_history(model.undo_history),
       )
  | TogglePreviewOnHover => {
      ...model,
      undo_history: {
        ...model.undo_history,
        preview_on_hover: !model.undo_history.preview_on_hover,
      },
    }
  | UpdateFontMetrics(metrics) => {...model, font_metrics: metrics}
  | UpdateSettings(u) => {
      ...model,
      settings: Settings.apply_update(u, model.settings),
    }
  | UpdateCursorInspector(u) => {
      ...model,
      cursor_inspector:
        CursorInspectorModel.apply_update(u, model.cursor_inspector),
    }
  | SerializeToConsole(obj) =>
    switch (obj) {
    | UHExp =>
      model
      |> Model.get_program
      |> Program.get_uhexp
      |> Serialization.string_of_exp
      |> Js.string
      |> JSUtil.log
    | DHExp =>
      switch (model |> Model.get_current_result |> ModelResult.get_current) {
      /* TODO: Print a message? */
      | ResultFail(_)
      | ResultTimedOut
      | ResultPending => ()
      | ResultOk(r) =>
        r
        |> ProgramResult.get_dhexp
        |> DHExp.sexp_of_t
        |> Sexplib.Sexp.to_string
        |> Js.string
        |> JSUtil.log
      }
    | ZExp =>
      model
      |> Model.get_program
      |> Program.get_zexp
      |> Serialization.string_of_zexp
      |> Js.string
      |> JSUtil.log
    };
    model;
  | Import(e) => Import.import(e, model)
  | LoadPermalink =>
    switch (Permalink.get_current()) {
    | Some(url) => model |> Permalink.put_model(url) |> Permalink.set_current
    | None => JSUtil.log("[Permalink.EmptyCurrent]")
    };
    model;
  };
};

let apply_action =
    (model: Model.t, action: ModelAction.t, state: State.t, ~schedule_action)
    : Model.t => {
  let settings = model.settings;
  if (settings.performance.measure) {
    Printf.printf("\n== Update.apply_action times ==\n");
  };
  TimeUtil.measure_time(
    "Update.apply_action",
    settings.performance.measure && settings.performance.update_apply_action,
    () => {
      log_action(action, state);
      apply_action(model, action, state, ~schedule_action);
    },
  );
};
