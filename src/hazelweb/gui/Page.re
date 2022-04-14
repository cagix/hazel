open Virtual_dom.Vdom;
open Node;
let logo_panel =
  a(
    [Attr.classes(["logo-text"]), Attr.href("https://hazel.org")],
    [text("Hazel")],
  );

let branch_panel =
  span(
    [Attr.classes(["branch-panel"])],
    [
      text("["),
      text(Version_autogenerated.branch),
      text(" @ "),
      text(Version_autogenerated.commit_hash_short),
      text(" ("),
      text(Version_autogenerated.commit_time),
      text(")]"),
    ],
  );

let top_bar = (~inject: ModelAction.t => Ui_event.t, ~model: Model.t) => {
  div(
    [Attr.classes(["top-bar"])],
    [
      logo_panel,
      CardsPanel.view(~inject, ~model),
      ActionMenu.view(~inject),
    ],
  );
};

let cell_status_panel = (~settings: Settings.t, ~model: Model.t, ~inject) => {
  let program = Model.get_program(model);
  let selected_instance = {
    Model.(
      switch (get_selected_hole_instance(model)) {
      | Some((hole_inst, approach)) =>
        switch (approach) {
        | Exact => print_endline("exact_dhcode")
        | Nearest => print_endline("nearest_dhcode")
        };
        Some(hole_inst);
      | None => None
      }
    );
  };
  let (_, ty, _) = program.edit_state;
  let result =
    settings.evaluation.show_unevaluated_elaboration
      ? program |> Program.get_elaboration
      : program |> Program.get_result |> Result.get_dhexp;
  div(
    [],
    [
      div(
        [Attr.classes(["cell-status"])],
        [
          div(
            [Attr.classes(["type-indicator"])],
            [
              div(
                [Attr.classes(["type-label"])],
                [text("Result of type: ")],
              ),
              div([Attr.classes(["htype-view"])], [HTypCode.view(ty)]),
            ],
          ),
        ],
      ),
      div(
        [Attr.classes(["result-view"])],
        [
          DHCode.view(
            ~inject,
            ~selected_instance,
            ~settings=settings.evaluation,
            ~width=80,
            ~font_metrics=model.font_metrics,
            result,
          ),
        ],
      ),
    ],
  );
};

let left_sidebar = (~inject: ModelAction.t => Event.t, ~model: Model.t) =>
  Sidebar.left(~inject, ~is_open=model.left_sidebar_open, () =>
    [ActionPanel.view(~inject, model)]
  );

let right_sidebar = (~inject: ModelAction.t => Event.t, ~model: Model.t) => {
  let settings = model.settings;
  let program = Model.get_program(model);
  let selected_instance = Model.get_selected_hole_instance(model);
  Sidebar.right(~inject, ~is_open=model.right_sidebar_open, () =>
    [
      ContextInspector.view(
        ~inject,
        ~selected_instance,
        ~settings=settings.evaluation,
        ~font_metrics=model.font_metrics,
        program,
      ),
      UndoHistoryPanel.view(~inject, model),
      SettingsPanel.view(~inject, settings),
    ]
  );
};

let view = (~inject: ModelAction.t => Event.t, model: Model.t) => {
  let settings = model.settings;
  TimeUtil.measure_time(
    "Page.view",
    settings.performance.measure && settings.performance.page_view,
    () => {
      let card_caption = Model.get_card(model).info.caption;
      let cell_status =
        !settings.evaluation.evaluate
          ? div([], []) : cell_status_panel(~settings, ~model, ~inject);
      div(
        [Attr.id("root")],
        [
          top_bar(~inject, ~model),
          div(
            [Attr.classes(["main-area"])],
            [
              left_sidebar(~inject, ~model),
              div(
                [Attr.classes(["flex-wrapper"])],
                [
                  div(
                    [Attr.id("page-area")],
                    [
                      div(
                        [Attr.classes(["page"])],
                        [
                          div(
                            [Attr.classes(["card-caption"])],
                            [card_caption],
                          ),
                          Cell.view(~inject, model),
                          cell_status,
                        ],
                      ),
                      div(
                        [
                          Attr.style(
                            Css_gen.(
                              white_space(`Pre) @> font_family(["monospace"])
                            ),
                          ),
                        ],
                        [branch_panel],
                      ),
                    ],
                  ),
                ],
              ),
              right_sidebar(~inject, ~model),
            ],
          ),
        ],
      );
    },
  );
};
