// codegen.cpp — Oxide LLVM IR Text Emitter Implementation
// Phase 2: structs, for-loops, break/continue

#include "codegen.h"
#include <cassert>
#include <sstream>

namespace oxide {

CodeGen::CodeGen(std::string filename)
    : filename_(std::move(filename)), current_(&functions_) {}

std::string CodeGen::generate(Program& program) {
  program.accept(*this);
  std::ostringstream out;
  out << "; Oxide compiler — generated LLVM IR\n";
  out << "; Source: " << filename_ << "\n";
  out << "; Compile with: clang output.ll -o program\n\n";
  out << "declare i32 @printf(ptr nocapture, ...)\n";
  out << "declare i32 @puts(ptr nocapture)\n\n";
  out << "@fmt_int_nl = private constant [5 x i8] c\"%ld\\0A\\00\"\n";
  out << "@fmt_flt_nl = private constant [5 x i8] c\"%g\\0A\\00\\00\"\n";
  out << "@fmt_bool_t = private constant [6 x i8] c\"true\\0A\\00\"\n";
  out << "@fmt_bool_f = private constant [7 x i8] c\"false\\0A\\00\"\n";
  for (auto& sc : strConstants_) {
    int len = (int)sc.value.size() + 1;
    out << "@str" << sc.idx << " = private constant [" << len << " x i8] c\"";
    for (char ch : sc.value) {
      if      (ch == '\n') out << "\\0A";
      else if (ch == '\t') out << "\\09";
      else if (ch == '\"')  out << "\\22";
      else if (ch == '\\') out << "\\5C";
      else                 out << ch;
    }
    out << "\\00\"\n";
  }
  out << "\n" << globals_.str();
  out << "\n" << functions_.str();
  return out.str();
}

std::string CodeGen::llvmType(TypeKind k) const {
  switch (k) {
  case TypeKind::Int:    return "i64";
  case TypeKind::Float:  return "double";
  case TypeKind::Bool:   return "i1";
  case TypeKind::String: return "ptr";
  case TypeKind::Void:   return "void";
  case TypeKind::Struct: return "ptr";
  default:               return "i64";
  }
}

std::string CodeGen::llvmZero(TypeKind k) const {
  switch (k) {
  case TypeKind::Float: return "0.0";
  case TypeKind::Bool:  return "false";
  default:              return "0";
  }
}

void CodeGen::emit(const std::string& line) { *current_ << line << "\n"; }
void CodeGen::emitLabel(int n) { *current_ << "lbl" << n << ":\n"; }

void CodeGen::addError(const std::string& msg, const SourceLoc& loc) {
  errors_.push_back(filename_ + ":" + std::to_string(loc.line) + ":" +
                    std::to_string(loc.column) + ": codegen error: " + msg);
}

int CodeGen::addStringConstant(const std::string& s) {
  for (auto& sc : strConstants_) {
    if (sc.value == s) return sc.idx;
  }
  int idx = freshStr();
  strConstants_.push_back({idx, s, (int)s.size() + 1});
  return idx;
}

void CodeGen::pushVarScope() { varScopes_.push_back({}); }
void CodeGen::popVarScope()  { if (!varScopes_.empty()) varScopes_.pop_back(); }

void CodeGen::declareVar(const std::string& name, int allocaReg) {
  if (!varScopes_.empty()) varScopes_.back()[name] = allocaReg;
}

int CodeGen::lookupVar(const std::string& name) {
  for (auto it = varScopes_.rbegin(); it != varScopes_.rend(); ++it) {
    auto f = it->find(name);
    if (f != it->end()) return f->second;
  }
  return -1;
}

std::string CodeGen::buildStructLayout(const StructDef& def) {
  std::string s = "{ ";
  for (size_t i = 0; i < def.fields.size(); ++i) {
    if (i > 0) s += ", ";
    s += llvmType(def.fields[i].type);
  }
  s += " }";
  return s;
}

int CodeGen::emitFieldGEP(const std::string& structTypeName, int structPtrReg,
                           int fieldIndex, TypeKind) {
  int ptrReg = freshReg();
  emit("  %" + std::to_string(ptrReg) + " = getelementptr inbounds " +
       structTypeName + ", ptr %" + std::to_string(structPtrReg) +
       ", i32 0, i32 " + std::to_string(fieldIndex));
  return ptrReg;
}

void CodeGen::emitPrintCall(TypeKind type, int valReg) {
  if (type == TypeKind::Int) {
    int fmtReg = freshReg();
    emit("  %" + std::to_string(fmtReg) + " = getelementptr [5 x i8], ptr @fmt_int_nl, i32 0, i32 0");
    emit("  call i32 (ptr, ...) @printf(ptr %" + std::to_string(fmtReg) + ", i64 %" + std::to_string(valReg) + ")");
  } else if (type == TypeKind::Float) {
    int fmtReg = freshReg();
    emit("  %" + std::to_string(fmtReg) + " = getelementptr [5 x i8], ptr @fmt_flt_nl, i32 0, i32 0");
    emit("  call i32 (ptr, ...) @printf(ptr %" + std::to_string(fmtReg) + ", double %" + std::to_string(valReg) + ")");
  } else if (type == TypeKind::Bool) {
    int tLbl = freshLabel(), fLbl = freshLabel(), endLbl = freshLabel();
    emit("  br i1 %" + std::to_string(valReg) + ", label %lbl" + std::to_string(tLbl) + ", label %lbl" + std::to_string(fLbl));
    emitLabel(tLbl);
    emit("  call i32 (ptr, ...) @printf(ptr @fmt_bool_t)");
    emit("  br label %lbl" + std::to_string(endLbl));
    emitLabel(fLbl);
    emit("  call i32 (ptr, ...) @printf(ptr @fmt_bool_f)");
    emit("  br label %lbl" + std::to_string(endLbl));
    emitLabel(endLbl);
  } else if (type == TypeKind::String) {
    emit("  call i32 @puts(ptr %" + std::to_string(valReg) + ")");
  }
}

void CodeGen::visit(IntLiteralExpr& expr) {
  int reg = freshReg();
  emit("  %" + std::to_string(reg) + " = add i64 0, " + std::to_string(expr.value));
  lastReg_  = reg;
  lastType_ = TypeKind::Int;
}

void CodeGen::visit(FloatLiteralExpr& expr) {
  int reg = freshReg();
  emit("  %" + std::to_string(reg) + " = fadd double 0.0, " + std::to_string(expr.value));
  lastReg_  = reg;
  lastType_ = TypeKind::Float;
}

void CodeGen::visit(BoolLiteralExpr& expr) {
  int reg = freshReg();
  emit("  %" + std::to_string(reg) + " = add i1 0, " + (expr.value ? "1" : "0"));
  lastReg_  = reg;
  lastType_ = TypeKind::Bool;
}

void CodeGen::visit(StringLiteralExpr& expr) {
  int idx = addStringConstant(expr.value);
  int len = (int)expr.value.size() + 1;
  int reg = freshReg();
  emit("  %" + std::to_string(reg) + " = getelementptr [" + std::to_string(len) + " x i8], ptr @str" + std::to_string(idx) + ", i32 0, i32 0");
  lastReg_  = reg;
  lastType_ = TypeKind::String;
}

void CodeGen::visit(IdentifierExpr& expr) {
  int allocaReg = lookupVar(expr.name);
  if (allocaReg < 0) {
    addError("undefined variable '" + expr.name + "'", expr.loc);
    lastReg_  = 0;
    lastType_ = TypeKind::Unknown;
    return;
  }
  auto typeIt = varTypeMap_.find(expr.name);
  TypeKind t = (typeIt != varTypeMap_.end()) ? typeIt->second : TypeKind::Int;
  if (t == TypeKind::Struct) {
    auto sit = varStructTypeMap_.find(expr.name);
    lastStructTypeName_ = (sit != varStructTypeMap_.end()) ? sit->second : "";
    lastReg_  = allocaReg;
    lastType_ = TypeKind::Struct;
    return;
  }
  int loadReg = freshReg();
  emit("  %" + std::to_string(loadReg) + " = load " + llvmType(t) + ", ptr %" + std::to_string(allocaReg));
  lastReg_  = loadReg;
  lastType_ = t;
}

void CodeGen::visit(BinaryExpr& expr) {
  expr.lhs->accept(*this);
  int lReg = lastReg_;
  TypeKind lType = lastType_;
  expr.rhs->accept(*this);
  int rReg = lastReg_;
  TypeKind rType = lastType_;
  int res = freshReg();
  bool isFloat = (lType == TypeKind::Float || rType == TypeKind::Float);
  auto& op = expr.op;
  if (isFloat && lType == TypeKind::Int) {
    int pr = freshReg();
    emit("  %" + std::to_string(pr) + " = sitofp i64 %" + std::to_string(lReg) + " to double");
    lReg = pr;
  }
  if (isFloat && rType == TypeKind::Int) {
    int pr = freshReg();
    emit("  %" + std::to_string(pr) + " = sitofp i64 %" + std::to_string(rReg) + " to double");
    rReg = pr;
  }
  auto lr = "%" + std::to_string(lReg);
  auto rr = "%" + std::to_string(rReg);
  if (op == "+") {
    emit("  %" + std::to_string(res) + (isFloat ? " = fadd double " + lr + ", " + rr : " = add i64 " + lr + ", " + rr));
  } else if (op == "-") {
    emit("  %" + std::to_string(res) + (isFloat ? " = fsub double " + lr + ", " + rr : " = sub i64 " + lr + ", " + rr));
  } else if (op == "*") {
    emit("  %" + std::to_string(res) + (isFloat ? " = fmul double " + lr + ", " + rr : " = mul i64 " + lr + ", " + rr));
  } else if (op == "/") {
    emit("  %" + std::to_string(res) + (isFloat ? " = fdiv double " + lr + ", " + rr : " = sdiv i64 " + lr + ", " + rr));
  } else if (op == "%") {
    emit("  %" + std::to_string(res) + " = srem i64 " + lr + ", " + rr);
    lastReg_ = res; lastType_ = TypeKind::Int; return;
  } else if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
    if (isFloat) {
      std::string pred = (op == "==") ? "oeq" : (op == "!=") ? "one" : (op == "<") ? "olt" : (op == "<=") ? "ole" : (op == ">") ? "ogt" : "oge";
      emit("  %" + std::to_string(res) + " = fcmp " + pred + " double " + lr + ", " + rr);
    } else {
      std::string pred = (op == "==") ? "eq" : (op == "!=") ? "ne" : (op == "<") ? "slt" : (op == "<=") ? "sle" : (op == ">") ? "sgt" : "sge";
      emit("  %" + std::to_string(res) + " = icmp " + pred + " i64 " + lr + ", " + rr);
    }
    lastReg_ = res; lastType_ = TypeKind::Bool; return;
  } else if (op == "&&") {
    emit("  %" + std::to_string(res) + " = and i1 " + lr + ", " + rr);
    lastReg_ = res; lastType_ = TypeKind::Bool; return;
  } else if (op == "||") {
    emit("  %" + std::to_string(res) + " = or i1 " + lr + ", " + rr);
    lastReg_ = res; lastType_ = TypeKind::Bool; return;
  }
  lastReg_  = res;
  lastType_ = isFloat ? TypeKind::Float : lType;
}

