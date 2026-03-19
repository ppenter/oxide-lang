// type_checker.cpp — Oxide Type Checker Implementation
// Phase 2: Language Expansion (structs, for-loops, break/continue)

#include "type_checker.h"
#include <cassert>

namespace oxide {

TypeChecker::TypeChecker(std::string filename)
    : filename_(std::move(filename)) {
  fnTable_.reserve(32);
  allDeclared_.reserve(64);
  structTable_.reserve(16);
  structVarNames_.reserve(32);
  scopes_.reserve(8);

  auto registerBuiltin = [&](const std::string& name,
                              std::vector<TypeKind> pts,
                              std::vector<std::string> pns,
                              TypeKind ret) {
    FnSig sig;
    sig.paramTypes = std::move(pts);
    sig.paramNames = std::move(pns);
    sig.returnType = ret;
    fnTable_[name] = std::move(sig);
  };

  registerBuiltin("server_listen",
    {TypeKind::String, TypeKind::Int}, {"host", "port"}, TypeKind::Void);
  registerBuiltin("getenv_int",
    {TypeKind::String, TypeKind::Int}, {"name", "default"}, TypeKind::Int);
  registerBuiltin("getenv",
    {TypeKind::String, TypeKind::String}, {"name", "default"}, TypeKind::String);
  registerBuiltin("server_start",
    {TypeKind::String}, {"handler_name"}, TypeKind::Void);
  registerBuiltin("router_get",
    {TypeKind::String, TypeKind::String}, {"path", "handler"}, TypeKind::Void);
  registerBuiltin("router_post",
    {TypeKind::String, TypeKind::String}, {"path", "handler"}, TypeKind::Void);
  registerBuiltin("router_put",
    {TypeKind::String, TypeKind::String}, {"path", "handler"}, TypeKind::Void);
  registerBuiltin("router_delete",
    {TypeKind::String, TypeKind::String}, {"path", "handler"}, TypeKind::Void);
  registerBuiltin("router_patch",
    {TypeKind::String, TypeKind::String}, {"path", "handler"}, TypeKind::Void);
  registerBuiltin("router_start",
    {TypeKind::String, TypeKind::Int}, {"host", "port"}, TypeKind::Void);
  registerBuiltin("int_to_string",
    {TypeKind::Int}, {"x"}, TypeKind::String);
  registerBuiltin("float_to_string",
    {TypeKind::Float}, {"x"}, TypeKind::String);
  registerBuiltin("bool_to_string",
    {TypeKind::Bool}, {"x"}, TypeKind::String);
  registerBuiltin("string_len",
    {TypeKind::String}, {"s"}, TypeKind::Int);
  registerBuiltin("string_concat",
    {TypeKind::String, TypeKind::String}, {"a", "b"}, TypeKind::String);
}

bool TypeChecker::check(Program& program) {
  pushScope();
  program.accept(*this);
  popScope();
  return errors_.empty();
}

static std::string typeKindName(TypeKind k) {
  switch (k) {
  case TypeKind::Int:     return "int";
  case TypeKind::Float:   return "float";
  case TypeKind::Bool:    return "bool";
  case TypeKind::String:  return "string";
  case TypeKind::Void:    return "void";
  case TypeKind::Struct:  return "struct";
  case TypeKind::Unknown: return "unknown";
  }
  return "?";
}

const Symbol* TypeChecker::getSymbol(const std::string& name) const {
  auto it = allDeclared_.find(name);
  if (it != allDeclared_.end()) return &it->second;
  return nullptr;
}

const TypeChecker::FnSig* TypeChecker::getFn(const std::string& name) const {
  auto it = fnTable_.find(name);
  if (it != fnTable_.end()) return &it->second;
  return nullptr;
}

const TypeChecker::StructInfo* TypeChecker::getStruct(const std::string& name) const {
  auto it = structTable_.find(name);
  if (it != structTable_.end()) return &it->second;
  return nullptr;
}

std::string TypeChecker::getStructVarName(const std::string& varName) const {
  auto it = structVarNames_.find(varName);
  if (it != structVarNames_.end()) return it->second;
  return "";
}

void TypeChecker::pushScope() { scopes_.push_back({}); }

void TypeChecker::popScope() {
  if (!scopes_.empty()) scopes_.pop_back();
}

void TypeChecker::declare(const std::string& name, TypeKind type, bool isConst) {
  if (scopes_.empty()) return;
  Symbol sym{name, type, isConst, true};
  scopes_.back()[name] = sym;
  allDeclared_[name] = sym;
}

Symbol* TypeChecker::lookup(const std::string& name) {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) return &found->second;
  }
  return nullptr;
}

