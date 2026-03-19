// type_checker.h — Oxide Type Checker
// Phase 1: Core Compiler MVP

#pragma once
#include "../ast/ast.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace oxide {

struct Symbol {
  std::string name;
  TypeKind type       = TypeKind::Unknown;
  bool isConst        = false;
  bool isDefined      = false;
};

using Scope = std::unordered_map<std::string, Symbol>;

class TypeChecker : public ASTVisitor {
public:
  explicit TypeChecker(std::string filename = "<stdin>");

  bool check(Program& program);

  const std::vector<std::string>& errors() const { return errors_; }

  struct FnSig {
    std::vector<TypeKind> paramTypes;
    std::vector<std::string> paramNames;
    TypeKind returnType;
  };

  struct StructInfo {
    std::string name;
    std::vector<std::pair<std::string, TypeKind>> fields;
  };

  static std::string typeKindName(TypeKind k);

  const Symbol* getSymbol(const std::string& name) const;
  const FnSig* getFn(const std::string& name) const;
  const StructInfo* getStruct(const std::string& name) const;
  std::string getStructVarName(const std::string& varName) const;

private:
  std::string filename_;
  std::vector<std::string> errors_;
  std::vector<Scope> scopes_;
  TypeKind lastType_ = TypeKind::Unknown;
  TypeKind currentFnReturnType_ = TypeKind::Void;

  void pushScope();
  void popScope();
  void declare(const std::string& name, TypeKind type, bool isConst);
  Symbol* lookup(const std::string& name);
  void addError(const std::string& msg, const SourceLoc& loc);
  TypeKind binaryResultType(const std::string& op, TypeKind l, TypeKind r,
                             const SourceLoc& loc);
  static std::string typeName(TypeKind k);
  bool validateTypeAnnotation(const TypeAnnotation& ann, const SourceLoc& loc);

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

  void visit(LetStmt&)     override;
  void visit(ConstStmt&)   override;
  void visit(ReturnStmt&)  override;
  void visit(PrintStmt&)   override;
  void visit(ExprStmt&)    override;
  void visit(BlockStmt&)   override;
  void visit(IfStmt&)      override;
  void visit(WhileStmt&)   override;
  void visit(ForStmt&)     override;
  void visit(BreakStmt&)   override;
  void visit(ContinueStmt&) override;

  void visit(FnDecl&)    override;
  void visit(StructDecl&) override;
  void visit(Program&)   override;

  std::unordered_map<std::string, FnSig> fnTable_;
  std::unordered_map<std::string, StructInfo> structTable_;
  std::unordered_map<std::string, Symbol> allDeclared_;
  std::string lastStructName_;
  std::unordered_map<std::string, std::string> structVarNames_;
  int loopDepth_ = 0;
};

} // namespace oxide