void CodeGen::visit(UnaryExpr& expr) {
  expr.operand->accept(*this);
  int operandReg = lastReg_;
  TypeKind t = lastType_;
  int res = freshReg();
  if (expr.op == "-") {
    if (t == TypeKind::Float) {
      emit("  %" + std::to_string(res) + " = fneg double %" + std::to_string(operandReg));
    } else {
      emit("  %" + std::to_string(res) + " = sub i64 0, %" + std::to_string(operandReg));
    }
    lastReg_ = res; lastType_ = t;
  } else if (expr.op == "!") {
    emit("  %" + std::to_string(res) + " = xor i1 %" + std::to_string(operandReg) + ", 1");
    lastReg_ = res; lastType_ = TypeKind::Bool;
  }
}

void CodeGen::visit(CallExpr& expr) {
  std::vector<std::pair<int, TypeKind>> argRegs;
  for (auto& arg : expr.args) {
    arg->accept(*this);
    argRegs.push_back({lastReg_, lastType_});
  }
  TypeKind retType = TypeKind::Void;
  auto it = fnReturnTypes_.find(expr.callee);
  if (it != fnReturnTypes_.end()) retType = it->second;
  int res = -1;
  std::string callStr;
  if (retType != TypeKind::Void) {
    res = freshReg();
    callStr = "  %" + std::to_string(res) + " = call " + llvmType(retType) + " @" + expr.callee + "(";
  } else {
    callStr = "  call void @" + expr.callee + "(";
  }
  for (size_t i = 0; i < argRegs.size(); ++i) {
    if (i > 0) callStr += ", ";
    callStr += llvmType(argRegs[i].second) + " %" + std::to_string(argRegs[i].first);
  }
  callStr += ")";
  emit(callStr);
  lastReg_  = (res >= 0) ? res : 0;
  lastType_ = retType;
}

