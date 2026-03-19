// ox_to_js.h — Oxide → JavaScript Transpiler
//
// Strips Oxide type annotations from source code and emits valid JavaScript.
// Used by OxxPreprocessor to handle  client { }  blocks inside .oxx files.
//
// Transformations applied:
//   let x: int = 5         →  let x = 5
//   const y: string = "hi" →  const y = "hi"
//   function foo(a: int): string { ... }  →  function foo(a) { ... }
//   fn bar(): void { ... }               →  function bar() { ... }
//   print(x)               →  console.log(x)
//   for i in 0..10 { }     →  for (let i = 0; i < 10; i++) { }
//   import { ... }         →  (stripped)
//   interface / struct      →  (stripped)
//
// JavaScript APIs pass through unchanged:
//   console.log(...)  new Date()  Math.floor(x)  document.querySelector(...)

#pragma once
#include <string>
#include <vector>
#include "../lexer/token.h"

namespace oxide {

class OxToJs {
public:
  /// Transpile Oxide source to JavaScript. Returns the JS string.
  std::string transpile(const std::string& src);

private:
  std::vector<Token> toks_;
  size_t             idx_ = 0;
  std::string        out_;
  bool               needSpace_ = false; // emit a space before the next token?

  // ── Token access ────────────────────────────────────────────────
  const Token& cur()          const;
  const Token& peekAt(size_t n) const;
  void         advance();
  bool         atEnd()        const;

  // ── Emission helpers ────────────────────────────────────────────
  void emitText(const std::string& text);

  // ── Processing ──────────────────────────────────────────────────
  void processOne();

  /// Skip a single type name token (int/float/bool/string/void or identifier).
  void skipTypeName();

  /// Skip an entire { ... } block (used for struct / interface stripping).
  void skipBlock();
};

} // namespace oxide
