open Virtual_dom.Vdom;
open Node;
open Haz3lcore;
open Util.Web;

type state = (Id.t, Editor.t);

let view =
    (
      ~inject,
      ~model as
        {
          editors,
          font_metrics,
          show_backpack_targets,
          settings,
          mousedown,
          langDocMessages,
          results,
          _,
        }: Model.t,
    ) => {
  let editor = Editors.get_editor(editors);
  let zipper = editor.state.zipper;
  let unselected = Zipper.unselect_and_zip(zipper);
  let (term, _) = MakeTerm.go(unselected);
  let info_map = Statics.mk_map(term);
  let result =
    settings.dynamics
      ? ModelResult.get_simple(
          ModelResults.lookup(results, ScratchSlide.scratch_key),
        )
      : None;
  let color_highlighting: option(ColorSteps.colorMap) =
    if (langDocMessages.highlight && langDocMessages.show) {
      Some(
        LangDoc.get_color_map(
          ~doc=langDocMessages,
          Indicated.index(zipper),
          info_map,
        ),
      );
    } else {
      None;
    };

  let code_id = "code-container";
  let editor_view =
    Cell.editor_with_result_view(
      ~inject,
      ~font_metrics,
      ~show_backpack_targets,
      ~clss=["single"],
      ~selected=true,
      ~mousedown,
      ~code_id,
      ~settings,
      ~color_highlighting,
      ~info_map,
      ~result,
      editor,
    );
  let bottom_bar =
    settings.statics
      ? [
        CursorInspector.view(
          ~inject,
          ~settings,
          ~show_lang_doc=langDocMessages.show,
          zipper,
          info_map,
        ),
      ]
      : [];
  let sidebar =
    langDocMessages.show && settings.statics
      ? LangDoc.view(
          ~inject,
          ~font_metrics,
          ~settings,
          ~doc=langDocMessages,
          Indicated.index(zipper),
          info_map,
        )
      : div([]);

  [
    div(
      ~attr=Attr.id("main"),
      [div(~attr=clss(["editor", "single"]), [editor_view])],
    ),
    sidebar,
  ]
  @ bottom_bar;
};

