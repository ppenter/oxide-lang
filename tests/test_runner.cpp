// test_runner.cpp — Oxide Phase 1 Test Suite
// Hand-rolled test framework.  Build with the Makefile and run:
//   ./build/run_tests

#include "../compiler/lexer/lexer.h"
#include "../compiler/lexer/token.h"
#include "../compiler/parser/parser.h"
#include "../compiler/type_checker/type_checker.h"
#include "../compiler/codegen/codegen.h"
#include "../compiler/ast/ast.h"

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cassert>

// ─────────────────────────────────────────────────────────────────────────────
// Mini test framework
// ─────────────────────────────────────────────────────────────────────────────

struct TestResult {
  std::string name;
  bool passed;
  std::string message;
};

static std::vector<TestResult> gResults;
static int gPassed = 0, gFailed = 0;

static void runTest(const std::string& name, std::function<void(bool&, std::string&)> fn) {
  bool passed = true;
  std::string msg;
  try {
    fn(passed, msg);
  } catch (const std::exception& e) {
    passed = false;
    msg = std::string("exception: ") + e.what();
  } catch (...) {
    passed = false;
    msg = "unknown exception thrown";
  }
  gResults.push_back({name, passed, msg});
  if (passed) ++gPassed; else ++gFailed;
}

static void check(bool& passed, std::string& msg, bool cond, const std::string& failMsg) {
  if (!cond && passed) {
    passed = false;
    msg = failMsg;
  }
}

static std::vector<oxide::TokenKind> lexKinds(const std::string& src) {
  oxide::Lexer l(src);
  auto toks = l.tokenize();
  std::vector<oxide::TokenKind> kinds;
  for (auto& t : toks) {
    if (t.kind != oxide::TokenKind::END_OF_FILE) kinds.push_back(t.kind);
  }
  return kinds;
}

static std::vector<std::string> lexLexemes(const std::string& src) {
  oxide::Lexer l(src);
  auto toks = l.tokenize();
  std::vector<std::string> lex;
  for (auto& t : toks) {
    if (t.kind != oxide::TokenKind::END_OF_FILE) lex.push_back(t.lexeme);
  }
  return lex;
}

static void testLexerIntegers(bool& ok, std::string& msg) {
  auto kinds = lexKinds("42 0 9999");
  check(ok, msg, kinds.size() == 3, "expected 3 tokens");
  for (auto k : kinds)
    check(ok, msg, k == oxide::TokenKind::INT_LITERAL, "expected INT_LITERAL");
}

static void testLexerFloats(bool& ok, std::string& msg) {
  auto kinds = lexKinds("3.14 0.0 1e10 2.5E-3");
  check(ok, msg, kinds.size() == 4, "expected 4 float tokens");
  for (auto k : kinds)
    check(ok, msg, k == oxide::TokenKind::FLOAT_LITERAL, "expected FLOAT_LITERAL");
}

static void testLexerStrings(bool& ok, std::string& msg) {
  auto toks = lexLexemes("\"hello\" \"world\"");
  check(ok, msg, toks.size() == 2, "expected 2 string tokens");
  check(ok, msg, toks[0] == "hello", "first string should be 'hello'");
  check(ok, msg, toks[1] == "world", "second string should be 'world'");
}

static void testLexerKeywords(bool& ok, std::string& msg) {
  using K = oxide::TokenKind;
  auto kinds = lexKinds("let const fn return if else while for import export true false print int float bool string void");
  std::vector<K> expected = {
    K::KW_LET, K::KW_CONST, K::KW_FN, K::KW_RETURN,
    K::KW_IF, K::KW_ELSE, K::KW_WHILE, K::KW_FOR,
    K::KW_IMPORT, K::KW_EXPORT, K::KW_TRUE, K::KW_FALSE,
    K::KW_PRINT, K::KW_INT, K::KW_FLOAT, K::KW_BOOL,
    K::KW_STRING, K::KW_VOID
  };
  check(ok, msg, kinds.size() == expected.size(), "keyword token count mismatch");
}

static std::unique_ptr<oxide::Program> parseStr(const std::string& src) {
  oxide::Lexer lexer(src);
  auto toks = lexer.tokenize();
  oxide::Parser parser(toks);
  return parser.parse();
}

static void testParseIntLiteral(bool& ok, std::string& msg) {
  auto prog = parseStr("42;");
  check(ok, msg, prog != nullptr, "program should not be null");
  check(ok, msg, prog->topLevelStmts.size() == 1, "expected 1 top-level stmt");
}

static void testParseLetDecl(bool& ok, std::string& msg) {
  auto prog = parseStr("let x = 10;");
  check(ok, msg, prog != nullptr, "program not null");
  check(ok, msg, prog->topLevelStmts.size() == 1, "1 statement");
}

static bool typeCheck(const std::string& src) {
  oxide::Lexer lexer(src);
  auto toks = lexer.tokenize();
  oxide::Parser parser(toks);
  auto prog = parser.parse();
  if (!prog) return false;
  oxide::TypeChecker tc;
  return tc.check(*prog);
}

static void testTypeCheckIntOps(bool& ok, std::string& msg) {
  bool result = typeCheck("let x: int = 1 + 2;");
  check(ok, msg, result, "int arithmetic should type-check");
}

static std::string codegenSource(const std::string& src) {
  oxide::Lexer lexer(src);
  auto toks = lexer.tokenize();
  oxide::Parser parser(toks);
  auto prog = parser.parse();
  if (!prog) return "";
  oxide::CodeGen cg;
  return cg.generate(*prog);
}

static void testCodegenContainsDefine(bool& ok, std::string& msg) {
  std::string ir = codegenSource("fn main() -> void { }");
  check(ok, msg, ir.find("define void @main") != std::string::npos, "IR should contain function definition");
}

int main() {
  std::cout << "\u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n";
  std::cout << "  Oxide Phase 1 + Phase 2 Test Suite\n";
  std::cout << "\u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d\n\n";

  std::cout << "Lexer tests\n";
  runTest("lexer: integer literals", testLexerIntegers);
  runTest("lexer: float literals", testLexerFloats);
  runTest("lexer: string literals", testLexerStrings);
  runTest("lexer: keywords", testLexerKeywords);

  std::cout << "\nParser tests\n";
  runTest("parser: integer literal", testParseIntLiteral);
  runTest("parser: let declaration", testParseLetDecl);

  std::cout << "\nType checker tests\n";
  runTest("types: int arithmetic", testTypeCheckIntOps);

  std::cout << "\nCodegen tests\n";
  runTest("codegen: define emitted", testCodegenContainsDefine);

  std::cout << "\n\u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n";
  for (auto& r : gResults) {
    std::cout << (r.passed ? "  [PASS] " : "  [FAIL] ") << r.name;
    if (!r.passed) std::cout << "\n         \u2192 " << r.message;
    std::cout << "\n";
  }
  std::cout << "\n  Results: " << gPassed << " passed, " << gFailed << " failed\n";
  std::cout << "\u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d\n";

  return (gFailed == 0) ? 0 : 1;
}
