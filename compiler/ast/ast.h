// ast.h — Oxide Language Abstract Syntax Tree
// Phase 1: Core Compiler MVP
//
// Defines the node hierarchy for the Oxide AST.
// Design:
//   - All nodes inherit from ASTNode (virtual base)
//   - Visitor pattern for traversal (codegen, type checker, etc.)
//   - Smart pointers (unique_ptr) for ownership
//   - Each node stores source location for diagnostics

#pragma once
#include <memory>
#include <string>
#include <vector>

namespace oxide {

// ─────────────────────────────────────────────────────────────
// Source location
// ─────────────────────────────────────────────────────────────

struct SourceLoc {
  int line   = 0;
  int column = 0;
  std::string toString() const {
    return std::to_string(line) + ":" + std::to_string(column);
  }
};

// ─────────────────────────────────────────────────────────────
// Forward declarations for visitor
// ─────────────────────────────────────────────────────────────

struct IntLiteralExpr;
struct FloatLiteralExpr;
struct BoolLiteralExpr;
struct StringLiteralExpr;
struct IdentifierExpr;
struct BinaryExpr;
struct UnaryExpr;
struct CallExpr;
struct AssignExpr;
struct FieldAccessExpr;   // Phase 2
struct StructLiteralExpr; // Phase 2
struct StringInterpExpr;  // Phase 2: "Hello, ${name}!"
struct ImportDecl;        // Phase 2: import system

struct LetStmt;
struct ConstStmt;
struct ReturnStmt;
struct PrintStmt;
struct ExprStmt;
struct BlockStmt;
struct IfStmt;
struct WhileStmt;
struct ForStmt;      // Phase 2: for i in start..end
struct BreakStmt;    // Phase 2: break
struct ContinueStmt; // Phase 2: continue
struct FnDecl;
struct StructDecl;   // Phase 2: struct Name { ... }
struct Program;

// ─────────────────────────────────────────────────────────────
// Visitor interface
// ─────────────────────────────────────────────────────────────

struct ASTVisitor {
  virtual ~ASTVisitor() = default;

  // Expressions
  virtual void visit(IntLiteralExpr&)      = 0;
  virtual void visit(FloatLiteralExpr&)    = 0;
  virtual void visit(BoolLiteralExpr&)     = 0;
  virtual void visit(StringLiteralExpr&)   = 0;
  virtual void visit(IdentifierExpr&)      = 0;
  virtual void visit(BinaryExpr&)          = 0;
  virtual void visit(UnaryExpr&)           = 0;
  virtual void visit(CallExpr&)            = 0;
  virtual void visit(AssignExpr&)          = 0;
  virtual void visit(FieldAccessExpr&)     = 0;  // Phase 2
  virtual void visit(StructLiteralExpr&)   = 0;  // Phase 2
  virtual void visit(StringInterpExpr&)    = 0;  // Phase 2: string interpolation
  virtual void visit(ImportDecl&)          = 0;  // Phase 2: import

  // Statements
  virtual void visit(LetStmt&)     = 0;
  virtual void visit(ConstStmt&)   = 0;
  virtual void visit(ReturnStmt&)  = 0;
  virtual void visit(PrintStmt&)   = 0;
  virtual void visit(ExprStmt&)    = 0;
  virtual void visit(BlockStmt&)   = 0;
  virtual void visit(IfStmt&)      = 0;
  virtual void visit(WhileStmt&)   = 0;
  virtual void visit(ForStmt&)     = 0;  // Phase 2
  virtual void visit(BreakStmt&)   = 0;  // Phase 2
  virtual void visit(ContinueStmt&) = 0; // Phase 2

  // Declarations
  virtual void visit(FnDecl&)    = 0;
  virtual void visit(StructDecl&) = 0; // Phase 2
  virtual void visit(Program&)   = 0;
};

// ─────────────────────────────────────────────────────────────
// Base node
// ─────────────────────────────────────────────────────────────

struct ASTNode {
  SourceLoc loc;
  virtual ~ASTNode()                        = default;
  virtual void accept(ASTVisitor& v)        = 0;
};

using NodePtr = std::unique_ptr<ASTNode>;

// ─────────────────────────────────────────────────────────────
// Type representation (simple for Phase 1)
// ─────────────────────────────────────────────────────────────

enum class TypeKind { Int, Float, Bool, String, Void, Unknown, Struct };

struct TypeAnnotation {
  TypeKind kind    = TypeKind::Unknown;
  bool isOptional  = false; // T? syntax
  std::string name; // raw name (for user-defined / generic types)
  std::vector<TypeAnnotation> typeParams; // generic params e.g. List<int> — [int]