let download_slide_state = state => {
  let json_data = ScratchSlide.export(state);
  JsUtil.download_json("hazel-scratchpad", json_data);
};
let breadcrumb_bar =
    (
      ~inject,
      ~model as
        {
          editors,
          //font_metrics,
          //show_backpack_targets,
          settings,
          //mousedown,
          //langDocMessages,
          _,
          //results,
        }: Model.t,
    ) => {
  let editor = Editors.get_editor(editors);
  let zipper = editor.state.zipper;
  let unselected = Zipper.unselect_and_zip(zipper);
  let (term, _) = MakeTerm.go(unselected);
  let info_map = Statics.mk_map(term);
  switch (zipper.backpack, Indicated.index(zipper)) {
  | ([_, ..._], _) => [div([text("")])]
  | (_, None) => [div([text("")])]
  | (_, Some(id)) =>
    switch (Id.Map.find_opt(id, info_map)) {
    | None => [div([text("")])]
    | Some(ci) =>
      let ancestors = Info.ancestors_of(ci);
      let ancestors =
        List.map(
          a => {
            switch (Id.Map.find_opt(a, info_map)) {
            | Some(v) =>
              switch (v) {
              | Info.InfoExp({term, ancestors, _}) =>
                switch (term.term) {
                | Fun(_) => List.hd(ancestors)
                | _ => Uuidm.nil
                }
              | _ => Uuidm.nil
              }
            | None => Uuidm.nil
            }
          },
          ancestors,
        );
      let view_fun = (name, ids, level) => [
        div(
          ~attr=
            Attr.many([
              clss(["breadcrumb_bar_function"]),
              Attr.on_click(_ =>
                inject(Update.Set(Breadcrumb_bar(level, true)))
              ),
              Attr.on_click(_ =>
                inject(
                  UpdateAction.PerformAction(Jump(TileId(List.hd(ids)))),
                )
              ),
            ]),
          [text(name)],
        ),
      ];
      //if (List.exists(a => a == List.hd(ids), ancestors)) {
      //} else {
      //  [];
      //};
      let funs_display = Array.make(10, []);
      funs_display[0] = [];
      let rec tag_term = (term: TermBase.UExp.t, level: int) => {
        switch (term.term) {
        | Let({term: Var(name), _}, {term: Fun(_, fbody), _}, body) =>
          funs_display[level] =
            List.cons((name, term.ids), funs_display[level]);
          tag_term(body, level);
          tag_term(fbody, level + 1);
        | UnOp(_, t1)
        | Fun(_, t1)
        | TyAlias(_, _, t1)
        | Test(t1)
        | Parens(t1)
        | Ap(_, t1) => tag_term(t1, level)
        | Let(_, t1, t2)
        | Seq(t1, t2)
        | Cons(t1, t2)
        | ListConcat(t1, t2)
        | BinOp(_, t1, t2) =>
          tag_term(t1, level);
          tag_term(t2, level);
        | If(t1, t2, t3) =>
          tag_term(t1, level);
          tag_term(t2, level);
          tag_term(t3, level);
        | ListLit(t)
        | Tuple(t) => List.iter(t1 => tag_term(t1, level), t)
        | Match(t1, t) =>
          tag_term(t1, level);
          List.iter(t1 => tag_term(t1, level), List.map(snd, t));
        | _ => ()
        };
      };
      let () = tag_term(term, 1);
      let rec breadcrumb_funs = (level, res) =>
        if (level == 0) {
          res;
        } else if (funs_display[level] == []) {
          breadcrumb_funs(level - 1, res);
        } else {
          let toggle =
            List.concat(
              List.map(
                t =>
                  if (List.exists(a => a == List.hd(snd(t)), ancestors)) {
                    [text(fst(t))];
                  } else {
                    [];
                  },
                List.rev(funs_display[level]),
              ),
            );
          let siblings =
            List.concat(
              List.map(
                t => view_fun(fst(t), snd(t), level),
                List.rev(funs_display[level]),
              ),
            );

          let siblings_div = {
            let clss =
              clss(
                ["siblings"]
                @ (
                  List.nth(settings.breadcrumb_bars, level)
                    ? ["visible"] : []
                ),
              );
            div(~attr=clss, siblings);
          };
          let toggle_div = {
            [
              div(
                ~attr=
                  Attr.many([
                    Attr.on_click(_ =>
                      inject(Update.Set(Breadcrumb_bar(level, false)))
                    ),
                    clss(
                      ["toggle"]
                      @ (
                        List.nth(settings.breadcrumb_bars, level)
                          ? ["visible"] : []
                      ),
                    ),
                  ]),
                toggle @ [siblings_div],
              ),
            ];
          };
          let ret = toggle_div;
          let ret =
            if (res == []) {
              ret;
            } else if (toggle == []) {
              ret;
            } else {
              ret @ [text("->")] @ res;
            };
          breadcrumb_funs(level - 1, ret);
        };
      breadcrumb_funs(Array.length(funs_display) - 1, []);
    }
  };
};
let toolbar_buttons = (~inject, state: ScratchSlide.state) => {
  let export_button =
    Widgets.button(
      Icons.export,
      _ => {
        download_slide_state(state);
        Virtual_dom.Vdom.Effect.Ignore;
      },
      ~tooltip="Export Scratchpad",
    );
  let import_button =
    Widgets.file_select_button(
      "import-scratchpad",
      Icons.import,
      file => {
        switch (file) {
        | None => Virtual_dom.Vdom.Effect.Ignore
        | Some(file) => inject(UpdateAction.InitImportScratchpad(file))
        }
      },
      ~tooltip="Import Scratchpad",
    );

  let reset_button =
    Widgets.button(
      Icons.trash,
      _ => {
        let confirmed =
          JsUtil.confirm(
            "Are you SURE you want to reset this scratchpad? You will lose any existing code.",
          );
        if (confirmed) {
          inject(ResetCurrentEditor);
        } else {
          Virtual_dom.Vdom.Effect.Ignore;
        };
      },
      ~tooltip="Reset Scratchpad",
    );
  [export_button, import_button] @ [reset_button];
};
