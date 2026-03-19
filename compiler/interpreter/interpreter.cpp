// interpreter.cpp — Oxide Tree-Walking Interpreter Implementation
// Phase 2: structs, for-loops, break/continue, module loading

#include "interpreter.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../oxx/oxx_preprocessor.h"
#include <cmath>
#include <cstdlib>   // std::getenv
#include <cstring>   // std::strerror, std::memset
#include <cerrno>    // errno, EADDRINUSE
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
// POSIX sockets — for server_listen() and server_start() built-ins
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>

namespace oxide {

// ─────────────────────────────────────────────────────────────
// OxValue::toString
// ─────────────────────────────────────────────────────────────

std::string OxValue::toString() const {
  switch (tag) {
  case Tag::Int:    return std::to_string(asInt);
  case Tag::Float: {
    std::ostringstream ss;
    ss << asFloat;
    return ss.str();
  }
  case Tag::Bool:   return asBool ? "true" : "false";
  case Tag::String: return asString;
  case Tag::Struct: {
    std::string s = asStructType + "{";
    // Reserve a generous initial capacity to avoid repeated reallocations
    // when concatenating many field name/value pairs.
    if (structFields && !structFields->empty()) {
      s.reserve(asStructType.size() + 2 + structFields->size() * 24);
    }
    bool first = true;
    if (structFields) {
      for (auto& [k, v] : *structFields) {
        if (!first) s += ", ";
        s += k + ": " + v.toString();
        first = false;
      }
    }
    s += "}";
    return s;
  }
  case Tag::Void:   return "<void>";
  }
  return "";
}

// ─────────────────────────────────────────────────────────────
// Control-flow signals (thrown to unwind the stack)
// ─────────────────────────────────────────────────────────────

struct BreakSignal    {};
struct ContinueSignal {};

// ─────────────────────────────────────────────────────────────
// Constructor / error helpers
// ─────────────────────────────────────────────────────────────

Interpreter::Interpreter(std::string filename)
    : filename_(std::move(filename)) {}

void Interpreter::addError(const std::string& msg, const SourceLoc& loc) {
  errors_.push_back(filename_ + ":" + std::to_string(loc.line) + ":" +
                    std::to_string(loc.column) + ": runtime error: " + msg);
}

// ─────────────────────────────────────────────────────────────
// Public entry point
// ─────────────────────────────────────────────────────────────

bool Interpreter::run(Program& program) {
  // Derive current directory from filename_ for relative import resolution
  {
    std::string fn = filename_;
    // Normalize: replace backslashes, strip trailing filename
    size_t slash = fn.find_last_of("/\\");
    currentDir_  = (slash != std::string::npos) ? fn.substr(0, slash) : ".";
  }

  env_ = &globalEnv_;
  try {
    program.accept(*this);
  } catch (const RuntimeError& e) {
    errors_.push_back(std::string("runtime error: ") + e.what());
    return false;
  }
  return errors_.empty();
}

// ─────────────────────────────────────────────────────────────
// Module system helpers
// ─────────────────────────────────────────────────────────────

/// Returns directory component of a path.
static std::string dirOf(const std::string& path) {
  size_t slash = path.find_last_of("/\\");
  return (slash != std::string::npos) ? path.substr(0, slash) : ".";
}

/// Join a directory and a relative path (handling ./  and  ../)
static std::string joinPath(const std::string& base, const std::string& rel) {
  if (rel.empty()) return base;
  // If rel is already absolute, return as-is
  if (rel[0] == '/') return rel;
  std::string result = base + "/" + rel;
  // Simplistic normalization: collapse internal ./
  // (A full realpath() would be cleaner but adds a POSIX dependency)
  return result;
}

std::string Interpreter::resolveImportPath(const std::string& importPath) const {
  // Skip standard library modules — not resolved from disk yet
  if (importPath.rfind("@oxide/", 0) == 0) return "";

  std::string path = importPath;
  bool aliasApplied = false;

  // Apply path aliases from oxide.config.json (e.g. "@/" → "examples/webapp/./src/")
  // The replacement value is already expressed relative to the process working directory
  // (it was resolved in loadPathAliases() as configDir + "/" + value).
  for (auto& [alias, replacement] : pathAliases_) {
    if (path.rfind(alias, 0) == 0) {
      path = replacement + path.substr(alias.size());
      aliasApplied = true;
      break;
    }
  }

  // Absolute path — return as-is
  if (!path.empty() && path[0] == '/') return path;

  // Alias-replaced path: already relative to CWD (not to currentDir_)
  // Return directly — do NOT join with currentDir_ again.
  if (aliasApplied) return path;

  // Explicit relative import (./foo  or  ../bar): resolve relative to the
  // directory of the file that contains the import statement.
  if (path.rfind("./", 0) == 0 || path.rfind("../", 0) == 0) {
    return joinPath(currentDir_, path);
  }

  // Bare path (no ./ or ../ prefix, no alias): resolve relative to currentDir_
  return joinPath(currentDir_, path);
}

bool Interpreter::loadModule(const std::string& resolvedPath,
                              const SourceLoc& loc) {
  // Read the source file
  std::ifstream ifs(resolvedPath);
  if (!ifs.is_open()) {
    addError("cannot open module '" + resolvedPath + "'", loc);
    return false;
  }
  std::string src((std::istreambuf_iterator<char>(ifs)),
                   std::istreambuf_iterator<char>());

  // Preprocess .oxx files: convert JSX template syntax → Oxide string expressions
  bool isOxx = resolvedPath.size() >= 4 &&
               resolvedPath.substr(resolvedPath.size() - 4) == ".oxx";
  if (isOxx) {
    OxxPreprocessor pp;
    src = pp.preprocess(src);
  }

  // Lex
  Lexer lexer(src, resolvedPath);
  auto tokens = lexer.tokenize();
  if (!lexer.errors().empty()) {
    for (auto& e : lexer.errors()) errors_.push_back(e);
    return false;
  }

  // Parse
  Parser parser(std::move(tokens), resolvedPath);
  auto program = parser.parse();
  if (!parser.errors().empty()) {
    for (auto& e : parser.errors()) errors_.push_back(e);
    return false;
  }

  // Save interpreter state so we don't corrupt the caller's context
  std::string savedDir = currentDir_;
  Env* savedEnv = env_;

  // Each module runs in its own fresh global env (child of the main global
  // so that top-level functions defined in modules can call each other)
  Env moduleGlobal(&globalEnv_);
  env_        = &moduleGlobal;
  currentDir_ = dirOf(resolvedPath);

  // Execute the module: imports first, then structs, then fns, then stmts
  try {
    for (auto& imp  : program->imports)        imp->accept(*this);
    for (auto& s    : program->structs)         s->accept(*this);
    for (auto& fn   : program->functions)       functions_[fn->name] = fn.get();
    for (auto& stmt : program->topLevelStmts)   stmt->accept(*this);
  } catch (const RuntimeError& e) {
    errors_.push_back(std::string("runtime error in module '") +
                      resolvedPath + "': " + e.what());
    env_        = savedEnv;
    currentDir_ = savedDir;
    return false;
  }

  // Build module export record.
  // Use the AST declarations directly — NOT a before/after diff — so that
  // modules sharing function names (e.g. multiple .oxx files each exporting
  // `render`) are all correctly exported regardless of load order.
  ModuleExports exports;
  exports.vars = moduleGlobal.vars; // snapshot of module-level variables

  for (auto& fn : program->functions)
    exports.functionNames.insert(fn->name);
  for (auto& s : program->structs)
    exports.structNames.insert(s->name);

  // Hoist module-level variables into globalEnv_ so that functions defined
  // in the module can access them when called later (fn envs have globalEnv_
  // as parent, but module env is a transient local — we keep the vars alive
  // by promoting them to the true global scope).
  for (auto& [name, val] : moduleGlobal.vars) {
    globalEnv_.set(name, val);
  }

  moduleCache_[resolvedPath]   = std::move(exports);
  ownedModules_.push_back(std::move(program));

  // Restore caller context
  env_        = savedEnv;
  currentDir_ = savedDir;
  return true;
}

void Interpreter::bindImportedNames(const std::string& resolvedPath,
                                     const ImportDecl& decl) {
  auto& exports = moduleCache_.at(resolvedPath);

  auto bindSpec = [&](const std::string& name, const std::string& alias) {
    // Variable export
    auto vit = exports.vars.find(name);
    if (vit != exports.vars.end()) {
      env_->set(alias, vit->second);
      return;
    }
    // Function export — already in functions_; create alias if needed
    if (exports.functionNames.count(name)) {
      if (alias != name && functions_.count(name))
        functions_[alias] = functions_[name];
      return;
    }
    // Struct export — already in structLayouts_; create alias if needed
    if (exports.structNames.count(name)) {
      if (alias != name && structLayouts_.count(name))
        structLayouts_[alias] = structLayouts_[name];
      return;
    }
    // Not found — emit a warning but don't hard-fail
    errors_.push_back("warning: '" + name + "' not found in module '" +
                      resolvedPath + "'");
  };

  switch (decl.kind) {
  case ImportKind::Named:
  case ImportKind::Type: // type-only: bind struct names, no runtime vars
    for (auto& spec : decl.specifiers)
      bindSpec(spec.name, spec.alias);
    break;
  case ImportKind::Star:
    // Bind everything under the namespace alias
    for (auto& [name, val] : exports.vars)
      env_->set(decl.starAlias + "_" + name, val); // flattened for now
    for (auto& name : exports.functionNames) {
      if (decl.starAlias + "_" + name != name)
        functions_[decl.starAlias + "_" + name] = functions_[name];
    }
    break;
  }
}

// ─────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────

void Interpreter::execBlock(BlockStmt& block, Env* parentEnv) {
  Env blockEnv(parentEnv);
  Env* prev = env_;
  env_ = &blockEnv;
  try {
    for (auto& stmt : block.stmts) stmt->accept(*this);
  } catch (...) {
    env_ = prev;
    throw;
  }
  env_ = prev;
}

// ─────────────────────────────────────────────────────────────
// Binary / unary operators
// ─────────────────────────────────────────────────────────────

OxValue Interpreter::applyBinary(const std::string& op,
                                   OxValue& l, OxValue& r,
                                   const SourceLoc& loc) {
  (void)loc;
  if (op == "&&") return OxValue::makeBool(l.asBool && r.asBool);
  if (op == "||") return OxValue::makeBool(l.asBool || r.asBool);

  // String concatenation — auto-coerce non-strings to their string representation
  if (op == "+" && (l.tag == OxValue::Tag::String || r.tag == OxValue::Tag::String)) {
    return OxValue::makeString(l.toString() + r.toString());
  }

  // String equality / inequality (must check before falling through to numeric)
  if (l.tag == OxValue::Tag::String || r.tag == OxValue::Tag::String) {
    if (op == "==") return OxValue::makeBool(l.toString() == r.toString());
    if (op == "!=") return OxValue::makeBool(l.toString() != r.toString());
    if (op == "<")  return OxValue::makeBool(l.toString() <  r.toString());
    if (op == "<=") return OxValue::makeBool(l.toString() <= r.toString());
    if (op == ">")  return OxValue::makeBool(l.toString() >  r.toString());
    if (op == ">=") return OxValue::makeBool(l.toString() >= r.toString());
  }

  // Bool equality
  if (l.tag == OxValue::Tag::Bool || r.tag == OxValue::Tag::Bool) {
    if (op == "==") return OxValue::makeBool(l.asBool == r.asBool);
    if (op == "!=") return OxValue::makeBool(l.asBool != r.asBool);
  }

  // Numeric ops
  bool isFloat = (l.tag == OxValue::Tag::Float || r.tag == OxValue::Tag::Float);
  double lv = (l.tag == OxValue::Tag::Float) ? l.asFloat : (double)l.asInt;
  double rv = (r.tag == OxValue::Tag::Float) ? r.asFloat : (double)r.asInt;

  if (op == "+") return isFloat ? OxValue::makeFloat(lv + rv) : OxValue::makeInt(l.asInt + r.asInt);
  if (op == "-") return isFloat ? OxValue::makeFloat(lv - rv) : OxValue::makeInt(l.asInt - r.asInt);
  if (op == "*") return isFloat ? OxValue::makeFloat(lv * rv) : OxValue::makeInt(l.asInt * r.asInt);
  if (op == "/") {
    if (!isFloat && r.asInt == 0) throw RuntimeError("division by zero");
    return isFloat ? OxValue::makeFloat(lv / rv) : OxValue::makeInt(l.asInt / r.asInt);
  }
  if (op == "%") {
    if (r.asInt == 0) throw RuntimeError("modulo by zero");
    return OxValue::makeInt(l.asInt % r.asInt);
  }

  // Comparisons
  if (op == "==") return isFloat ? OxValue::makeBool(lv == rv) : OxValue::makeBool(l.asInt == r.asInt);
  if (op == "!=") return isFloat ? OxValue::makeBool(lv != rv) : OxValue::makeBool(l.asInt != r.asInt);
  if (op == "<")  return isFloat ? OxValue::makeBool(lv <  rv) : OxValue::makeBool(l.asInt <  r.asInt);
  if (op == "<=") return isFloat ? OxValue::makeBool(lv <= rv) : OxValue::makeBool(l.asInt <= r.asInt);
  if (op == ">")  return isFloat ? OxValue::makeBool(lv >  rv) : OxValue::makeBool(l.asInt >  r.asInt);
  if (op == ">=") return isFloat ? OxValue::makeBool(lv >= rv) : OxValue::makeBool(l.asInt >= r.asInt);

  throw RuntimeError("unknown operator '" + op + "'");
}

OxValue Interpreter::applyUnary(const std::string& op, OxValue& v,
                                  const SourceLoc& loc) {
  (void)loc;
  if (op == "-") {
    if (v.tag == OxValue::Tag::Float) return OxValue::makeFloat(-v.asFloat);
    return OxValue::makeInt(-v.asInt);
  }
  if (op == "!") return OxValue::makeBool(!v.asBool);
  throw RuntimeError("unknown unary operator '" + op + "'");
}

// ─────────────────────────────────────────────────────────────
// HTTP server helpers (for server_listen / server_start built-ins)
// ─────────────────────────────────────────────────────────────

// Volatile flag set by SIGINT handler so server_start() can shut down cleanly.
static volatile sig_atomic_t g_oxide_server_stop = 0;
static void oxide_sigint_handler(int) { g_oxide_server_stop = 1; }

// Call a user-defined Oxide function with pre-evaluated args.
// Used by server_start() to invoke the request handler for each HTTP request.
OxValue Interpreter::callUserFunction(FnDecl* fn,
                                       const std::vector<OxValue>& args) {
  Env fnEnv(&globalEnv_);
  for (size_t i = 0; i < fn->params.size() && i < args.size(); ++i)
    fnEnv.set(fn->params[i].name, args[i]);

  Env* prev = env_;
  env_ = &fnEnv;
  result_ = OxValue::makeVoid();

  try {
    fn->body->accept(*this);
  } catch (ReturnSignal& ret) {
    result_ = ret.value;
  } catch (...) {
    env_ = prev;
    throw;
  }
  env_ = prev;
  return result_;
}

// ─────────────────────────────────────────────────────────────
// Visitor — Expressions
// ─────────────────────────────────────────────────────────────

void Interpreter::visit(IntLiteralExpr& e)    { result_ = OxValue::makeInt(e.value); }
void Interpreter::visit(FloatLiteralExpr& e)  { result_ = OxValue::makeFloat(e.value); }
void Interpreter::visit(BoolLiteralExpr& e)   { result_ = OxValue::makeBool(e.value); }
void Interpreter::visit(StringLiteralExpr& e) { result_ = OxValue::makeString(e.value); }

void Interpreter::visit(StringInterpExpr& e) {
  // Build the interpolated string by concatenating text segments and expression results.
  // textParts[0] + str(exprParts[0]) + textParts[1] + str(exprParts[1]) + ... + textParts[N]
  std::string out;
  for (size_t i = 0; i < e.exprParts.size(); ++i) {
    out += e.textParts[i];          // literal text segment
    e.exprParts[i]->accept(*this);  // evaluate expression
    out += result_.toString();      // auto-coerce to string
  }
  // Append the final text segment (always present, may be empty)
  if (!e.textParts.empty()) out += e.textParts.back();
  result_ = OxValue::makeString(out);
}

void Interpreter::visit(IdentifierExpr& e) {
  OxValue* val = env_->get(e.name);
  if (!val) throw RuntimeError("undefined variable '" + e.name + "'");
  result_ = *val;
}

void Interpreter::visit(BinaryExpr& e) {
  e.lhs->accept(*this);
  OxValue l = result_;
  e.rhs->accept(*this);
  OxValue r = result_;
  result_ = applyBinary(e.op, l, r, e.loc);
}

void Interpreter::visit(UnaryExpr& e) {
  e.operand->accept(*this);
  result_ = applyUnary(e.op, result_, e.loc);
}

void Interpreter::visit(CallExpr& e) {
  // [CallExpr implementation - first 50 lines abbreviated for space]
  std::vector<OxValue> args;
  for (auto& arg : e.args) {
    arg->accept(*this);
    args.push_back(result_);
  }
  // Built-in function handling and user function calls...
  auto it = functions_.find(e.callee);
  if (it == functions_.end())
    throw RuntimeError("undefined function '" + e.callee + "'");
  FnDecl* fn = it->second;
  if (args.size() != fn->params.size()) {
    throw RuntimeError("function '" + e.callee + "' expects " +
                       std::to_string(fn->params.size()) + " argument(s), got " +
                       std::to_string(args.size()));
  }
  Env fnEnv(&globalEnv_);
  for (size_t i = 0; i < fn->params.size(); ++i)
    fnEnv.set(fn->params[i].name, args[i]);
  Env* prev = env_;
  env_ = &fnEnv;
  result_ = OxValue::makeVoid();
  try {
    fn->body->accept(*this);
  } catch (ReturnSignal& ret) {
    result_ = ret.value;
  } catch (...) {
    env_ = prev;
    throw;
  }
  env_ = prev;
}

void Interpreter::visit(AssignExpr& e) {
  e.value->accept(*this);
  OxValue val = result_;
  if (!env_->assign(e.name, val))
    throw RuntimeError("assignment to undefined variable '" + e.name + "'");
  result_ = val;
}

void Interpreter::visit(FieldAccessExpr& expr) {
  expr.object->accept(*this);
  OxValue obj = result_;
  if (obj.tag != OxValue::Tag::Struct) {
    throw RuntimeError("field access '.'" + expr.field +
                       "' on non-struct value of type " + obj.toString());
  }
  if (!obj.structFields) {
    throw RuntimeError("struct '" + obj.asStructType + "' has no fields (null)");
  }
  auto it = obj.structFields->find(expr.field);
  if (it == obj.structFields->end()) {
    throw RuntimeError("struct '" + obj.asStructType + "' has no field '" +
                       expr.field + "'");
  }
  result_ = it->second;
}

void Interpreter::visit(StructLiteralExpr& expr) {
  OxValue sv;
  sv.tag          = OxValue::Tag::Struct;
  sv.asStructType = expr.typeName;
  sv.structFields = std::make_shared<std::unordered_map<std::string, OxValue>>();
  for (auto& fi : expr.fields) {
    fi.value->accept(*this);
    (*sv.structFields)[fi.name] = result_;
  }
  result_ = std::move(sv);
}

void Interpreter::visit(LetStmt& s) {
  OxValue val = OxValue::makeVoid();
  if (s.initializer) {
    s.initializer->accept(*this);
    val = result_;
  }
  env_->set(s.name, val);
}

void Interpreter::visit(ConstStmt& s) {
  s.initializer->accept(*this);
  env_->set(s.name, result_);
}

void Interpreter::visit(ReturnStmt& s) {
  if (s.value) {
    s.value->accept(*this);
    throw ReturnSignal{result_};
  }
  throw ReturnSignal{OxValue::makeVoid()};
}

void Interpreter::visit(PrintStmt& s) {
  s.value->accept(*this);
  std::cout << result_.toString() << "\n";
}

void Interpreter::visit(ExprStmt& s)   { s.expr->accept(*this); }
void Interpreter::visit(BlockStmt& s)  { execBlock(s, env_); }

void Interpreter::visit(IfStmt& s) {
  s.condition->accept(*this);
  if (result_.asBool) {
    execBlock(*s.thenBlock, env_);
  } else if (s.elseBlock) {
    execBlock(*s.elseBlock, env_);
  }
}

void Interpreter::visit(WhileStmt& s) {
  while (true) {
    s.condition->accept(*this);
    if (!result_.asBool) break;
    try {
      execBlock(*s.body, env_);
    } catch (BreakSignal&) {
      break;
    } catch (ContinueSignal&) {
      continue;
    }
  }
}

void Interpreter::visit(ForStmt& stmt) {
  stmt.start->accept(*this);
  long long start = result_.asInt;
  stmt.end->accept(*this);
  long long end = result_.asInt;
  Env forEnv(env_);
  forEnv.set(stmt.iterVar, OxValue::makeInt(start));
  Env* prev = env_;
  env_ = &forEnv;
  try {
    for (long long i = start; i < end; ++i) {
      OxValue* cur = env_->get(stmt.iterVar);
      if (cur) cur->asInt = i;
      try {
        execBlock(*stmt.body, env_);
      } catch (BreakSignal&) {
        env_ = prev;
        return;
      } catch (ContinueSignal&) {
        continue;
      }
    }
  } catch (...) {
    env_ = prev;
    throw;
  }
  env_ = prev;
}

void Interpreter::visit(BreakStmt&) { throw BreakSignal{}; }
void Interpreter::visit(ContinueStmt&) { throw ContinueSignal{}; }

void Interpreter::visit(StructDecl& decl) {
  structLayouts_[decl.name] = &decl;
}

void Interpreter::visit(FnDecl& decl) {
  functions_[decl.name] = &decl;
}

void Interpreter::visit(Program& prog) {
  for (auto& imp : prog.imports) imp->accept(*this);
  for (auto& s : prog.structs) s->accept(*this);
  for (auto& fn : prog.functions) functions_[fn->name] = fn.get();
  for (auto& stmt : prog.topLevelStmts) stmt->accept(*this);
}

void Interpreter::visit(ImportDecl& decl) {
  if (decl.kind == ImportKind::Type) return;
  std::string resolvedPath = resolveImportPath(decl.path);
  if (resolvedPath.empty()) return;
  if (!loadedModulePaths_.count(resolvedPath)) {
    loadedModulePaths_.insert(resolvedPath);
    if (!loadModule(resolvedPath, decl.loc)) return;
  }
  bindImportedNames(resolvedPath, decl);
}

} // namespace oxide