void TypeChecker::addError(const std::string& msg, const SourceLoc& loc) {
  errors_.push_back(filename_ + ":" + std::to_string(loc.line) + ":" +
                    std::to_string(loc.column) + ": type error: " + msg);
}

bool TypeChecker::validateTypeAnnotation(const TypeAnnotation& ann,
                                          const SourceLoc& loc) {
  if (ann.kind == TypeKind::Struct) {
    if (structTable_.find(ann.name) != structTable_.end()) return true;
    Symbol* sym = lookup(ann.name);
    if (sym && sym->type == TypeKind::Unknown) return true;
    addError("unknown type '" + ann.name + "'", loc);
    return false;
  }
  if (ann.kind == TypeKind::Unknown && !ann.name.empty()) {
    Symbol* sym = lookup(ann.name);
    if (sym && sym->type == TypeKind::Unknown) return true;
    addError("unknown type '" + ann.name + "'", loc);
    return false;
  }
  return true;
}

std::string TypeChecker::typeName(TypeKind k) {
  switch (k) {
  case TypeKind::Int:     return "int";
  case TypeKind::Float:   return "float";
  case TypeKind::Bool:    return "bool";
  case TypeKind::String:  return "string";
  case TypeKind::Void:    return "void";
  case TypeKind::Struct:  return "struct";
  case TypeKind::Unknown: return "unknown";
  }
  return "?";
}

TypeKind TypeChecker::binaryResultType(const std::string& op,
                                        TypeKind l, TypeKind r,
                                        const SourceLoc& loc) {
  if (op == "==" || op == "!=" || op == "<" || op == "<=" ||
      op == ">"  || op == ">=" || op == "&&" || op == "||") {
    return TypeKind::Bool;
  }

  if (l == TypeKind::Unknown || r == TypeKind::Unknown)
    return TypeKind::Unknown;

  if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
    if (l == TypeKind::String && op == "+" && r == TypeKind::String)
      return TypeKind::String;
    if (l == TypeKind::String && op == "+")
      return TypeKind::String;
    if (r == TypeKind::String && op == "+")
      return TypeKind::String;
    if ((l == TypeKind::Int || l == TypeKind::Float) &&
        (r == TypeKind::Int || r == TypeKind::Float)) {
      return (l == TypeKind::Float || r == TypeKind::Float)
                 ? TypeKind::Float
                 : TypeKind::Int;
    }
    addError("operator '" + op + "' not applicable to types '" +
             typeName(l) + "' and '" + typeName(r) + "'", loc);
    return TypeKind::Unknown;
  }
  return TypeKind::Unknown;
}

void TypeChecker::visit(IntLiteralExpr&)    { lastType_ = TypeKind::Int; }
void TypeChecker::visit(FloatLiteralExpr&)  { lastType_ = TypeKind::Float; }
void TypeChecker::visit(BoolLiteralExpr&)   { lastType_ = TypeKind::Bool; }
void TypeChecker::visit(StringLiteralExpr&) { lastType_ = TypeKind::String; }

void TypeChecker::visit(StringInterpExpr& expr) {
  for (auto& part : expr.exprParts) {
    part->accept(*this);
    TypeKind t = lastType_;
    if (t != TypeKind::Int && t != TypeKind::Float &&
        t != TypeKind::Bool && t != TypeKind::String &&
        t != TypeKind::Unknown) {
      addError("string interpolation expression must be int, float, bool, or string",
               part->loc);
    }
  }
  lastType_ = TypeKind::String;
}

