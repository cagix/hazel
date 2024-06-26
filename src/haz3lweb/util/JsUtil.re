open Js_of_ocaml;
open Virtual_dom.Vdom;
open Sexplib.Std;

let get_elem_by_id = id => {
  let doc = Dom_html.document;
  Js.Opt.get(
    doc##getElementById(Js.string(id)),
    () => {
      print_endline(id);
      assert(false);
    },
  );
};

let get_elem_by_selector = selector => {
  let doc = Dom_html.document;
  Js.Opt.get(
    doc##querySelector(Js.string(selector)),
    () => {
      print_endline(selector);
      assert(false);
    },
  );
};

let date_now = () => {
  [%js new Js.date_now];
};

let timestamp = () => date_now()##valueOf;

let get_key = evt =>
  Js.to_string(Js.Optdef.get(evt##.key, () => failwith("JsUtil.get_key")));

let ctrl_held = evt => Js.to_bool(evt##.ctrlKey);
let shift_held = evt => Js.to_bool(evt##.shiftKey);
let alt_held = evt => Js.to_bool(evt##.altKey);
let meta_held = evt => Js.to_bool(evt##.metaKey);

let num_clicks = (evt: Js.t(Js_of_ocaml.Dom_html.mouseEvent)): int =>
  Js.Unsafe.coerce(evt)##.detail;

let is_double_click = (evt: Js.t(Js_of_ocaml.Dom_html.mouseEvent)): bool =>
  num_clicks(evt) == 2;

let download_string_file =
    (~filename: string, ~content_type: string, ~contents: string) => {
  let blob = File.blob_from_string(~contentType=content_type, contents);
  let url = Dom_html.window##._URL##createObjectURL(blob);

  let link = Dom_html.createA(Dom_html.document);
  link##.href := url;
  link##setAttribute(Js.string("download"), Js.string(filename));
  link##.onclick := Dom_html.handler(_ => {Js._true});
  link##click;
};

let download_json = (filename, contents): unit =>
  download_string_file(
    ~filename=filename ++ ".json",
    ~content_type="application/json",
    ~contents=contents |> Yojson.Safe.to_string,
  );

let read_file = (file, k) => {
  let reader = [%js new File.fileReader];
  reader##readAsText(file);
  reader##.onload :=
    Dom.handler(_ => {
      let result = reader##.result;
      let option = Js.Opt.to_option(File.CoerceTo.string(result));
      let data = Option.map(Js.to_string, option);
      k(data);
      Js._true;
    });
};

let set_localstore = (k: string, v: string): unit => {
  let local_store =
    Js.Optdef.get(Dom_html.window##.localStorage, () => assert(false));
  local_store##setItem(Js.string(k), Js.string(v));
};

let get_localstore = (k: string): option(string) =>
  try({
    let local_store =
      Js.Optdef.get(Dom_html.window##.localStorage, () => assert(false));
    local_store##getItem(Js.string(k))
    |> (
      x => Js.Opt.get(x, () => assert(false)) |> Js.to_string |> Option.some
    );
  }) {
  | _ => None
  };

let clear_localstore = () => {
  let local_store =
    Js.Optdef.get(Dom_html.window##.localStorage, () => assert(false));
  local_store##clear;
};

let confirm = message => {
  Js.to_bool(Dom_html.window##confirm(Js.string(message)));
};

let log = data => {
  Firebug.console##log(data);
};

let clipboard_shim_id = "clipboard-shim";

let focus_clipboard_shim = () => get_elem_by_id(clipboard_shim_id)##focus;

let clipboard_shim = {
  Node.textarea(~attr=Attr.many([Attr.id(clipboard_shim_id)]), []);
};

let copy = (str: string) => {
  focus_clipboard_shim();
  Dom_html.document##execCommand(
    Js.string("selectAll"),
    Js.bool(false),
    Js.Opt.empty,
  );
  Dom_html.document##execCommand(
    Js.string("insertText"),
    Js.bool(false),
    Js.Opt.option(Some(Js.string(str))),
  );
  Dom_html.document##execCommand(
    Js.string("selectAll"),
    Js.bool(false),
    Js.Opt.empty,
  );
};

let scroll_cursor_into_view_if_needed = () =>
  try({
    let caret_elem = get_elem_by_id("caret");
    let main = get_elem_by_id("main");
    let main_rect = main##getBoundingClientRect;
    let caret_rect = caret_elem##getBoundingClientRect;

    if (caret_rect##.top < main_rect##.top) {
      caret_elem##scrollIntoView(Js._true);
    } else if (caret_rect##.bottom > main_rect##.bottom) {
      caret_elem##scrollIntoView(Js._false);
    };
  }) {
  | Assert_failure(_) => ()
  };

module Fragment = {
  let set_current = frag => {
    let frag =
      switch (frag) {
      | "" => ""
      | frag => "#" ++ frag
      };
    let history = Js_of_ocaml.Dom_html.window##.history;
    history##pushState(Js.null, Js.string(""), Js.some(Js.string(frag)));
  };

  let get_current = () => {
    let fragment_of_url = (url: Url.url): string =>
      switch (url) {
      | Http({hu_fragment: str, _})
      | Https({hu_fragment: str, _})
      | File({fu_fragment: str, _}) => str
      };
    Url.Current.get() |> Option.map(fragment_of_url);
  };
};

let stop_mousedown_propagation =
  Attr.on_mousedown(evt => {
    Js_of_ocaml.Dom_html.stopPropagation(evt);
    Virtual_dom.Vdom.Effect.Ignore;
  });

[@deriving (show({with_path: false}), sexp, yojson)]
type text_position = {
  row: int,
  col: int,
};

let get_textarea = (selector: string): Js.t(Dom_html.textAreaElement) =>
  Dom_html.CoerceTo.textarea(get_elem_by_selector(selector))
  |> Js.Opt.get(_, _ => failwith("get_caret_text_position"));

let get_textarea_lines =
    (textarea: Js.t(Dom_html.textAreaElement)): list(string) => {
  let text_content = Js.to_string(textarea##.value);
  Str.split(Str.regexp("\n"), text_content);
};

let textarea_max_row = (selector: string): int => {
  let textarea = get_textarea(selector);
  let lines = get_textarea_lines(textarea);
  List.length(lines) - 1;
};

let textarea_caret_position = (selector: string): text_position => {
  let textarea = get_textarea(selector);
  let lines = get_textarea_lines(textarea);
  let caret_position = textarea##.selectionStart;
  let rec find_position = (lines, current_position, row, col) => {
    switch (lines) {
    | [] => {row, col}
    | [line, ...rest] =>
      let line_length = String.length(line); // +1 for the newline
      if (current_position <= line_length) {
        {row, col: current_position};
      } else {
        find_position(rest, current_position - line_length - 1, row + 1, 0);
      };
    };
  };
  find_position(lines, caret_position, 0, 0);
};

[@deriving (show({with_path: false}), sexp, yojson)]
type position_type =
  | First
  | Middle
  | Last;

[@deriving (show({with_path: false}), sexp, yojson)]
type caret_position = {
  rows: position_type,
  cols: position_type,
};

let boundary_pos = (current, max) =>
  if (current == 0) {
    First;
  } else if (current == max) {
    Last;
  } else {
    Middle;
  };

let textarea_caret_rel_pos = (selector: string): caret_position => {
  let lines = selector |> get_textarea |> get_textarea_lines;
  let {row, col} = textarea_caret_position(selector);
  {
    rows: boundary_pos(row, List.length(lines) - 1),
    cols: boundary_pos(col, String.length(List.nth(lines, row))),
  };
};

let set_textarea_caret_to_end = (selector: string): unit => {
  let textarea = get_textarea(selector);
  // Focus the textarea
  textarea##focus;
  // Set the selection start (and implicitly, the end) to the length of the content
  let content_length = String.length(Js.to_string(textarea##.value));
  textarea##.selectionStart := content_length;
  textarea##.selectionEnd := content_length;
};
