// oxx_preprocessor.h — Oxide JSX Template Preprocessor
//
// Transforms JSX-like syntax in .oxx files into valid Oxide string expressions
// before the lexer/parser sees them.
//
// Input (.oxx):
//   function render(): string {
//     return (
//       <div class="card">
//         <h1>{title}</h1>
//         <p>{body}</p>
//       </div>
//     );
//   }
//
// Output (valid .ox):
//   function render(): string {
//     return (
//       "<div class=\"card\"><h1>" + (title) + "</h1><p>" + (body) + "</p></div>"
//     );
//   }
//
// Special Components:
//   <Router host="0.0.0.0" port={PORT}>
//     <Route method="GET"  path="/"    handler="render_index" />
//     <Route method="POST" path="/api" handler="handle_create" />
//   </Router>
//
// Compiles to:
//   router_get("/", "render_index");
//   router_post("/api", "handle_create");
//   router_start("0.0.0.0", PORT);

#pragma once
#include <string>
#include <vector>
#include <utility>
#include <map>
#include "ox_to_js.h"
#include "sfc_parser.h"

namespace oxide {

// A single segment of a JSX expression:
//   isDynamic=false  → raw HTML text (will be emitted as a quoted string literal)
//   isDynamic=true   → Oxide expression (will be wrapped in parens and concatenated)
using JsxPart = std::pair<bool, std::string>;

class OxxPreprocessor {
public:
  /// Transform source code. Detects SFC sections first; falls back to
  /// legacy JSX-only mode for files without section tags.
  std::string preprocess(const std::string& src);

private:
  // ── SFC assembly ────────────────────────────────────────────────────────
  // Assemble a parsed SfcFile into a complete Oxide source string, then
  // run that source through the JSX preprocessor.
  std::string assembleSfc(const SfcFile& sfc);

  // Split a <server> block's raw content into:
  //   imports   — "import { … } from "…";" lines → hoisted to module scope
  //   funcs     — function/fn declarations      → hoisted to module scope
  //   body      — everything else               → stays inside render()
  static void extractServerParts(const std::string& src,
                                  std::string& imports,
                                  std::string& funcs,
                                  std::string& body);

  // Split a <template> body at every <?oxide ... ?> boundary.
  // Returns alternating {isCode, content} pairs:
  //   isCode=false → HTML/JSX content (emit as <_> fragment)
  //   isCode=true  → raw Oxide code   (emit directly into render body)
  // Blank-only HTML segments are dropped.
  static std::vector<std::pair<bool,std::string>>
      splitOxideBlocks(const std::string& tmpl);

  // Parse a <client> section, separating reactive state declarations from
  // function/other code.
  //   state    — filled with { varname → initial_value_string } for each
  //              "let varname: type = value;" declaration
  //   fnCode   — remaining code (functions, other statements) for OxToJs
  static void extractClientState(const std::string& src,
                                  std::map<std::string, std::string>& state,
                                  std::string& fnCode);

  // ── JSX-only preprocessing (the original logic) ──────────────────────────
  // Called by preprocess() for legacy files, and by assembleSfc() to process
  // the assembled Oxide source (which may contain JSX in helper functions
  // and in the <template> section).
  std::string preprocessJsx(const std::string& src);

  // ── Internal state for preprocessJsx ────────────────────────────────────
  const std::string* src_ = nullptr;
  size_t pos_ = 0;
  std::string out_;

  // Reactive client-side state declarations from the <client> section.
  // Populated by assembleSfc() before preprocessJsx() runs.
  // Key = variable name, Value = initial value string (unquoted for numbers,
  // quoted for strings, so it can be embedded in ox-text spans).
  std::map<std::string, std::string> clientState_;

  // True when a '<' at the current position would open a JSX element rather
  // than act as the less-than comparison operator.
  // Set to true after: ( , = + - * / % ! & | [ { ; : newline  return let const
  // Set to false after: identifiers, numbers, ) ] }
  bool jsxOk_ = true;

  // ── Low-level character access ──────────────────────────────────
  char cur() const { return pos_ < src_->size() ? (*src_)[pos_] : '\0'; }
  char peek(size_t offset = 1) const {
    size_t p = pos_ + offset;
    return p < src_->size() ? (*src_)[p] : '\0';
  }
  char consume() { return (*src_)[pos_++]; }

  // ── Main processing loop ────────────────────────────────────────
  void processChar();

  // ── JSX element parser ──────────────────────────────────────────
  // Called when '<tagname' is detected. Converts the full element tree
  // (including nested children) into an Oxide string expression.
  std::string parseJsxElement();

  // Special handler for <Router port={...} host="..."> <Route .../> </Router>
  // Emits router_METHOD(path, handler) calls + router_start(host, port).
  std::string parseRouterComponent(const std::string& hostAttr,
                                    const std::string& portExpr);

  // Parse JSX children (text / {expr} / nested <elements>) until </tagName>.
  // Returns a list of JsxPart segments.
  std::vector<JsxPart> parseJsxChildren(const std::string& tagName);

  // Read the Oxide expression inside { ... }, handling nested braces.
  std::string readInterpolation();

  // Read attribute value: either "literal" or {expression}.
  // isExpr is set to true when the value was {expression}.
  std::string readAttrValue(bool& isExpr);

  // Skip spaces (not newlines)
  void skipSpaces();
  // Skip all whitespace including newlines
  void skipAllWs();

  // ── Output helpers ──────────────────────────────────────────────
  // Escape a raw string for use inside an Oxide double-quoted string literal.
  static std::string escapeForOxide(const std::string& s);

  // Join JsxParts into a single Oxide expression:
  //   static parts  → "escaped content"
  //   dynamic parts → (oxide_expression)
  //   all joined with ' + '
  // Adjacent static parts are merged before emission.
  static std::string partsToExpr(const std::vector<JsxPart>& parts);
};

} // namespace oxide
