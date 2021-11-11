open Virtual_dom.Vdom;
open Node;
let logo_panel =
  a(
    [Attr.classes(["logo-text"]), Attr.href("https://hazel.org")],
    [text("Hazel")],
  );

let top_bar = (~inject, ~model: Model.t) => {
  div(
    [Attr.classes(["top-bar"])],
    [
      logo_panel,
      CardsPanel.view(~inject, ~model),
      ActionMenu.view(~inject),
    ],
  );
};

let left_sidebar = (~inject, ~model: Model.t) =>
  Sidebar.left(~inject, ~is_open=model.left_sidebar_open, () =>
    [ActionPanel.view(~inject, model)]
  );

let right_sidebar =
    (
      ~inject: ModelAction.t => Event.t,
      ~model: Model.t,
      ~result as {assert_map, hii, _}: Result.t,
    ) => {
  let program = Model.get_program(model);
  Sidebar.right(~inject, ~is_open=model.right_sidebar_open, () =>
    [
      AssertPanel.view(~inject, ~model, ~assert_map),
      ContextInspector.view(
        ~inject,
        ~selected_instance=Model.get_selected_hole_instance(model),
        ~settings=model.settings.evaluation,
        ~font_metrics=model.font_metrics,
        ~hii,
        program,
      ),
      UndoHistoryPanel.view(~inject, model),
      SettingsPanel.view(~inject, model.settings),
    ]
  );
};

let git_panel = {
  let git_str =
    Printf.sprintf(
      "[%s @ %s (%s)]",
      Version_autogenerated.branch,
      Version_autogenerated.commit_hash_short,
      Version_autogenerated.commit_time,
    );
  span([Attr.class_("branch-panel")], [text(git_str)]);
};

let type_view = (ty: HTyp.t): Node.t => {
  let type_label =
    div([Attr.class_("label")], [text("Result of type: ")]);
  let type_view = div([Attr.class_("htype-view")], [HTypCode.view(ty)]);
  div([Attr.class_("type")], [type_label, type_view]);
};

let result_view = (~inject, ~model: Model.t, ~result: DHExp.t): Node.t =>
  div(
    [Attr.classes(["cell-result"])],
    [
      DHCode.view(
        ~inject,
        ~selected_instance=Model.get_selected_hole_instance(model),
        ~font_metrics=model.font_metrics,
        ~settings=model.settings.evaluation,
        ~width=80,
        result,
      ),
    ],
  );

let status_view =
    (~inject, ~model: Model.t, ~result as {result, result_ty, _}: Result.t)
    : Node.t =>
  div(
    [Attr.class_("cell-status")],
    model.settings.evaluation.evaluate
      ? [type_view(result_ty), result_view(~model, ~inject, ~result)] : [],
  );

let page = (~inject, ~model: Model.t, ~result: Result.t): Node.t => {
  let card_caption =
    div(
      [Attr.class_("card-caption")],
      [Model.get_card(model).info.caption],
    );
  div(
    [Attr.class_("page")],
    [
      card_caption,
      Cell.view(~inject, model),
      status_view(~model, ~inject, ~result),
    ],
  );
};

let view = (~inject, ~model: Model.t, ~result: Result.t): Node.t => {
  let page_area =
    div(
      [Attr.id(ViewUtil.page_area_id)],
      [page(~inject, ~model, ~result), git_panel],
    );
  let main_area =
    div(
      [Attr.id("main-area")],
      [
        left_sidebar(~inject, ~model),
        div([Attr.id("page-container")], [page_area]),
        right_sidebar(~inject, ~model, ~result),
      ],
    );
  div([Attr.id("root")], [top_bar(~inject, ~model), main_area]);
};
