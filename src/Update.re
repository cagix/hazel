module Js = Js_of_ocaml.Js;
module Dom_html = Js_of_ocaml.Dom_html;
module EditAction = Action;
open Sexplib.Std;
open ViewUtil;
open SemanticsCommon;

module Action = {
  [@deriving sexp]
  type t =
    | EditAction(EditAction.t)
    | ToggleLeftSidebar
    | ToggleRightSidebar
    | LoadExample(Examples.id)
    | SelectHoleInstance(MetaVar.t, Dynamics.inst_num)
    | InvalidVar(string)
    | MoveToHole(MetaVar.t)
    | SelectionChange;
};

[@warning "-27"]
let apply_action =
    (model: Model.t, action: Action.t, _, ~schedule_action): Model.t =>
  switch (action) {
  | EditAction(a) => Model.perform_edit_action(model, a)
  | ToggleLeftSidebar => Model.toggle_left_sidebar(model)
  | ToggleRightSidebar => Model.toggle_right_sidebar(model)
  | LoadExample(id) => Model.load_example(model, Examples.get(id))
  | SelectHoleInstance(u, i) => Model.select_hole_instance(model, (u, i))
  | InvalidVar(x) => model
  | MoveToHole(u) => Model.move_to_hole(model, u)
  | SelectionChange =>
    let anchorNode = Dom_html.window##getSelection##.anchorNode;
    let anchorOffset = Dom_html.window##getSelection##.anchorOffset;
    let is_cursor_position = node =>
      switch (Js.Opt.to_option(Dom_html.CoerceTo.element(node))) {
      | None => None
      | Some(elem) =>
        let id = Js.to_string(elem##.id);
        switch (
          steps_of_text_id(id),
          path_of_path_id(id),
          steps_of_node_id(id),
        ) {
        | (None, None, None) => None
        | (Some(steps), _, _) => Some((steps, Some(OnText(anchorOffset))))
        | (_, Some((steps, cursor)), _) => Some((steps, Some(cursor)))
        | (_, _, Some(steps)) => Some((steps, None))
        };
      };
    if (JSUtil.div_contains_node(
          JSUtil.forceGetElementById("cell"),
          anchorNode,
        )) {
      let closest_elem =
        Js.Opt.get(Dom_html.CoerceTo.element(anchorNode), () =>
          switch (anchorNode##.nodeType) {
          | TEXT =>
            switch (Js.Opt.to_option(anchorNode##.parentNode)) {
            | None => assert(false)
            | Some(parent) =>
              Js.Opt.get(Dom_html.CoerceTo.element(parent), () =>
                assert(false)
              )
            }
          | _ => assert(false)
          }
        );
      if (JSUtil.elem_has_cls(closest_elem, "not-editable")) {
        (); // caret transport will trigger a second selectionchange
          // event, let second trigger do the work
      } else {
        let (zblock, _, _) = model.edit_state;
        let (current_steps, current_cursor) = Path.of_zblock(zblock);
        switch (anchorNode |> JSUtil.query_ancestors(is_cursor_position)) {
        | None => ()
        | Some((steps, None)) =>
          steps == current_steps
            ? () : schedule_action(Action.EditAction(MoveToBefore(steps)))
        | Some((steps, Some(cursor))) =>
          steps == current_steps && cursor == current_cursor
            ? ()
            : schedule_action(Action.EditAction(MoveTo((steps, cursor))))
        };
      };
    };
    model;
  };