void TypeChecker::visit(IdentifierExpr& expr) {
  Symbol* sym = lookup(expr.name);
  if (!sym) {
    addError("undefined variable '" + expr.name + "'", expr.loc);
    lastType_ = TypeKind::Unknown;
    lastStructName_.clear();
  } else {
    lastType_ = sym->type;
    if (sym->type == TypeKind::Struct) {
      auto it = structVarNames_.find(expr.name);
      lastStructName_ = (it != structVarNames_.end()) ? it->second : "";
    } else {
      lastStructName_.clear();
    }
  }
}

void TypeChecker::visit(BinaryExpr& expr) {
  expr.lhs->accept(*this);
  TypeKind lType = lastType_;
  expr.rhs->accept(*this);
  TypeKind rType = lastType_;
  lastStructName_.clear();
  lastType_ = binaryResultType(expr.op, lType, rType, expr.loc);
}

void TypeChecker::visit(UnaryExpr& expr) {
  expr.operand->accept(*this);
  TypeKind t = lastType_;
  lastStructName_.clear();
  if (expr.op == "-") {
    if (t != TypeKind::Int && t != TypeKind::Float) {
      addError("unary '-' requires numeric operand, got '" + typeName(t) + "'",
               expr.loc);
      lastType_ = TypeKind::Unknown;
    }
  } else if (expr.op == "!") {
    if (t != TypeKind::Bool && t != TypeKind::Unknown) {
      addError("unary '!' requires bool operand, got '" + typeName(t) + "'",
               expr.loc);
    }
    lastType_ = TypeKind::Bool;
  }
}

void TypeChecker::visit(CallExpr& expr) {
  auto it = fnTable_.find(expr.callee);
  if (it == fnTable_.end()) {
    Symbol* sym = lookup(expr.callee);
    if (sym && sym->type == TypeKind::Unknown) {
      for (auto& arg : expr.args) arg->accept(*this);
      lastType_ = TypeKind::Unknown;
      lastStructName_.clear();
      return;
    }
    addError("call to undefined function '" + expr.callee + "'", expr.loc);
    lastType_ = TypeKind::Unknown;
    lastStructName_.clear();
    return;
  }
  const FnSig& sig = it->second;

  if (expr.args.size() != sig.paramTypes.size()) {
    addError("function '" + expr.callee + "' expects " +
             std::to_string(sig.paramTypes.size()) + " argument(s), got " +
             std::to_string(expr.args.size()), expr.loc);
  }

  for (size_t i = 0; i < expr.args.size(); ++i) {
    expr.args[i]->accept(*this);
    TypeKind argType = lastType_;
    if (i < sig.paramTypes.size() && argType != sig.paramTypes[i] &&
        argType != TypeKind::Unknown && sig.paramTypes[i] != TypeKind::Unknown) {
      addError("argument " + std::to_string(i + 1) + " to '" + expr.callee +
               "' expects '" + typeName(sig.paramTypes[i]) + "', got '" +
               typeName(argType) + "'", expr.loc);
    }
  }
  lastType_ = sig.returnType;
  lastStructName_.clear();
}

void TypeChecker::visit(AssignExpr& expr) {
  Symbol* sym = lookup(expr.name);
  if (!sym) {
    addError("assignment to undefined variable '" + expr.name + "'", expr.loc);
    lastType_ = TypeKind::Unknown;
    lastStructName_.clear();
    return;
  }
  if (sym->isConst) {
    addError("cannot assign to const variable '" + expr.name + "'", expr.loc);
  }
  expr.value->accept(*this);
  TypeKind valType = lastType_;
  if (valType != sym->type && valType != TypeKind::Unknown &&
      sym->type != TypeKind::Unknown) {
    addError("cannot assign '" + typeName(valType) + "' to variable '" +
             expr.name + "' of type '" + typeName(sym->type) + "'", expr.loc);
  }
  lastType_ = sym->type;
  lastStructName_.clear();
}