  std::string toString() const;
};

// ─────────────────────────────────────────────────────────────
// Expressions
// ─────────────────────────────────────────────────────────────

struct Expr : ASTNode {};
using ExprPtr = std::unique_ptr<Expr>;

/// Integer literal  42
struct IntLiteralExpr : Expr {
  long long value;
  explicit IntLiteralExpr(long long v) : value(v) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// Float literal  3.14
struct FloatLiteralExpr : Expr {
  double value;
  explicit FloatLiteralExpr(double v) : value(v) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// Bool literal  true | false
struct BoolLiteralExpr : Expr {
  bool value;
  explicit BoolLiteralExpr(bool v) : value(v) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// String literal  "hello"
struct StringLiteralExpr : Expr {
  std::string value;
  explicit StringLiteralExpr(std::string v) : value(std::move(v)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// Variable reference  myVar
struct IdentifierExpr : Expr {
  std::string name;
  explicit IdentifierExpr(std::string n) : name(std::move(n)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// Binary expression  lhs op rhs
struct BinaryExpr : Expr {
  std::string op; // "+", "-", "*", "/", "==", "!=", "<", "<=", ">", ">="
  ExprPtr lhs;
  ExprPtr rhs;
  BinaryExpr(std::string op, ExprPtr lhs, ExprPtr rhs)
      : op(std::move(op)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// Unary expression  op expr
struct UnaryExpr : Expr {
  std::string op; // "-", "!"
  ExprPtr operand;
  UnaryExpr(std::string op, ExprPtr operand)
      : op(std::move(op)), operand(std::move(operand)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// Function call  foo(args...)
struct CallExpr : Expr {
  std::string callee;
  std::vector<ExprPtr> args;
  CallExpr(std::string callee, std::vector<ExprPtr> args)
      : callee(std::move(callee)), args(std::move(args)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// Assignment expression  name = expr
struct AssignExpr : Expr {
  std::string name;
  ExprPtr value;
  AssignExpr(std::string name, ExprPtr value)
      : name(std::move(name)), value(std::move(value)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

// ── Phase 2 expressions ───────────────────────────────────────

/// Field access  expr.fieldName
struct FieldAccessExpr : Expr {
  ExprPtr object;
  std::string field;
  FieldAccessExpr(ExprPtr obj, std::string f)
      : object(std::move(obj)), field(std::move(f)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// Struct literal  MyStruct { field1: val1, field2: val2 }
struct FieldInit {
  std::string name;
  ExprPtr value;
};

struct StructLiteralExpr : Expr {
  std::string typeName;  // name of the struct type
  std::vector<FieldInit> fields;
  StructLiteralExpr(std::string typeName, std::vector<FieldInit> fields)
      : typeName(std::move(typeName)), fields(std::move(fields)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// String interpolation  "Hello, ${name}! You have ${count} items."
///
/// textParts has N+1 elements for N expressions (text before/between/after ${}).
/// exprParts has N elements — the expressions inside each ${}.
/// Evaluation: join textParts[0] + str(exprParts[0]) + textParts[1] + ...
struct StringInterpExpr : Expr {
  std::vector<std::string> textParts; // literal text segments (length = exprParts.size() + 1)
  std::vector<ExprPtr>     exprParts; // interpolated expressions
  StringInterpExpr() = default;
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

// ─────────────────────────────────────────────────────────────
// Statements
// ─────────────────────────────────────────────────────────────

struct Stmt : ASTNode {};
using StmtPtr = std::unique_ptr<Stmt>;

/// let name: type = expr;
struct LetStmt : Stmt {
  std::string name;
  TypeAnnotation typeAnnotation;
  bool hasType = false;
  ExprPtr initializer; // may be nullptr
  LetStmt(std::string name, ExprPtr init)
      : name(std::move(name)), initializer(std::move(init)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// const name: type = expr;
struct ConstStmt : Stmt {
  std::string name;
  TypeAnnotation typeAnnotation;
  bool hasType = false;
  ExprPtr initializer;
  ConstStmt(std::string name, ExprPtr init)
      : name(std::move(name)), initializer(std::move(init)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// return expr?;
struct ReturnStmt : Stmt {
  ExprPtr value; // may be nullptr for void returns
  explicit ReturnStmt(ExprPtr v) : value(std::move(v)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// print(expr);  — builtin for MVP
struct PrintStmt : Stmt {
  ExprPtr value;
  explicit PrintStmt(ExprPtr v) : value(std::move(v)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// Expression used as a statement  expr;
struct ExprStmt : Stmt {
  ExprPtr expr;
  explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// { stmts... }
struct BlockStmt : Stmt {
  std::vector<StmtPtr> stmts;
  explicit BlockStmt(std::vector<StmtPtr> stmts) : stmts(std::move(stmts)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// if (cond) then_block else else_block?
struct IfStmt : Stmt {
  ExprPtr condition;
  std::unique_ptr<BlockStmt> thenBlock;
  std::unique_ptr<BlockStmt> elseBlock; // may be nullptr
  IfStmt(ExprPtr cond, std::unique_ptr<BlockStmt> thenB,
         std::unique_ptr<BlockStmt> elseB)
      : condition(std::move(cond)), thenBlock(std::move(thenB)),
        elseBlock(std::move(elseB)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// while (cond) { body }
struct WhileStmt : Stmt {
  ExprPtr condition;
  std::unique_ptr<BlockStmt> body;
  WhileStmt(ExprPtr cond, std::unique_ptr<BlockStmt> body)
      : condition(std::move(cond)), body(std::move(body)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

// ── Phase 2 statements ─────────────────────────────────────────

/// for iterVar in start..end { body }
/// Iterates [start, end) incrementing by 1 each iteration.
struct ForStmt : Stmt {
  std::string iterVar;               // loop variable name (implicit int)
  ExprPtr start;                     // range start (inclusive)
  ExprPtr end;                       // range end (exclusive)
  std::unique_ptr<BlockStmt> body;
  ForStmt(std::string iter, ExprPtr s, ExprPtr e,
          std::unique_ptr<BlockStmt> b)
      : iterVar(std::move(iter)), start(std::move(s)),
        end(std::move(e)), body(std::move(b)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// break;  — exit the innermost loop
struct BreakStmt : Stmt {
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// continue;  — jump to the next iteration of the innermost loop
struct ContinueStmt : Stmt {
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

// ─────────────────────────────────────────────────────────────
// Declarations
// ─────────────────────────────────────────────────────────────

/// Function parameter
struct Param {
  std::string name;
  TypeAnnotation type;
};

/// fn name(params) -> returnType { body }
struct FnDecl : ASTNode {
  std::string name;
  std::vector<Param> params;
  TypeAnnotation returnType;
  std::unique_ptr<BlockStmt> body;
  FnDecl(std::string name, std::vector<Param> params,
         TypeAnnotation retType, std::unique_ptr<BlockStmt> body)
      : name(std::move(name)), params(std::move(params)),
        returnType(std::move(retType)), body(std::move(body)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

// ── Phase 2: Import declarations ──────────────────────────────

/// How names are imported from a module
enum class ImportKind {
  Named,   // import { A, B as C } from "mod"
  Star,    // import * as ns from "mod"
  Type,    // import type { T } from "mod"
};

/// A single binding inside an import: the original name + optional alias
struct ImportSpecifier {
  std::string name;  // e.g. "Router"
  std::string alias; // e.g. "R" in `Router as R`, else same as name
};

/// import { A, B as C } from "path"
/// import * as ns from "path"
/// import type { T } from "path"
struct ImportDecl : ASTNode {
  ImportKind kind = ImportKind::Named;
  std::vector<ImportSpecifier> specifiers; // empty for Star imports
  std::string starAlias;  // for `import * as ns`, ns goes here
  std::string path;       // module path string e.g. "@oxide/http"
  ImportDecl() = default;
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

// ── Phase 2 declarations ───────────────────────────────────────

/// A single field in a struct declaration
struct StructField {
  std::string name;
  TypeAnnotation type;
};

/// struct Name { field: type; ... }
struct StructDecl : ASTNode {
  std::string name;
  std::vector<StructField> fields;
  StructDecl(std::string name, std::vector<StructField> fields)
      : name(std::move(name)), fields(std::move(fields)) {}
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

/// Top-level program = list of declarations / statements
struct Program : ASTNode {
  std::vector<std::unique_ptr<ImportDecl>> imports;   // Phase 2: import stmts
  std::vector<std::unique_ptr<StructDecl>> structs;   // Phase 2: struct decls
  std::vector<std::unique_ptr<FnDecl>> functions;
  std::vector<StmtPtr> topLevelStmts; // for REPL-like usage
  void accept(ASTVisitor& v) override { v.visit(*this); }
};

// ─────────────────────────────────────────────────────────────
// TypeAnnotation helper
// ─────────────────────────────────────────────────────────────

inline std::string TypeAnnotation::toString() const {
  std::string s;
  switch (kind) {
  case TypeKind::Int:     s = "int";     break;
  case TypeKind::Float:   s = "float";   break;
  case TypeKind::Bool:    s = "bool";    break;
  case TypeKind::String:  s = "string";  break;
  case TypeKind::Void:    s = "void";    break;
  case TypeKind::Struct:  s = name;      break; // name holds struct / generic name
  case TypeKind::Unknown: s = name.empty() ? "?" : name; break;
  }
  // Generic type parameters, e.g. List<int> or Map<string, int>
  if (!typeParams.empty()) {
    s += "<";
    for (size_t i = 0; i < typeParams.size(); ++i) {
      if (i > 0) s += ", ";
      s += typeParams[i].toString();
    }
    s += ">";
  }
  if (isOptional) s += "?";
  return s;
}

} // namespace oxide