void CodeGen::visit(AssignExpr& expr) {
  expr.value->accept(*this);
  int valReg = lastReg_;
  TypeKind t = lastType_;
  int allocaReg = lookupVar(expr.name);
  if (allocaReg < 0) {
    addError("undefined variable '" + expr.name + "'", expr.loc);
    return;
  }
  emit("  store " + llvmType(t) + " %" + std::to_string(valReg) + ", ptr %" + std::to_string(allocaReg));
  lastReg_  = valReg;
  lastType_ = t;
}

void CodeGen::visit(FieldAccessExpr& expr) {
  expr.object->accept(*this);
  int objReg = lastReg_;
  std::string sname = lastStructTypeName_;
  lastStructTypeName_.clear();
  if (sname.empty()) {
    addError("field access on non-struct expression", expr.loc);
    lastReg_  = 0;
    lastType_ = TypeKind::Unknown;
    return;
  }
  auto it = structDefs_.find(sname);
  if (it == structDefs_.end()) {
    addError("unknown struct type '" + sname + "'", expr.loc);
    lastReg_  = 0;
    lastType_ = TypeKind::Unknown;
    return;
  }
  const StructDef& def = it->second;
  int fieldIdx = -1;
  TypeKind fieldType = TypeKind::Unknown;
  for (auto& f : def.fields) {
    if (f.name == expr.field) {
      fieldIdx  = f.index;
      fieldType = f.type;
      break;
    }
  }
  if (fieldIdx < 0) {
    addError("struct '" + sname + "' has no field '" + expr.field + "'", expr.loc);
    lastReg_  = 0;
    lastType_ = TypeKind::Unknown;
    return;
  }
  int fieldPtrReg = emitFieldGEP(llvmStructName(sname), objReg, fieldIdx, fieldType);
  int loadReg = freshReg();
  emit("  %" + std::to_string(loadReg) + " = load " + llvmType(fieldType) + ", ptr %" + std::to_string(fieldPtrReg));
  lastReg_  = loadReg;
  lastType_ = fieldType;
}

