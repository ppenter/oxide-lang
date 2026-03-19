// codegen.h — Oxide LLVM IR Text Emitter
// Phase 2: Language Expansion (structs, for-loops, break/continue)

#pragma once
#include "../ast/ast.h"
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace oxide {

class CodeGen : public ASTVisitor {
public:
  explicit CodeGen(std::string filename = "<stdin>");
  std::string generate(Program& program);
  const std::vector<std::string>& errors() const { return errors_; }

private:
  std::string filename_;
  std::vector<std::string> errors_;
  std::ostringstream globals_;
  std::ostringstream functions_;
  std::ostringstream* current_;
  int nextReg_    = 0;
  int nextLabel_  = 0;
  int nextStrCst_ = 0;
  int freshReg()   { return nextReg_++; }
  int freshLabel() { return nextLabel_++; }
  int freshStr()   { return nextStrCst_++; }
  std::vector<std::unordered_map<std::string, int>> varScopes_;
  void pushVarScope();
  void popVarScope();
  void declareVar(const std::string& name, int allocaReg);
  int  lookupVar(const std::string& name);
  TypeKind lastType_ = TypeKind::Unknown;
  int lastReg_ = -1;
  std::string lastStructTypeName_;
  std::string llvmType(TypeKind k) const;
  std::string llvmZero(TypeKind k) const;
  void emit(const std::string& line);
  void emitLabel(int n);
  void addError(const std::string& msg, const SourceLoc& loc);
  struct StrConst { int idx; std::string value; int len; };
  std::vector<StrConst> strConstants_;
  int addStringConstant(const std::string& s);
  std::unordered_map<std::string, TypeKind> varTypeMap_;
  std::unordered_map<std::string, std::string> varStructTypeMap_;
  std::unordered_map<std::string, TypeKind> fnReturnTypes_;
  struct FieldInfo { std::string name; TypeKind type; int index; };
  struct StructDef  { std::string name; std::vector<FieldInfo> fields; };
  std::unordered_map<std::string, StructDef> structDefs_;
  std::string llvmStructName(const std::string& sname) const { return "%struct." + sname; }
  std::string buildStructLayout(const StructDef& def);
  int emitFieldGEP(const std::string& structTypeName, int structPtrReg, int fieldIndex, TypeKind fieldType);
  struct LoopLabels { int condLbl; int endLbl; };
  std::vector<LoopLabels> loopStack_;
  void visit(IntLiteralExpr&)      override;
  void visit(FloatLiteralExpr&)    override;
  void visit(BoolLiteralExpr&)     override;
  void visit(StringLiteralExpr&)   override;
  void visit(IdentifierExpr&)      override;
  void visit(BinaryExpr&)          override;
  void visit(UnaryExpr&)           override;
  void visit(CallExpr&)            override;
  void visit(AssignExpr&)          override;
  void visit(FieldAccessExpr&)     override;
  void visit(StructLiteralExpr&)   override;
  void visit(StringInterpExpr&)    override;
  void visit(ImportDecl&)          override;
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
  void emitTopLevelMain(Program& program);
  void emitPrintCall(TypeKind type, int valReg);
};

} // namespace oxide
