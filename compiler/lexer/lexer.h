// lexer.h — Oxide Language Lexer
// Phase 1: Core Compiler MVP
//
// The Lexer (tokenizer) converts Oxide source text into a flat list of Tokens.
// It supports:
//   - Integer and float literals
//   - String literals (with basic escape handling)
//   - Identifiers and all keywords
//   - All Oxide operators (including two-char ones: ==, !=, <=, >=, &&, ||, ->)
//   - Line/block comments  (// and /* */)
//   - Tracks line/column for error reporting

#pragma once
#include "token.h"
#include <string>
#include <vector>

namespace oxide {

class Lexer {
public:
  /// Construct a Lexer over the given source text.
  /// @param source   Full source code string
  /// @param filename Filename used in diagnostics (optional)
  explicit Lexer(std::string source, std::string filename = "<stdin>");

  /// Tokenize the entire source and return the token list.
  /// Always ends with an END_OF_FILE token.
  std::vector<Token> tokenize();

  /// Returns accumulated error messages (if any)
  const std::vector<std::string>& errors() const { return errors_; }

private:
  std::string source_;
  std::string filename_;
  size_t pos_  = 0; // current position in source_
  int    line_ = 1;
  int    col_  = 1;

  std::vector<std::string> errors_;

  // ── Low-level helpers ────────────────────────────────
  char peek() const;
  char peekNext() const;
  char advance();
  bool isAtEnd() const;
  bool match(char expected);

  // ── Whitespace / comments ────────────────────────────
  void skipWhitespaceAndComments();

  // ── Token scanners ───────────────────────────────────
  Token scanNumber(int startLine, int startCol);
  Token scanString(int startLine, int startCol);
  /// Like scanString but handles ${...} interpolation.
  /// Returns one token for plain strings, or HEAD/expr.../MID/expr.../TAIL sequence.
  std::vector<Token> scanStringTokens(int startLine, int startCol);
  Token scanIdentOrKeyword(int startLine, int startCol);
  Token scanOperatorOrDelim(int startLine, int startCol);

  void addError(const std::string& msg, int line, int col);

  // ── Keyword table lookup ─────────────────────────────
  static TokenKind keywordKind(const std::string& word);
};

} // namespace oxide