void CodeGen::visit(StructLiteralExpr& expr) {
  auto it = structDefs_.find(expr.typeName);
  if (it == structDefs_.end()) {
    addError("unknown struct type '" + expr.typeName + "'", expr.loc);
    lastReg_  = 0;
    lastType_ = TypeKind::Unknown;
    lastStructTypeName_.clear();
    return;
  }
  const StructDef& def = it->second;
  std::string llvmST = llvmStructName(expr.typeName);
  int structReg = freshReg();
  emit("  %" + std::to_string(structReg) + " = alloca " + llvmST);
  for (auto& fi : expr.fields) {
    int fieldIdx = -1;
    TypeKind fieldType = TypeKind::Unknown;
    for (auto& f : def.fields) {
      if (f.name == fi.name) {
        fieldIdx  = f.index;
        fieldType = f.type;
        break;
      }
    }
    if (fieldIdx < 0) continue;
    fi.value->accept(*this);
    int valReg = lastReg_;
    int fieldPtrReg = emitFieldGEP(llvmST, structReg, fieldIdx, fieldType);
    emit("  store " + llvmType(fieldType) + " %" + std::to_string(valReg) + ", ptr %" + std::to_string(fieldPtrReg));
  }
  for (auto& f : def.fields) {
    bool provided = false;
    for (auto& fi : expr.fields) {
      if (fi.name == f.name) { provided = true; break; }
    }
    if (!provided) {
      int fieldPtrReg = emitFieldGEP(llvmST, structReg, f.index, f.type);
      emit("  store " + llvmType(f.type) + " " + llvmZero(f.type) + ", ptr %" + std::to_string(fieldPtrReg));
    }
  }
  lastReg_  = structReg;
  lastType_ = TypeKind::Struct;
  lastStructTypeName_ = expr.typeName;
}