void TypeChecker::visit(FieldAccessExpr& expr) {
  expr.object->accept(*this);
  TypeKind objType = lastType_;
  std::string structName = lastStructName_;

  lastStructName_.clear();

  if (objType == TypeKind::Unknown) {
    lastType_ = TypeKind::Unknown;
    return;
  }

  if (objType != TypeKind::Struct || structName.empty()) {
    addError("field access '" + expr.field +
             "' on non-struct type '" + typeName(objType) + "'", expr.loc);
    lastType_ = TypeKind::Unknown;
    return;
  }

  auto it = structTable_.find(structName);
  if (it == structTable_.end()) {
    Symbol* sym = lookup(structName);
    if (sym && sym->type == TypeKind::Unknown) {
      lastType_ = TypeKind::Unknown;
      return;
    }
    addError("unknown struct type '" + structName + "'", expr.loc);
    lastType_ = TypeKind::Unknown;
    return;
  }
  const StructInfo& info = it->second;
  for (auto& [fname, ftype] : info.fields) {
    if (fname == expr.field) {
      lastType_ = ftype;
      return;
    }
  }
  addError("struct '" + structName + "' has no field '" + expr.field + "'",
           expr.loc);
  lastType_ = TypeKind::Unknown;
}

void TypeChecker::visit(StructLiteralExpr& expr) {
  auto it = structTable_.find(expr.typeName);
  if (it == structTable_.end()) {
    Symbol* sym = lookup(expr.typeName);
    if (sym && sym->type == TypeKind::Unknown) {
      for (auto& fi : expr.fields) fi.value->accept(*this);
      lastType_ = TypeKind::Struct;
      lastStructName_ = expr.typeName;
      return;
    }
    addError("unknown struct type '" + expr.typeName + "'", expr.loc);
    lastType_ = TypeKind::Unknown;
    lastStructName_.clear();
    return;
  }
  const StructInfo& info = it->second;

  for (auto& fi : expr.fields) {
    bool found = false;
    TypeKind expectedType = TypeKind::Unknown;
    for (auto& [fname, ftype] : info.fields) {
      if (fname == fi.name) {
        found = true;
        expectedType = ftype;
        break;
      }
    }
    if (!found) {
      addError("struct '" + expr.typeName + "' has no field '" + fi.name + "'",
               expr.loc);
    }
    fi.value->accept(*this);
    TypeKind valType = lastType_;
    if (found && expectedType != TypeKind::Unknown &&
        valType != expectedType && valType != TypeKind::Unknown) {
      addError("field '" + fi.name + "' expects '" + typeName(expectedType) +
               "', got '" + typeName(valType) + "'", expr.loc);
    }
  }

  lastType_ = TypeKind::Struct;
  lastStructName_ = expr.typeName;
}

void TypeChecker::visit(LetStmt& stmt) {
  TypeKind declaredType = TypeKind::Unknown;
  std::string declaredStructName;

  if (stmt.hasType) {
    if (stmt.typeAnnotation.kind == TypeKind::Struct) {
      bool knownStruct = structTable_.find(stmt.typeAnnotation.name) != structTable_.end();
      if (!knownStruct) {
        Symbol* sym = lookup(stmt.typeAnnotation.name);
        knownStruct = sym && sym->type == TypeKind::Unknown;
      }
      if (!knownStruct) {
        addError("unknown type '" + stmt.typeAnnotation.name + "'", stmt.loc);
      } else {
        declaredType = TypeKind::Struct;
        declaredStructName = stmt.typeAnnotation.name;
      }
    } else if (validateTypeAnnotation(stmt.typeAnnotation, stmt.loc)) {
      declaredType = stmt.typeAnnotation.kind;
    }
  }

  TypeKind initType = TypeKind::Unknown;
  if (stmt.initializer) {
    stmt.initializer->accept(*this);
    initType = lastType_;
    if (initType == TypeKind::Struct) {
      declaredStructName = lastStructName_;
    }
  }

  TypeKind finalType = declaredType;
  if (finalType == TypeKind::Unknown) finalType = initType;

  if (declaredType != TypeKind::Unknown && initType != TypeKind::Unknown &&
      declaredType != initType) {
    addError("declared type '" + typeName(declaredType) +
             "' but initializer has type '" + typeName(initType) + "'", stmt.loc);
  }

  declare(stmt.name, finalType, false);
  if (finalType == TypeKind::Struct && !declaredStructName.empty()) {
    structVarNames_[stmt.name] = declaredStructName;
  }
}

