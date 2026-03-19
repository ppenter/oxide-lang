// ox_to_js.cpp — Oxide → JavaScript Transpiler Implementation

#include "ox_to_js.h"
#include "../lexer/lexer.h"
#include <cctype>
#include <sstream>

namespace oxide {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static bool isTypeKeyword(TokenKind k) {
  return k == TokenKind::KW_INT    ||
         k == TokenKind::KW_FLOAT  ||
         k == TokenKind::KW_BOOL   ||
         k == TokenKind::KW_STRING ||
         k == TokenKind::KW_VOID;
}

// Is this a token that starts a new "word" (needs a space if the prev was also word-like)?
static bool isWordLike(TokenKind k) {
  switch (k) {
  case TokenKind::IDENTIFIER:
  case TokenKind::INT_LITERAL:
  case TokenKind::FLOAT_LITERAL:
  case TokenKind::BOOL_LITERAL:
  case TokenKind::STRING_LITERAL:
  case TokenKind::KW_LET:
  case TokenKind::KW_CONST:
  case TokenKind::KW_FN:
  case TokenKind::KW_FUNCTION:
  case TokenKind::KW_RETURN:
  case TokenKind::KW_IF:
  case TokenKind::KW_ELSE:
  case TokenKind::KW_WHILE:
  case TokenKind::KW_FOR:
  case TokenKind::KW_IN:
  case TokenKind::KW_TRUE:
  case TokenKind::KW_FALSE:
  case TokenKind::KW_PRINT:
  case TokenKind::KW_BREAK:
  case TokenKind::KW_CONTINUE:
  case TokenKind::KW_STRUCT:
  case TokenKind::KW_INTERFACE:
  case TokenKind::KW_IMPORT:
  case TokenKind::KW_FROM:
    return true;
  default:
    return false;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Token access
// ─────────────────────────────────────────────────────────────────────────────

const Token& OxToJs::cur() const {
  if (idx_ >= toks_.size()) return toks_.back(); // EOF sentinel
  return toks_[idx_];
}

const Token& OxToJs::peekAt(size_t n) const {
  size_t p = idx_ + n;
  if (p >= toks_.size()) return toks_.back();
  return toks_[p];
}

void OxToJs::advance() {
  if (!atEnd()) idx_++;
}

bool OxToJs::atEnd() const {
  return idx_ >= toks_.size() || cur().kind == TokenKind::END_OF_FILE;
}

// ─────────────────────────────────────────────────────────────────────────────
// Emission
// ─────────────────────────────────────────────────────────────────────────────

void OxToJs::emitText(const std::string& text) {
  if (text.empty()) return;

  if (needSpace_) {
    // Don't add a space before punctuation that shouldn't have one
    char fc = text[0];
    bool isPunct = fc == '(' || fc == ')' || fc == '{' || fc == '}' ||
                   fc == '[' || fc == ']' || fc == ',' || fc == ';' ||
                   fc == '.' || fc == ':';
    if (!isPunct) out_ += ' ';
  }

  out_ += text;

  // After the token, decide whether the NEXT token needs a space before it
  char lc = text.back();
  // After '(' and '[', no leading space for the first inner token
  needSpace_ = (lc != '(' && lc != '[' && lc != ' ' && lc != '\n');
}

// ─────────────────────────────────────────────────────────────────────────────
// Public entry point
// ─────────────────────────────────────────────────────────────────────────────

std::string OxToJs::transpile(const std::string& src) {
  Lexer lexer(src, "<client>");
  toks_ = lexer.tokenize();
  idx_  = 0;
  out_.clear();
  needSpace_ = false;

  while (!atEnd()) {
    processOne();
  }
  return out_;
}

// ─────────────────────────────────────────────────────────────────────────────
// Type-name skipper
// ─────────────────────────────────────────────────────────────────────────────

void OxToJs::skipTypeName() {
  if (atEnd()) return;
  if (isTypeKeyword(cur().kind) || cur().kind == TokenKind::IDENTIFIER) {
    advance();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Block skipper  { ... }
// ─────────────────────────────────────────────────────────────────────────────

void OxToJs::skipBlock() {
  if (!atEnd() && cur().kind == TokenKind::LBRACE) advance(); // consume '{'
  int depth = 1;
  while (!atEnd() && depth > 0) {
    if (cur().kind == TokenKind::LBRACE)      depth++;
    else if (cur().kind == TokenKind::RBRACE) { --depth; if (depth == 0) { advance(); return; } }
    advance();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Main token processor
// ─────────────────────────────────────────────────────────────────────────────

void OxToJs::processOne() {
  if (atEnd()) return;
  const Token& t = cur();

  switch (t.kind) {

  // ── Skip import statements entirely ─────────────────────────────────────
  case TokenKind::KW_IMPORT: {
    advance(); // skip 'import'
    // Consume until semicolon or after 'from "..."'
    while (!atEnd()) {
      if (cur().kind == TokenKind::SEMICOLON) { advance(); break; }
      if (cur().kind == TokenKind::KW_FROM) {
        advance(); // 'from'
        if (!atEnd() && cur().kind == TokenKind::STRING_LITERAL) advance(); // path
        if (!atEnd() && cur().kind == TokenKind::SEMICOLON) advance();
        break;
      }
      advance();
    }
    out_ += '\n';
    needSpace_ = false;
    break;
  }

  // ── Skip struct / interface declarations ────────────────────────────────
  case TokenKind::KW_STRUCT:
  case TokenKind::KW_INTERFACE: {
    advance(); // keyword
    if (!atEnd() && cur().kind == TokenKind::IDENTIFIER) advance(); // name
    if (!atEnd() && cur().kind == TokenKind::LBRACE) skipBlock();
    out_ += '\n';
    needSpace_ = false;
    break;
  }

  // ── fn → function ───────────────────────────────────────────────────────
  case TokenKind::KW_FN: {
    emitText("function");
    advance();
    break;
  }

  // ── print → console.log ─────────────────────────────────────────────────
  case TokenKind::KW_PRINT: {
    emitText("console.log");
    advance();
    break;
  }

  // ── Type annotation: skip ': TypeName' ──────────────────────────────────
  // In Oxide, ':' is ONLY used for type annotations (no ternary ?:)
  case TokenKind::COLON: {
    advance(); // skip ':'
    skipTypeName();
    // needSpace_ unchanged — the space context before the ':' is preserved
    break;
  }

  // ── Return type: skip '-> TypeName' ─────────────────────────────────────
  case TokenKind::ARROW: {
    advance(); // skip '->'
    skipTypeName();
    break;
  }

  // ── for...in  →  for (let VAR = START; VAR < END; VAR++) ────────────────
  case TokenKind::KW_FOR: {
    advance(); // skip 'for'
    // Detect: IDENT KW_IN expr DOT_DOT expr
    if (!atEnd() && cur().kind == TokenKind::IDENTIFIER &&
        peekAt(1).kind == TokenKind::KW_IN) {

      std::string varName = cur().lexeme;
      advance(); // IDENT
      advance(); // KW_IN

      // Collect start expression (tokens before DOT_DOT)
      std::string startExpr;
      while (!atEnd() && cur().kind != TokenKind::DOT_DOT &&
                          cur().kind != TokenKind::LBRACE) {
        if (!startExpr.empty()) startExpr += ' ';
        startExpr += cur().lexeme;
        advance();
      }
      // Collect end expression (tokens after DOT_DOT, before LBRACE)
      std::string endExpr;
      if (!atEnd() && cur().kind == TokenKind::DOT_DOT) {
        advance(); // skip '..'
        while (!atEnd() && cur().kind != TokenKind::LBRACE) {
          if (!endExpr.empty()) endExpr += ' ';
          endExpr += cur().lexeme;
          advance();
        }
      }

      if (needSpace_) out_ += ' ';
      out_ += "for (let " + varName + " = " + startExpr + "; " +
              varName + " < " + endExpr + "; " + varName + "++)";
      needSpace_ = true;
    } else {
      emitText("for");
    }
    break;
  }

  // ── Semicolons: emit then newline for readability ────────────────────────
  case TokenKind::SEMICOLON: {
    out_ += ";\n";
    needSpace_ = false;
    advance();
    break;
  }

  // ── Braces: emit then newline for readability ────────────────────────────
  case TokenKind::LBRACE: {
    if (needSpace_) out_ += ' ';
    out_ += "{\n";
    needSpace_ = false;
    advance();
    break;
  }
  case TokenKind::RBRACE: {
    out_ += "}\n";
    needSpace_ = false;
    advance();
    break;
  }

  // ── String literals: re-add double quotes (lexer strips them) ───────────
  case TokenKind::STRING_LITERAL: {
    // The Oxide lexer stores the raw content without surrounding quotes.
    // Re-add them so the emitted JS has valid string literals.
    if (needSpace_) out_ += ' ';
    out_ += '"';
    out_ += t.lexeme;
    out_ += '"';
    needSpace_ = true;
    advance();
    break;
  }

  // ── Default: emit token lexeme as-is ────────────────────────────────────
  default: {
    bool wl = isWordLike(t.kind);
    // Only add a leading space if this is a word-like token and we need one
    if (wl) {
      emitText(t.lexeme);
    } else {
      // Punctuation / operators: emit without forced space
      if (needSpace_ && t.kind != TokenKind::RPAREN &&
                        t.kind != TokenKind::RBRACKET &&
                        t.kind != TokenKind::COMMA &&
                        t.kind != TokenKind::SEMICOLON &&
                        t.kind != TokenKind::DOT) {
        out_ += ' ';
      }
      out_ += t.lexeme;
      needSpace_ = (t.kind == TokenKind::RPAREN   ||
                    t.kind == TokenKind::RBRACKET  ||
                    t.kind == TokenKind::IDENTIFIER ||
                    t.kind == TokenKind::INT_LITERAL ||
                    t.kind == TokenKind::FLOAT_LITERAL ||
                    t.kind == TokenKind::BOOL_LITERAL);
    }
    advance();
    break;
  }

  } // switch
}

} // namespace oxide
