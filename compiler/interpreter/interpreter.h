// interpreter.h — Oxide Tree-Walking Interpreter
// Executes an Oxide AST directly — no LLVM, no clang needed.
// Usage:  ./oxc file.ox        (runs immediately)
//         ./oxc file.ox --emit (prints LLVM IR instead)

#pragma once
#include "../ast/ast.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace oxide {

// ── Runtime value ────────────────────────────────────────────
// An OxValue holds any Oxide runtime value.
struct OxValue {
  enum class Tag { Int, Float, Bool, String, Struct, Void };
  Tag tag = Tag::Void;

  long long   asInt    = 0;
  double      asFloat  = 0.0;
  bool        asBool   = false;
  std::string asString;

  // Phase 2: struct fields (shared_ptr to allow self-referential OxValue)
  std::string asStructType;  // e.g. "Point"
  std::shared_ptr<std::unordered_map<std::string, OxValue>> structFields;

  static OxValue makeInt(long long v)        { OxValue o; o.tag=Tag::Int;    o.asInt=v;             return o; }
  static OxValue makeFloat(double v)         { OxValue o; o.tag=Tag::Float;  o.asFloat=v;           return o; }
  static OxValue makeBool(bool v)            { OxValue o; o.tag=Tag::Bool;   o.asBool=v;            return o; }
  static OxValue makeString(std::string v)   { OxValue o; o.tag=Tag::String; o.asString=std::move(v); return o; }
  static OxValue makeVoid()                  { OxValue o; o.tag=Tag::Void;                          return o; }

  std::string toString() const;
};

// ── Return signal ─────────────────────────────────────────────
// Thrown inside function bodies to unwind the call stack on `return`.
struct ReturnSignal {
  OxValue value;
};

// ── Runtime error ─────────────────────────────────────────────
struct RuntimeError : std::runtime_error {
  explicit RuntimeError(const std::string& msg) : std::runtime_error(msg) {}
};

// ── Environment (scope) ───────────────────────────────────────
struct Env {
  std::unordered_map<std::string, OxValue> vars;
  Env* parent = nullptr;

  explicit Env(Env* parent = nullptr) : parent(parent) {}

  void set(const std::string& name, OxValue val) {
    vars[name] = std::move(val);
  }

  // Assign to existing variable in nearest enclosing scope that has it
  bool assign(const std::string& name, OxValue val) {
    auto it = vars.find(name);
    if (it != vars.end()) { it->second = std::move(val); return true; }
    if (parent) return parent->assign(name, val);
    return false;
  }

  OxValue* get(const std::string& name) {
    auto it = vars.find(name);
    if (it != vars.end()) return &it->second;
    if (parent) return parent->get(name);
    return nullptr;
  }
};

// ── Module cache entry ────────────────────────────────────────
struct ModuleExports {
  std::unordered_map<std::string, OxValue> vars;
  std::unordered_set<std::string> functionNames;
  std::unordered_set<std::string> structNames;
};

// ── Interpreter ───────────────────────────────────────────────
class Interpreter : public ASTVisitor {
public:
  explicit Interpreter(std::string filename = "<stdin>");

  bool run(Program& program);

  void setPathAliases(std::unordered_map<std::string, std::string> aliases) {
    pathAliases_ = std::move(aliases);
  }

  const std::vector<std::string>& errors() const { return errors_; }

private:
  std::string filename_;
  std::vector<std::string> errors_;
  OxValue result_;
  Env* env_ = nullptr;
  Env globalEnv_;
  std::unordered_map<std::string, FnDecl*> functions_;
  std::unordered_map<std::string, StructDecl*> structLayouts_;
  std::string currentDir_;
  std::unordered_map<std::string, std::string> pathAliases_;
  std::unordered_set<std::string> loadedModulePaths_;
  std::unordered_map<std::string, ModuleExports> moduleCache_;
  std::vector<std::unique_ptr<Program>> ownedModules_;
  int serverFd_ = -1;
  std::string requestHandler_;
  struct RouteEntry { std::string method; std::string path; std::string handlerName; };
  std::vector<RouteEntry> routeTable_;

  void addError(const std::string& msg, const SourceLoc& loc);
  OxValue eval(ASTNode& node);
  OxValue evalExpr(Expr& expr);
  void execStmt(Stmt& stmt);
  void execBlock(BlockStmt& block, Env* parentEnv);
  OxValue callUserFunction(FnDecl* fn, const std::vector<OxValue>& args);
  OxValue applyBinary(const std::string& op, OxValue& l, OxValue& r, const SourceLoc& loc);
  OxValue applyUnary(const std::string& op, OxValue& v, const SourceLoc& loc);
  std::string resolveImportPath(const std::string& importPath) const;
  bool loadModule(const std::string& resolvedPath, const SourceLoc& loc);
  void bindImportedNames(const std::string& resolvedPath, const ImportDecl& decl);

  void visit(IntLiteralExpr&)    override;
  void visit(FloatLiteralExpr&)  override;
  void visit(BoolLiteralExpr&)   override;
  void visit(StringLiteralExpr&) override;
  void visit(IdentifierExpr&)    override;
  void visit(BinaryExpr&)        override;
  void visit(UnaryExpr&)         override;
  void visit(CallExpr&)          override;
  void visit(AssignExpr&)        override;
  void visit(FieldAccessExpr&)   override;
  void visit(StructLiteralExpr&) override;
  void visit(StringInterpExpr&)  override;
  void visit(ImportDecl&)        override;
  void visit(LetStmt&)      override;
  void visit(ConstStmt&)    override;
  void visit(ReturnStmt&)   override;
  void visit(PrintStmt&)    override;
  void visit(ExprStmt&)     override;
  void visit(BlockStmt&)    override;
  void visit(IfStmt&)       override;
  void visit(WhileStmt&)    override;
  void visit(ForStmt&)      override;
  void visit(BreakStmt&)    override;
  void visit(ContinueStmt&) override;
  void visit(FnDecl&)    override;
  void visit(StructDecl&) override;
  void visit(Program&)   override;
};

} // namespace oxide