void TypeChecker::visit(ConstStmt& stmt) {
  TypeKind declaredType = TypeKind::Unknown;
  if (stmt.hasType) {
    if (validateTypeAnnotation(stmt.typeAnnotation, stmt.loc)) {
      declaredType = stmt.typeAnnotation.kind;
    }
  }

  TypeKind initType = TypeKind::Unknown;
  if (stmt.initializer) {
    stmt.initializer->accept(*this);
    initType = lastType_;
  }

  TypeKind finalType = declaredType;
  if (finalType == TypeKind::Unknown) finalType = initType;

  if (declaredType != TypeKind::Unknown && initType != TypeKind::Unknown &&
      declaredType != initType) {
    addError("declared type '" + typeName(declaredType) +
             "' but initializer has type '" + typeName(initType) + "'", stmt.loc);
  }

  declare(stmt.name, finalType, true);
}

void TypeChecker::visit(ReturnStmt& stmt) {
  TypeKind retType = TypeKind::Void;
  if (stmt.value) {
    stmt.value->accept(*this);
    retType = lastType_;
  }
  if (retType != currentFnReturnType_ &&
      retType != TypeKind::Unknown &&
      currentFnReturnType_ != TypeKind::Unknown) {
    if (!(retType == TypeKind::Int && currentFnReturnType_ == TypeKind::Float)) {
      addError("return type mismatch: expected '" + typeName(currentFnReturnType_) +
               "', got '" + typeName(retType) + "'", stmt.loc);
    }
  }
}

void TypeChecker::visit(PrintStmt& stmt) {
  stmt.value->accept(*this);
}

void TypeChecker::visit(ExprStmt& stmt) {
  stmt.expr->accept(*this);
}

void TypeChecker::visit(BlockStmt& stmt) {
  pushScope();
  for (auto& s : stmt.stmts) s->accept(*this);
  popScope();
}

void TypeChecker::visit(IfStmt& stmt) {
  stmt.condition->accept(*this);
  TypeKind condType = lastType_;
  if (condType != TypeKind::Bool && condType != TypeKind::Unknown) {
    addError("if condition must be bool, got '" + typeName(condType) + "'",
             stmt.loc);
  }
  stmt.thenBlock->accept(*this);
  if (stmt.elseBlock) stmt.elseBlock->accept(*this);
}

void TypeChecker::visit(WhileStmt& stmt) {
  stmt.condition->accept(*this);
  TypeKind condType = lastType_;
  if (condType != TypeKind::Bool && condType != TypeKind::Unknown) {
    addError("while condition must be bool, got '" + typeName(condType) + "'",
             stmt.loc);
  }
  ++loopDepth_;
  stmt.body->accept(*this);
  --loopDepth_;
}

void TypeChecker::visit(ForStmt& stmt) {
  stmt.start->accept(*this);
  TypeKind startType = lastType_;
  if (startType != TypeKind::Int && startType != TypeKind::Unknown) {
    addError("for-in range start must be int, got '" + typeName(startType) + "'",
             stmt.loc);
  }
  stmt.end->accept(*this);
  TypeKind endType = lastType_;
  if (endType != TypeKind::Int && endType != TypeKind::Unknown) {
    addError("for-in range end must be int, got '" + typeName(endType) + "'",
             stmt.loc);
  }

  pushScope();
  declare(stmt.iterVar, TypeKind::Int, true);
  ++loopDepth_;
  stmt.body->accept(*this);
  --loopDepth_;
  popScope();
}

