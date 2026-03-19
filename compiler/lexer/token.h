// token.h — Oxide Language Token Definitions
// Phase 1: Core Compiler MVP
//
// Defines all token types and the Token struct used by the Lexer.

#pragma once
#include <string>

namespace oxide {

/// All token types in the Oxide language
enum class TokenKind {
  // ── Literals ──────────────────────────────────────────
  INT_LITERAL,    // e.g. 42
  FLOAT_LITERAL,  // e.g. 3.14
  BOOL_LITERAL,   // true | false
  STRING_LITERAL, // e.g. "hello"

  // ── Identifiers ───────────────────────────────────────
  IDENTIFIER, // e.g. myVar, foo

  // ── Keywords ──────────────────────────────────────────
  KW_LET,      // let
  KW_CONST,    // const
  KW_FN,       // fn
  KW_RETURN,   // return
  KW_IF,       // if
  KW_ELSE,     // else
  KW_WHILE,    // while
  KW_FOR,      // for
  KW_IN,       // in
  KW_IMPORT,   // import
  KW_FROM,     // from
  KW_EXPORT,   // export
  KW_TRUE,     // true
  KW_FALSE,    // false
  KW_PRINT,    // print (builtin for MVP)
  KW_INT,      // int  (type keyword)
  KW_FLOAT,    // float (type keyword)
  KW_BOOL,     // bool (type keyword)
  KW_STRING,   // string (type keyword)
  KW_VOID,     // void (type keyword)
  KW_STRUCT,   // struct
  KW_BREAK,    // break
  KW_CONTINUE, // continue
  KW_AS,        // as        (import alias)
  KW_TYPE,      // type      (import type)
  KW_FUNCTION,  // function  (TypeScript alias for fn)
  KW_INTERFACE, // interface (TypeScript alias for struct)

  // ── Operators ─────────────────────────────────────────
  PLUS,          // +
  MINUS,         // -
  STAR,          // *
  SLASH,         // /
  PERCENT,       // %
  EQ,            // =
  EQ_EQ,         // ==
  BANG_EQ,       // !=
  LT,            // <
  LT_EQ,         // <=
  GT,            // >
  GT_EQ,         // >=
  AMP_AMP,       // &&
  PIPE_PIPE,     // ||
  BANG,          // !
  ARROW,         // ->
  COLON,         // :
  COLON_COLON,   // ::
  QUESTION,      // ?
  DOT_DOT,       // ..  (range operator for for-in)

  // ── Delimiters ────────────────────────────────────────
  LPAREN,    // (
  RPAREN,    // )
  LBRACE,    // {
  RBRACE,    // }
  LBRACKET,  // [
  RBRACKET,  // ]
  COMMA,     // ,
  SEMICOLON, // ;
  DOT,       // .

  // ── String Interpolation ─────────────────────────────
  // These tokens are emitted when a string literal contains ${...} expressions.
  // e.g.  "Hello, ${name}! You have ${count} items."
  // Lexer emits: INTERP_STR_HEAD("Hello, "), <name tokens>, INTERP_STR_TAIL(" items.")
  // With two interpolations: HEAD, <expr1 tokens>, MID(text), <expr2 tokens>, TAIL
  INTERP_STR_HEAD, // text before first ${
  INTERP_STR_MID,  // text between two ${} blocks
  INTERP_STR_TAIL, // text after last }

  // ── Special ───────────────────────────────────────────
  END_OF_FILE,
  UNKNOWN,
};

/// A single token produced by the Lexer
struct Token {
  TokenKind kind;
  std::string lexeme; // raw source text
  int line;           // 1-based line number
  int column;         // 1-based column number

  Token(TokenKind kind, std::string lexeme, int line, int col)
      : kind(kind), lexeme(std::move(lexeme)), line(line), column(col) {}

  /// Human-readable name for debugging / error messages
  std::string kindName() const;
};

/// Returns a printable name for a TokenKind
std::string tokenKindName(TokenKind kind);

} // namespace oxide