void CodeGen::visit(LetStmt& stmt) {
  TypeKind t = TypeKind::Int;
  std::string structTypeName;
  if (stmt.hasType) {
    t = stmt.typeAnnotation.kind;
    if (t == TypeKind::Struct) structTypeName = stmt.typeAnnotation.name;
  } else if (stmt.initializer) {
    stmt.initializer->accept(*this);
    t = lastType_;
    if (t == TypeKind::Struct) structTypeName = lastStructTypeName_;
  }
  if (t == TypeKind::Struct && !structTypeName.empty()) {
    int allocaReg = freshReg();
    emit("  %" + std::to_string(allocaReg) + " = alloca " + llvmStructName(structTypeName));
    declareVar(stmt.name, allocaReg);
    varTypeMap_[stmt.name] = TypeKind::Struct;
    varStructTypeMap_[stmt.name] = structTypeName;
    if (stmt.initializer) {
      int srcReg;
      if (stmt.hasType) {
        stmt.initializer->accept(*this);
        srcReg = lastReg_;
      } else {
        srcReg = lastReg_;
      }
      auto it = structDefs_.find(structTypeName);
      if (it != structDefs_.end()) {
        for (auto& f : it->second.fields) {
          int srcPtr = emitFieldGEP(llvmStructName(structTypeName), srcReg, f.index, f.type);
          int tmpReg = freshReg();
          emit("  %" + std::to_string(tmpReg) + " = load " + llvmType(f.type) + ", ptr %" + std::to_string(srcPtr));
          int dstPtr = emitFieldGEP(llvmStructName(structTypeName), allocaReg, f.index, f.type);
          emit("  store " + llvmType(f.type) + " %" + std::to_string(tmpReg) + ", ptr %" + std::to_string(dstPtr));
        }
      }
    }
    return;
  }
  int allocaReg = freshReg();
  emit("  %" + std::to_string(allocaReg) + " = alloca " + llvmType(t));
  declareVar(stmt.name, allocaReg);
  varTypeMap_[stmt.name] = t;
  if (stmt.initializer) {
    if (stmt.hasType) stmt.initializer->accept(*this);
    emit("  store " + llvmType(t) + " %" + std::to_string(lastReg_) + ", ptr %" + std::to_string(allocaReg));
  }
}

void CodeGen::visit(ConstStmt& stmt) {
  TypeKind t = TypeKind::Int;
  if (stmt.hasType) {
    t = stmt.typeAnnotation.kind;
  } else if (stmt.initializer) {
    stmt.initializer->accept(*this);
    t = lastType_;
  }
  int allocaReg = freshReg();
  emit("  %" + std::to_string(allocaReg) + " = alloca " + llvmType(t));
  declareVar(stmt.name, allocaReg);
  varTypeMap_[stmt.name] = t;
  if (stmt.initializer) {
    if (stmt.hasType) stmt.initializer->accept(*this);
    emit("  store " + llvmType(t) + " %" + std::to_string(lastReg_) + ", ptr %" + std::to_string(allocaReg));
  }
}

void CodeGen::visit(ReturnStmt& stmt) {
  if (stmt.value) {
    stmt.value->accept(*this);
    emit("  ret " + llvmType(lastType_) + " %" + std::to_string(lastReg_));
  } else {
    emit("  ret void");
  }
}