void TypeChecker::visit(BreakStmt& stmt) {
  if (loopDepth_ == 0) {
    addError("'break' used outside of a loop", stmt.loc);
  }
}

void TypeChecker::visit(ContinueStmt& stmt) {
  if (loopDepth_ == 0) {
    addError("'continue' used outside of a loop", stmt.loc);
  }
}

void TypeChecker::visit(FnDecl& decl) {
  validateTypeAnnotation(decl.returnType, decl.loc);
  for (auto& p : decl.params) validateTypeAnnotation(p.type, decl.loc);

  FnSig sig;
  for (auto& p : decl.params) {
    sig.paramTypes.push_back(p.type.kind);
    sig.paramNames.push_back(p.name);
  }
  sig.returnType = decl.returnType.kind;
  fnTable_[decl.name] = sig;

  TypeKind prevReturn = currentFnReturnType_;
  currentFnReturnType_ = decl.returnType.kind;

  pushScope();
  for (auto& p : decl.params) {
    declare(p.name, p.type.kind, false);
    if (p.type.kind == TypeKind::Struct && !p.type.name.empty()) {
      structVarNames_[p.name] = p.type.name;
    }
  }
  decl.body->accept(*this);
  popScope();

  currentFnReturnType_ = prevReturn;
}

void TypeChecker::visit(StructDecl& decl) {
  if (structTable_.find(decl.name) != structTable_.end()) {
    addError("struct '" + decl.name + "' already declared", decl.loc);
    return;
  }
  StructInfo info;
  info.name = decl.name;
  for (auto& f : decl.fields) {
    if (f.type.kind == TypeKind::Struct) {
      if (structTable_.find(f.type.name) == structTable_.end()) {
        addError("field '" + f.name + "' has unknown type '" + f.type.name + "'",
                 decl.loc);
      }
    } else {
      validateTypeAnnotation(f.type, decl.loc);
    }
    info.fields.push_back({f.name, f.type.kind});
  }
  structTable_[decl.name] = std::move(info);
}

void TypeChecker::visit(ImportDecl& decl) {
  switch (decl.kind) {
  case ImportKind::Named:
  case ImportKind::Type:
    for (auto& spec : decl.specifiers) {
      declare(spec.alias, TypeKind::Unknown, true);
    }
    break;
  case ImportKind::Star:
    if (!decl.starAlias.empty()) {
      declare(decl.starAlias, TypeKind::Unknown, true);
    }
    break;
  }
}

void TypeChecker::visit(Program& program) {
  for (auto& imp : program.imports) imp->accept(*this);
  for (auto& s : program.structs) s->accept(*this);
  for (auto& stmt : program.topLevelStmts) {
    if (auto* ls = dynamic_cast<LetStmt*>(stmt.get())) {
      TypeKind kind = TypeKind::Unknown;
      if (ls->hasType) kind = ls->typeAnnotation.kind;
      declare(ls->name, kind, false);
      if (kind == TypeKind::Struct && !ls->typeAnnotation.name.empty())
        structVarNames_[ls->name] = ls->typeAnnotation.name;
    } else if (auto* cs = dynamic_cast<ConstStmt*>(stmt.get())) {
      TypeKind kind = TypeKind::Unknown;
      if (cs->hasType) kind = cs->typeAnnotation.kind;
      declare(cs->name, kind, true);
    }
  }

  for (auto& fn : program.functions) {
    FnSig sig;
    for (auto& p : fn->params) {
      sig.paramTypes.push_back(p.type.kind);
      sig.paramNames.push_back(p.name);
    }
    sig.returnType = fn->returnType.kind;
    fnTable_[fn->name] = sig;
  }

  for (auto& fn : program.functions) fn->accept(*this);
  for (auto& stmt : program.topLevelStmts) stmt->accept(*this);
}

} // namespace oxide
