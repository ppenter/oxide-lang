// parser.cpp — Oxide Recursive Descent Parser Implementation

#include "parser.h"
#include <cassert>
#include <stdexcept>

namespace oxide {

// ─────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────

Parser::Parser(std::vector<Token> tokens, std::string filename)
    : tokens_(std::move(tokens)), filename_(std::move(filename)) {}

// ─────────────────────────────────────────────────────────────
// Token navigation helpers
// ─────────────────────────────────────────────────────────────

bool Parser::isAtEnd() const {
  return pos_ >= tokens_.size() ||
         tokens_[pos_].kind == TokenKind::END_OF_FILE;
}

const Token& Parser::peek() const {
  return tokens_[pos_];
}

const Token& Parser::peekNext() const {
  size_t next = pos_ + 1;
  if (next >= tokens_.size()) return tokens_.back();
  return tokens_[next];
}

const Token& Parser::previous() const {
  assert(pos_ > 0);
  return tokens_[pos_ - 1];
}

const Token& Parser::advance() {
  if (!isAtEnd()) ++pos_;
  return previous();
}

bool Parser::check(TokenKind kind) const {
  if (isAtEnd()) return kind == TokenKind::END_OF_FILE;
  return peek().kind == kind;
}

bool Parser::match(TokenKind kind) {
  if (check(kind)) { advance(); return true; }
  return false;
}

bool Parser::match(std::initializer_list<TokenKind> kinds) {
  for (auto k : kinds) { if (match(k)) return true; }
  return false;
}

const Token& Parser::expect(TokenKind kind, const std::string& msg) {
  if (check(kind)) return advance();
  addError(msg, peek());
  return peek();
}

void Parser::addError(const std::string& msg, const Token& tok) {
  errors_.push_back(filename_ + ":" + std::to_string(tok.line) + ":" +
                    std::to_string(tok.column) + ": parse error: " + msg);
}

void Parser::synchronize() {
  advance();
  while (!isAtEnd()) {
    if (previous().kind == TokenKind::SEMICOLON) return;
    switch (peek().kind) {
    case TokenKind::KW_FN:
    case TokenKind::KW_FUNCTION:
    case TokenKind::KW_LET:
    case TokenKind::KW_CONST:
    case TokenKind::KW_RETURN:
    case TokenKind::KW_IF:
    case TokenKind::KW_WHILE:
    case TokenKind::KW_FOR:
    case TokenKind::KW_PRINT:
      return;
    default:
      break;
    }
    advance();
  }
}

// ─────────────────────────────────────────────────────────────
// Type annotation parsing
// ─────────────────────────────────────────────────────────────

TypeAnnotation Parser::parseType() {
  TypeAnnotation ann;
  ann.name = peek().lexeme;
  switch (peek().kind) {
  case TokenKind::KW_INT:    ann.kind = TypeKind::Int;    advance(); break;
  case TokenKind::KW_FLOAT:  ann.kind = TypeKind::Float;  advance(); break;
  case TokenKind::KW_BOOL:   ann.kind = TypeKind::Bool;   advance(); break;
  case TokenKind::KW_STRING: ann.kind = TypeKind::String; advance(); break;
  case TokenKind::KW_VOID:   ann.kind = TypeKind::Void;   advance(); break;
  case TokenKind::IDENTIFIER:
    ann.kind = TypeKind::Struct;
    advance();
    break;
  default:
    addError("expected type name, got '" + peek().lexeme + "'", peek());
    ann.kind = TypeKind::Unknown;
    break;
  }
  if (check(TokenKind::LT)) {
    advance();
    while (!isAtEnd() && !check(TokenKind::GT)) {
      ann.typeParams.push_back(parseType());
      if (!match(TokenKind::COMMA)) break;
    }
    expect(TokenKind::GT, "expected '>' to close generic type parameters");
  }
  if (match(TokenKind::QUESTION)) ann.isOptional = true;
  return ann;
}

// ─────────────────────────────────────────────────────────────
// Top-level parse
// ─────────────────────────────────────────────────────────────

std::unique_ptr<Program> Parser::parse() {
  auto program = std::make_unique<Program>();

  while (!isAtEnd()) {
    try {
      if (check(TokenKind::KW_IMPORT)) {
        program->imports.push_back(parseImportDecl());
      } else if (check(TokenKind::KW_STRUCT) || check(TokenKind::KW_INTERFACE)) {
        program->structs.push_back(parseStructDecl());
      } else if (check(TokenKind::KW_FN) || check(TokenKind::KW_FUNCTION)) {
        program->functions.push_back(parseFnDecl());
      } else {
        program->topLevelStmts.push_back(parseStatement());
      }
    } catch (const std::runtime_error&) {
      synchronize();
    }
  }
  return program;
}

// ─────────────────────────────────────────────────────────────
// Import declaration (Phase 2)
// ─────────────────────────────────────────────────────────────

std::unique_ptr<ImportDecl> Parser::parseImportDecl() {
  Token tok = peek();
  expect(TokenKind::KW_IMPORT, "expected 'import'");

  auto decl = std::make_unique<ImportDecl>();
  decl->loc = {tok.line, tok.column};

  if (check(TokenKind::KW_TYPE)) {
    advance();
    decl->kind = ImportKind::Type;
    expect(TokenKind::LBRACE, "expected '{' after 'import type'");
    while (!check(TokenKind::RBRACE) && !isAtEnd()) {
      const Token& nameTok = expect(TokenKind::IDENTIFIER, "expected type name");
      ImportSpecifier spec;
      spec.name = nameTok.lexeme;
      spec.alias = nameTok.lexeme;
      if (check(TokenKind::KW_AS)) {
        advance();
        const Token& aliasTok = expect(TokenKind::IDENTIFIER, "expected alias after 'as'");
        spec.alias = aliasTok.lexeme;
      }
      decl->specifiers.push_back(spec);
      if (!match(TokenKind::COMMA)) break;
    }
    expect(TokenKind::RBRACE, "expected '}' to close import type list");
  }
  else if (check(TokenKind::STAR)) {
    advance();
    decl->kind = ImportKind::Star;
    expect(TokenKind::KW_AS, "expected 'as' after 'import *'");
    const Token& aliasTok = expect(TokenKind::IDENTIFIER, "expected namespace alias after 'import * as'");
    decl->starAlias = aliasTok.lexeme;
  }
  else {
    decl->kind = ImportKind::Named;
    expect(TokenKind::LBRACE, "expected '{' or '*' or 'type' after 'import'");
    while (!check(TokenKind::RBRACE) && !isAtEnd()) {
      const Token& nameTok = expect(TokenKind::IDENTIFIER, "expected name in import list");
      ImportSpecifier spec;
      spec.name = nameTok.lexeme;
      spec.alias = nameTok.lexeme;
      if (check(TokenKind::KW_AS)) {
        advance();
        const Token& aliasTok = expect(TokenKind::IDENTIFIER, "expected alias after 'as'");
        spec.alias = aliasTok.lexeme;
      }
      decl->specifiers.push_back(spec);
      if (!match(TokenKind::COMMA)) break;
    }
    expect(TokenKind::RBRACE, "expected '}' to close import list");
  }

  expect(TokenKind::KW_FROM, "expected 'from' after import specifiers");
  const Token& pathTok = expect(TokenKind::STRING_LITERAL, "expected module path string after 'from'");
  decl->path = pathTok.lexeme;
  expect(TokenKind::SEMICOLON, "expected ';' after import declaration");

  return decl;
}

// ─────────────────────────────────────────────────────────────
// Struct declaration (Phase 2)
// ─────────────────────────────────────────────────────────────

std::unique_ptr<StructDecl> Parser::parseStructDecl() {
  Token tok = peek();
  if (check(TokenKind::KW_INTERFACE)) advance();
  else expect(TokenKind::KW_STRUCT, "expected 'struct' or 'interface'");
  const Token& nameTok = expect(TokenKind::IDENTIFIER, "expected struct name");
  std::string name = nameTok.lexeme;

  expect(TokenKind::LBRACE, "expected '{' after struct name");

  std::vector<StructField> fields;
  while (!check(TokenKind::RBRACE) && !isAtEnd()) {
    const Token& fieldName = expect(TokenKind::IDENTIFIER, "expected field name");
    expect(TokenKind::COLON, "expected ':' after field name");
    TypeAnnotation fieldType = parseType();
    expect(TokenKind::SEMICOLON, "expected ';' after struct field");
    fields.push_back({fieldName.lexeme, fieldType});
  }
  expect(TokenKind::RBRACE, "expected '}' after struct fields");

  auto decl = std::make_unique<StructDecl>(name, std::move(fields));
  decl->loc = {tok.line, tok.column};
  return decl;
}

// ─────────────────────────────────────────────────────────────
// Function declaration
// ─────────────────────────────────────────────────────────────

std::unique_ptr<FnDecl> Parser::parseFnDecl() {
  Token fnTok = peek();
  if (check(TokenKind::KW_FUNCTION)) advance();
  else expect(TokenKind::KW_FN, "expected 'fn' or 'function'");
  const Token& nameTok = expect(TokenKind::IDENTIFIER, "expected function name after 'fn'");
  std::string name = nameTok.lexeme;

  expect(TokenKind::LPAREN, "expected '(' after function name");

  std::vector<Param> params;
  while (!check(TokenKind::RPAREN) && !isAtEnd()) {
    Param p;
    const Token& paramName = expect(TokenKind::IDENTIFIER, "expected parameter name");
    p.name = paramName.lexeme;
    expect(TokenKind::COLON, "expected ':' after parameter name");
    p.type = parseType();
    params.push_back(std::move(p));
    if (!match(TokenKind::COMMA)) break;
  }
  expect(TokenKind::RPAREN, "expected ')' after parameters");

  TypeAnnotation returnType;
  returnType.kind = TypeKind::Void;
  if (match(TokenKind::ARROW)) {
    returnType = parseType();
  } else if (match(TokenKind::COLON)) {
    returnType = parseType();
  }

  auto body = parseBlock();

  auto decl = std::make_unique<FnDecl>(name, std::move(params), returnType,
                                        std::move(body));
  decl->loc = {fnTok.line, fnTok.column};
  return decl;
}

// ─────────────────────────────────────────────────────────────
// Statement parsers
// ─────────────────────────────────────────────────────────────

StmtPtr Parser::parseStatement() {
  if (check(TokenKind::KW_LET))      return parseLetStmt();
  if (check(TokenKind::KW_CONST))    return parseConstStmt();
  if (check(TokenKind::KW_RETURN))   return parseReturnStmt();
  if (check(TokenKind::KW_PRINT))    return parsePrintStmt();
  if (check(TokenKind::KW_IF))       return parseIfStmt();
  if (check(TokenKind::KW_WHILE))    return parseWhileStmt();
  if (check(TokenKind::KW_FOR))      return parseForStmt();
  if (check(TokenKind::KW_BREAK))    return parseBreakStmt();
  if (check(TokenKind::KW_CONTINUE)) return parseContinueStmt();
  if (check(TokenKind::LBRACE)) {
    auto block = parseBlock();
    return block;
  }
  return parseExprStmt();
}

StmtPtr Parser::parseForStmt() {
  Token tok = peek();
  expect(TokenKind::KW_FOR, "expected 'for'");
  const Token& iterTok = expect(TokenKind::IDENTIFIER, "expected loop variable name after 'for'");
  std::string iterVar = iterTok.lexeme;
  expect(TokenKind::KW_IN, "expected 'in' after loop variable");
  noStructLiteral_ = true;
  ExprPtr startExpr = parseAddSub();
  expect(TokenKind::DOT_DOT, "expected '..' in for-in range");
  ExprPtr endExpr = parseAddSub();
  noStructLiteral_ = false;
  auto body = parseBlock();
  auto stmt = std::make_unique<ForStmt>(iterVar, std::move(startExpr),
                                         std::move(endExpr), std::move(body));
  stmt->loc = {tok.line, tok.column};
  return stmt;
}

StmtPtr Parser::parseBreakStmt() {
  Token tok = peek();
  expect(TokenKind::KW_BREAK, "expected 'break'");
  expect(TokenKind::SEMICOLON, "expected ';' after 'break'");
  auto stmt = std::make_unique<BreakStmt>();
  stmt->loc = {tok.line, tok.column};
  return stmt;
}

StmtPtr Parser::parseContinueStmt() {
  Token tok = peek();
  expect(TokenKind::KW_CONTINUE, "expected 'continue'");
  expect(TokenKind::SEMICOLON, "expected ';' after 'continue'");
  auto stmt = std::make_unique<ContinueStmt>();
  stmt->loc = {tok.line, tok.column};
  return stmt;
}

StmtPtr Parser::parseLetStmt() {
  Token tok = peek();
  expect(TokenKind::KW_LET, "expected 'let'");
  const Token& name = expect(TokenKind::IDENTIFIER, "expected variable name after 'let'");

  TypeAnnotation ann;
  bool hasType = false;
  if (match(TokenKind::COLON)) {
    ann = parseType();
    hasType = true;
  }

  ExprPtr init;
  if (match(TokenKind::EQ)) {
    init = parseExpr();
  }
  expect(TokenKind::SEMICOLON, "expected ';' after let declaration");

  auto stmt = std::make_unique<LetStmt>(name.lexeme, std::move(init));
  stmt->typeAnnotation = ann;
  stmt->hasType = hasType;
  stmt->loc = {tok.line, tok.column};
  return stmt;
}

StmtPtr Parser::parseConstStmt() {
  Token tok = peek();
  expect(TokenKind::KW_CONST, "expected 'const'");
  const Token& name = expect(TokenKind::IDENTIFIER, "expected variable name after 'const'");

  TypeAnnotation ann;
  bool hasType = false;
  if (match(TokenKind::COLON)) {
    ann = parseType();
    hasType = true;
  }

  expect(TokenKind::EQ, "expected '=' after const name");
  ExprPtr init = parseExpr();
  expect(TokenKind::SEMICOLON, "expected ';' after const declaration");

  auto stmt = std::make_unique<ConstStmt>(name.lexeme, std::move(init));
  stmt->typeAnnotation = ann;
  stmt->hasType = hasType;
  stmt->loc = {tok.line, tok.column};
  return stmt;
}

StmtPtr Parser::parseReturnStmt() {
  Token tok = peek();
  expect(TokenKind::KW_RETURN, "expected 'return'");
  ExprPtr value;
  if (!check(TokenKind::SEMICOLON) && !isAtEnd()) {
    value = parseExpr();
  }
  expect(TokenKind::SEMICOLON, "expected ';' after return value");
  auto stmt = std::make_unique<ReturnStmt>(std::move(value));
  stmt->loc = {tok.line, tok.column};
  return stmt;
}

StmtPtr Parser::parsePrintStmt() {
  Token tok = peek();
  expect(TokenKind::KW_PRINT, "expected 'print'");
  expect(TokenKind::LPAREN, "expected '(' after 'print'");
  ExprPtr value = parseExpr();
  expect(TokenKind::RPAREN, "expected ')' after print argument");
  expect(TokenKind::SEMICOLON, "expected ';' after print statement");
  auto stmt = std::make_unique<PrintStmt>(std::move(value));
  stmt->loc = {tok.line, tok.column};
  return stmt;
}

StmtPtr Parser::parseIfStmt() {
  Token tok = peek();
  expect(TokenKind::KW_IF, "expected 'if'");
  expect(TokenKind::LPAREN, "expected '(' after 'if'");
  ExprPtr cond = parseExpr();
  expect(TokenKind::RPAREN, "expected ')' after if condition");
  auto thenBlock = parseBlock();

  std::unique_ptr<BlockStmt> elseBlock;
  if (match(TokenKind::KW_ELSE)) {
    elseBlock = parseBlock();
  }

  auto stmt = std::make_unique<IfStmt>(std::move(cond), std::move(thenBlock),
                                        std::move(elseBlock));
  stmt->loc = {tok.line, tok.column};
  return stmt;
}

StmtPtr Parser::parseWhileStmt() {
  Token tok = peek();
  expect(TokenKind::KW_WHILE, "expected 'while'");
  expect(TokenKind::LPAREN, "expected '(' after 'while'");
  ExprPtr cond = parseExpr();
  expect(TokenKind::RPAREN, "expected ')' after while condition");
  auto body = parseBlock();
  auto stmt = std::make_unique<WhileStmt>(std::move(cond), std::move(body));
  stmt->loc = {tok.line, tok.column};
  return stmt;
}

std::unique_ptr<BlockStmt> Parser::parseBlock() {
  expect(TokenKind::LBRACE, "expected '{'");
  std::vector<StmtPtr> stmts;
  while (!check(TokenKind::RBRACE) && !isAtEnd()) {
    try {
      stmts.push_back(parseStatement());
    } catch (const std::runtime_error&) {
      synchronize();
    }
  }
  expect(TokenKind::RBRACE, "expected '}'");
  return std::make_unique<BlockStmt>(std::move(stmts));
}

StmtPtr Parser::parseExprStmt() {
  Token tok = peek();
  auto expr = parseExpr();
  expect(TokenKind::SEMICOLON, "expected ';' after expression");
  auto stmt = std::make_unique<ExprStmt>(std::move(expr));
  stmt->loc = {tok.line, tok.column};
  return stmt;
}

// ─────────────────────────────────────────────────────────────
// Expression parsers — Pratt precedence climbing
// ─────────────────────────────────────────────────────────────

ExprPtr Parser::parseExpr() { return parseAssign(); }

ExprPtr Parser::parseAssign() {
  if (check(TokenKind::IDENTIFIER) && peekNext().kind == TokenKind::EQ) {
    std::string name = peek().lexeme;
    SourceLoc loc = {peek().line, peek().column};
    advance();
    advance();
    ExprPtr value = parseAssign();
    auto expr = std::make_unique<AssignExpr>(name, std::move(value));
    expr->loc = loc;
    return expr;
  }
  return parseOr();
}

ExprPtr Parser::parseOr() {
  auto lhs = parseAnd();
  while (check(TokenKind::PIPE_PIPE)) {
    Token op = advance();
    auto rhs = parseAnd();
    SourceLoc loc = {op.line, op.column};
    auto expr = std::make_unique<BinaryExpr>("||" , std::move(lhs), std::move(rhs));
    expr->loc = loc;
    lhs = std::move(expr);
  }
  return lhs;
}

ExprPtr Parser::parseAnd() {
  auto lhs = parseEquality();
  while (check(TokenKind::AMP_AMP)) {
    Token op = advance();
    auto rhs = parseEquality();
    SourceLoc loc = {op.line, op.column};
    auto expr = std::make_unique<BinaryExpr>("&&" , std::move(lhs), std::move(rhs));
    expr->loc = loc;
    lhs = std::move(expr);
  }
  return lhs;
}

ExprPtr Parser::parseEquality() {
  auto lhs = parseComparison();
  while (check(TokenKind::EQ_EQ) || check(TokenKind::BANG_EQ)) {
    Token op = advance();
    auto rhs = parseComparison();
    SourceLoc loc = {op.line, op.column};
    auto expr = std::make_unique<BinaryExpr>(op.lexeme, std::move(lhs), std::move(rhs));
    expr->loc = loc;
    lhs = std::move(expr);
  }
  return lhs;
}

ExprPtr Parser::parseComparison() {
  auto lhs = parseAddSub();
  while (check(TokenKind::LT) || check(TokenKind::LT_EQ) ||
         check(TokenKind::GT) || check(TokenKind::GT_EQ)) {
    Token op = advance();
    auto rhs = parseAddSub();
    SourceLoc loc = {op.line, op.column};
    auto expr = std::make_unique<BinaryExpr>(op.lexeme, std::move(lhs), std::move(rhs));
    expr->loc = loc;
    lhs = std::move(expr);
  }
  return lhs;
}

ExprPtr Parser::parseAddSub() {
  auto lhs = parseMulDiv();
  while (check(TokenKind::PLUS) || check(TokenKind::MINUS)) {
    Token op = advance();
    auto rhs = parseMulDiv();
    SourceLoc loc = {op.line, op.column};
    auto expr = std::make_unique<BinaryExpr>(op.lexeme, std::move(lhs), std::move(rhs));
    expr->loc = loc;
    lhs = std::move(expr);
  }
  return lhs;
}

ExprPtr Parser::parseMulDiv() {
  auto lhs = parseUnary();
  while (check(TokenKind::STAR) || check(TokenKind::SLASH) ||
         check(TokenKind::PERCENT)) {
    Token op = advance();
    auto rhs = parseUnary();
    SourceLoc loc = {op.line, op.column};
    auto expr = std::make_unique<BinaryExpr>(op.lexeme, std::move(lhs), std::move(rhs));
    expr->loc = loc;
    lhs = std::move(expr);
  }
  return lhs;
}

ExprPtr Parser::parseUnary() {
  if (check(TokenKind::MINUS) || check(TokenKind::BANG)) {
    Token op = advance();
    auto operand = parseUnary();
    SourceLoc loc = {op.line, op.column};
    auto expr = std::make_unique<UnaryExpr>(op.lexeme, std::move(operand));
    expr->loc = loc;
    return expr;
  }
  return parseCall();
}

ExprPtr Parser::parseCall() {
  auto callee = parsePrimary();

  while (true) {
    if (auto* ident = dynamic_cast<IdentifierExpr*>(callee.get())) {
      if (check(TokenKind::LPAREN)) {
        advance();
        std::vector<ExprPtr> args;
        while (!check(TokenKind::RPAREN) && !isAtEnd()) {
          args.push_back(parseExpr());
          if (!match(TokenKind::COMMA)) break;
        }
        expect(TokenKind::RPAREN, "expected ')' after call arguments");
        std::string name = ident->name;
        SourceLoc loc = callee->loc;
        callee = std::make_unique<CallExpr>(name, std::move(args));
        callee->loc = loc;
        continue;
      }
    }
    if (check(TokenKind::DOT)) {
      Token dotTok = advance();
      const Token& fieldTok = expect(TokenKind::IDENTIFIER, "expected field name after '.'");
      SourceLoc loc = {dotTok.line, dotTok.column};
      auto fa = std::make_unique<FieldAccessExpr>(std::move(callee), fieldTok.lexeme);
      fa->loc = loc;
      callee = std::move(fa);
      continue;
    }
    break;
  }
  return callee;
}

ExprPtr Parser::parsePrimary() {
  const Token& tok = peek();
  SourceLoc loc = {tok.line, tok.column};

  if (check(TokenKind::INT_LITERAL)) {
    advance();
    long long val = std::stoll(previous().lexeme);
    auto expr = std::make_unique<IntLiteralExpr>(val);
    expr->loc = loc;
    return expr;
  }

  if (check(TokenKind::FLOAT_LITERAL)) {
    advance();
    double val = std::stod(previous().lexeme);
    auto expr = std::make_unique<FloatLiteralExpr>(val);
    expr->loc = loc;
    return expr;
  }

  if (check(TokenKind::KW_TRUE)) {
    advance();
    auto expr = std::make_unique<BoolLiteralExpr>(true);
    expr->loc = loc;
    return expr;
  }

  if (check(TokenKind::KW_FALSE)) {
    advance();
    auto expr = std::make_unique<BoolLiteralExpr>(false);
    expr->loc = loc;
    return expr;
  }

  if (check(TokenKind::STRING_LITERAL)) {
    advance();
    auto expr = std::make_unique<StringLiteralExpr>(previous().lexeme);
    expr->loc = loc;
    return expr;
  }

  if (check(TokenKind::INTERP_STR_HEAD)) {
    auto interp = std::make_unique<StringInterpExpr>();
    interp->loc = loc;
    interp->textParts.push_back(advance().lexeme);
    while (true) {
      interp->exprParts.push_back(parseOr());
      if (check(TokenKind::INTERP_STR_MID)) {
        interp->textParts.push_back(advance().lexeme);
      } else if (check(TokenKind::INTERP_STR_TAIL)) {
        interp->textParts.push_back(advance().lexeme);
        break;
      } else {
        const Token& t = peek();
        errors_.push_back(filename_ + ":" + std::to_string(t.line) + ":" +
                          std::to_string(t.column) +
                          ": error: expected string interpolation continuation (MID or TAIL)");
        break;
      }
    }
    return interp;
  }

  if (check(TokenKind::IDENTIFIER)) {
    std::string identName = peek().lexeme;
    advance();
    SourceLoc identLoc = loc;

    if (!noStructLiteral_ && check(TokenKind::LBRACE)) {
      advance();
      std::vector<FieldInit> fieldInits;
      while (!check(TokenKind::RBRACE) && !isAtEnd()) {
        const Token& fn = expect(TokenKind::IDENTIFIER, "expected field name in struct literal");
        expect(TokenKind::COLON, "expected ':' after field name in struct literal");
        ExprPtr val = parseExpr();
        fieldInits.push_back({fn.lexeme, std::move(val)});
        if (!match(TokenKind::COMMA)) break;
      }
      expect(TokenKind::RBRACE, "expected '}' to close struct literal");
      auto expr = std::make_unique<StructLiteralExpr>(identName, std::move(fieldInits));
      expr->loc = identLoc;
      return expr;
    }

    auto expr = std::make_unique<IdentifierExpr>(identName);
    expr->loc = identLoc;
    return expr;
  }

  if (match(TokenKind::LPAREN)) {
    auto inner = parseExpr();
    expect(TokenKind::RPAREN, "expected ')' after grouped expression");
    return inner;
  }

  addError("unexpected token '" + tok.lexeme + "' in expression", tok);
  advance();
  auto expr = std::make_unique<IntLiteralExpr>(0);
  expr->loc = loc;
  return expr;
}

} // namespace oxide
