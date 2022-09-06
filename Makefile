HTML_DIR=$(shell pwd)/_build/default/src/haz3lweb/www
HTML_FILE=$(HTML_DIR)/index.html
SERVER="http://localhost:8000/"

all: dev

deps:
	opam switch import opam.export

change-deps:
	opam switch export opam.export

update-ocaml:
	opam update
	opam switch create 4.14 ocaml-base-compiler.4.14.0
	opam switch import opam.export --update-invariant

dev:
	dune build @src/fmt --auto-promote || true
	dune build src --profile dev

watch:
	dune build @src/fmt --auto-promote src --profile dev --watch

watch-release:
	dune build @src/fmt --auto-promote src --profile release --watch

release:
	dune build src --profile release

echo-html-dir:
	@echo "$(HTML_DIR)"

echo-html:
	@echo "$(HTML_FILE)"

win-chrome:
	wslpath -w $(HTML_FILE) | xargs -0 "/mnt/c/Program Files/Google/Chrome/Application/chrome.exe"

win-firefox:
	wslpath -w $(HTML_FILE) | xargs -0 "/mnt/c/Program Files/Mozilla Firefox/firefox.exe"

firefox:
	firefox "$(HTML_FILE)" &

chrome:
	chrome "$(HTML_FILE)" &

chrome-browser:
	chrome-browser "$(HTML_FILE)" &

chromium:
	chromium "$(HTML_FILE)" &

chromium-browser:
	chromium-browser "$(HTML_FILE)" &

xdg-open:
	xdg-open "$(HTML_FILE)"

open:
	open "$(HTML_FILE)"

serve:
	cd $(HTML_DIR); python3 -m http.server 8000

repl:
	dune utop src/hazelcore

test:
	dune build @src/fmt --auto-promote || true
	dune exec src/hazeltest/hazeltest.exe -- --regression-dir src/hazeltest/regressions

reset-regression-tests:
	dune exec src/hazeltest/hazeltest.exe -- regression --regression-dir src/hazeltest/regressions --reset-regressions

fix-test-answers:
	dune promote || true

clean:
	dune clean

.PHONY: all deps dev release echo-html-dir echo-html win-chrome win-firefox repl clean