void CodeGen::visit(PrintStmt& stmt) {
  stmt.value->accept(*this);
  emitPrintCall(lastType_, lastReg_);
}

void CodeGen::visit(ExprStmt& stmt) { stmt.expr->accept(*this); }

void CodeGen::visit(BlockStmt& stmt) {
  pushVarScope();
  for (auto& s : stmt.stmts) s->accept(*this);
  popVarScope();
}

void CodeGen::visit(IfStmt& stmt) {
  int tLbl = freshLabel();
  int fLbl = freshLabel();
  int endLbl = freshLabel();
  stmt.condition->accept(*this);
  int condReg = lastReg_;
  emit("  br i1 %" + std::to_string(condReg) + ", label %lbl" + std::to_string(tLbl) + ", label %lbl" + std::to_string(fLbl));
  emitLabel(tLbl);
  stmt.thenBlock->accept(*this);
  emit("  br label %lbl" + std::to_string(endLbl));
  emitLabel(fLbl);
  if (stmt.elseBlock) stmt.elseBlock->accept(*this);
  emit("  br label %lbl" + std::to_string(endLbl));
  emitLabel(endLbl);
}

void CodeGen::visit(WhileStmt& stmt) {
  int condLbl = freshLabel();
  int bodyLbl = freshLabel();
  int endLbl = freshLabel();
  loopStack_.push_back({condLbl, endLbl});
  emit("  br label %lbl" + std::to_string(condLbl));
  emitLabel(condLbl);
  stmt.condition->accept(*this);
  int condReg = lastReg_;
  emit("  br i1 %" + std::to_string(condReg) + ", label %lbl" + std::to_string(bodyLbl) + ", label %lbl" + std::to_string(endLbl));
  emitLabel(bodyLbl);
  stmt.body->accept(*this);
  emit("  br label %lbl" + std::to_string(condLbl));
  emitLabel(endLbl);
  loopStack_.pop_back();
}

void CodeGen::visit(ForStmt& stmt) {
  stmt.start->accept(*this);
  int startVal = lastReg_;
  int iterAlloca = freshReg();
  emit("  %" + std::to_string(iterAlloca) + " = alloca i64");
  emit("  store i64 %" + std::to_string(startVal) + ", ptr %" + std::to_string(iterAlloca));
  pushVarScope();
  declareVar(stmt.iterVar, iterAlloca);
  varTypeMap_[stmt.iterVar] = TypeKind::Int;
  int condLbl = freshLabel();
  int bodyLbl = freshLabel();
  int endLbl = freshLabel();
  loopStack_.push_back({condLbl, endLbl});
  emit("  br label %lbl" + std::to_string(condLbl));
  emitLabel(condLbl);
  int iLoad = freshReg();
  emit("  %" + std::to_string(iLoad) + " = load i64, ptr %" + std::to_string(iterAlloca));
  stmt.end->accept(*this);
  int endVal = lastReg_;
  int cmpReg = freshReg();
  emit("  %" + std::to_string(cmpReg) + " = icmp slt i64 %" + std::to_string(iLoad) + ", %" + std::to_string(endVal));
  emit("  br i1 %" + std::to_string(cmpReg) + ", label %lbl" + std::to_string(bodyLbl) + ", label %lbl" + std::to_string(endLbl));
  emitLabel(bodyLbl);
  stmt.body->accept(*this);
  int iLoad2 = freshReg();
  int incReg = freshReg();
  emit("  %" + std::to_string(iLoad2) + " = load i64, ptr %" + std::to_string(iterAlloca));
  emit("  %" + std::to_string(incReg) + " = add i64 %" + std::to_string(iLoad2) + ", 1");
  emit("  store i64 %" + std::to_string(incReg) + ", ptr %" + std::to_string(iterAlloca));
  emit("  br label %lbl" + std::to_string(condLbl));
  emitLabel(endLbl);
  loopStack_.pop_back();
  popVarScope();
}

