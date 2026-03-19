// parser.h — Oxide Language Recursive Descent Parser
// Phase 1: Core Compiler MVP
//
// Parses a flat token list (from the Lexer) into an AST (Program node).
// Implements a standard recursive-descent approach with Pratt-style
// precedence for expressions.

#pragma once
#include "../ast/ast.h"
#include "../lexer/token.h"
#include <memory>
#include <string>
#include <vector>

namespace oxide {

class Parser {
public:
  /// @param tokens  Token stream ending with END_OF_FILE
  /// @param filename Used in error messages
  Parser(std::vector<Token> tokens, std::string filename = "<stdin>");

  /// Parse the full program. Returns nullptr if fatal parse errors occur.
  std::unique_ptr<Program> parse();

  /// Returns accumulated parse errors
  const std::vector<std::string>& errors() const { return errors_; }

private:
  std::vector<Token> tokens_;
  std::string filename_;
  size_t pos_ = 0;

  std::vector<std::string> errors_;

  // When true, parsePrimary() will not attempt to parse struct literals.
  // Set during for-in range parsing to prevent 'end {' being read as a struct.
  bool noStructLiteral_ = false;

  // ── Token navigation ────────────────────────────────
  const Token& peek() const;
  const Token& peekNext() const;
  const Token& previous() const;
  bool isAtEnd() const;
  const Token& advance();
  bool check(TokenKind kind) const;
  bool match(TokenKind kind);
  bool match(std::initializer_list<TokenKind> kinds);
  const Token& expect(TokenKind kind, const std::string& msg);

  void addError(const std::string& msg, const Token& tok);
  void synchronize();

  // ── Type parsing ────────────────────────────────────
  TypeAnnotation parseType();

  // ── Declaration parsers ──────────────────────────────────
  std::unique_ptr<FnDecl>     parseFnDecl();
  std::unique_ptr<StructDecl> parseStructDecl();
  std::unique_ptr<ImportDecl> parseImportDecl();

  // ── Statement parsers ────────────────────────────────────
  StmtPtr parseStatement();
  StmtPtr parseLetStmt();
  StmtPtr parseConstStmt();
  StmtPtr parseReturnStmt();
  StmtPtr parsePrintStmt();
  StmtPtr parseIfStmt();
  StmtPtr parseWhileStmt();
  StmtPtr parseForStmt();
  StmtPtr parseBreakStmt();
  StmtPtr parseContinueStmt();
  std::unique_ptr<BlockStmt> parseBlock();
  StmtPtr parseExprStmt();

  // ── Expression parsers (Pratt precedence) ───────────────────────────
  ExprPtr parseExpr();
  ExprPtr parseAssign();
  ExprPtr parseOr();
  ExprPtr parseAnd();
  ExprPtr parseEquality();
  ExprPtr parseComparison();
  ExprPtr parseAddSub();
  ExprPtr parseMulDiv();
  ExprPtr parseUnary();
  ExprPtr parseCall();
  ExprPtr parsePrimary();
};

} // namespace oxide