void CodeGen::visit(BreakStmt& stmt) {
  if (loopStack_.empty()) {
    addError("'break' outside loop", stmt.loc);
    return;
  }
  emit("  br label %lbl" + std::to_string(loopStack_.back().endLbl));
  emitLabel(freshLabel());
}

void CodeGen::visit(ContinueStmt& stmt) {
  if (loopStack_.empty()) {
    addError("'continue' outside loop", stmt.loc);
    return;
  }
  emit("  br label %lbl" + std::to_string(loopStack_.back().condLbl));
  emitLabel(freshLabel());
}

void CodeGen::visit(StructDecl& decl) {
  StructDef def;
  def.name = decl.name;
  for (int i = 0; i < (int)decl.fields.size(); ++i) {
    def.fields.push_back({decl.fields[i].name, decl.fields[i].type.kind, i});
  }
  structDefs_[decl.name] = def;
  std::ostringstream* saved = current_;
  current_ = &globals_;
  emit(llvmStructName(decl.name) + " = type " + buildStructLayout(def));
  current_ = saved;
}

void CodeGen::visit(FnDecl& decl) {
  nextReg_   = 0;
  nextLabel_ = 0;
  varScopes_.clear();
  varTypeMap_.clear();
  varStructTypeMap_.clear();
  pushVarScope();
  std::string paramStr;
  for (size_t i = 0; i < decl.params.size(); ++i) {
    if (i > 0) paramStr += ", ";
    TypeKind pk = decl.params[i].type.kind;
    if (pk == TypeKind::Struct) {
      paramStr += "ptr %" + decl.params[i].name;
    } else {
      paramStr += llvmType(pk) + " %" + decl.params[i].name;
    }
  }
  std::string retT = llvmType(decl.returnType.kind);
  emit("define " + retT + " @" + decl.name + "(" + paramStr + ") {");
  emit("entry:");
  for (auto& p : decl.params) {
    TypeKind pk = p.type.kind;
    int allocaReg = nextReg_++;
    if (pk == TypeKind::Struct) {
      emit("  %" + std::to_string(allocaReg) + " = alloca ptr");
      emit("  store ptr %" + p.name + ", ptr %" + std::to_string(allocaReg));
    } else {
      emit("  %" + std::to_string(allocaReg) + " = alloca " + llvmType(pk));
      emit("  store " + llvmType(pk) + " %" + p.name + ", ptr %" + std::to_string(allocaReg));
    }
    declareVar(p.name, allocaReg);
    varTypeMap_[p.name] = pk;
    if (pk == TypeKind::Struct) varStructTypeMap_[p.name] = p.type.name;
  }
  decl.body->accept(*this);
  if (decl.returnType.kind == TypeKind::Void) {
    emit("  ret void");
  }
  emit("}");
  emit("");
  popVarScope();
}

void CodeGen::visit(Program& program) {
  for (auto& imp : program.imports) imp->accept(*this);
  for (auto& s : program.structs) s->accept(*this);
  for (auto& fn : program.functions) {
    fnReturnTypes_[fn->name] = fn->returnType.kind;
  }
  for (auto& fn : program.functions) fn->accept(*this);
  if (!program.topLevelStmts.empty()) {
    emitTopLevelMain(program);
  }
}

void CodeGen::emitTopLevelMain(Program& program) {
  nextReg_   = 0;
  nextLabel_ = 0;
  varScopes_.clear();
  varTypeMap_.clear();
  varStructTypeMap_.clear();
  pushVarScope();
  emit("define i32 @main() {");
  emit("entry:");
  for (auto& stmt : program.topLevelStmts) stmt->accept(*this);
  emit("  ret i32 0");
  emit("}");
  emit("");
  popVarScope();
}

void CodeGen::visit(StringInterpExpr& expr) {
  addError("string interpolation is not yet supported in native codegen (use ox run)", expr.loc);
  int idx = addStringConstant("");
  int reg = freshReg();
  emit("  %" + std::to_string(reg) + " = getelementptr [1 x i8], ptr @str" + std::to_string(idx) + ", i32 0, i32 0");
  lastReg_  = reg;
  lastType_ = TypeKind::String;
}

void CodeGen::visit(ImportDecl&) {}

} // namespace oxide
