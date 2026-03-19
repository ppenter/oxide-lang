// main.cpp — Oxide Compiler Driver (oxc)
// Usage:
//   oxc <source.ox>             — lex, parse, type-check, then RUN via interpreter
//   oxc <source.ox> --emit      — emit LLVM IR to stdout
//   oxc <source.ox> --emit -o f — write LLVM IR to file

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "type_checker/type_checker.h"
#include "interpreter/interpreter.h"
#include "codegen/codegen.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

static std::string readFile(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    std::cerr << "oxc: cannot open file '" << path << "'\n";
    return {};
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static std::string dirOf(const std::string& path) {
  size_t slash = path.find_last_of("/\\");
  return (slash != std::string::npos) ? path.substr(0, slash) : ".";
}

static std::unordered_map<std::string, std::string>
loadPathAliases(const std::string& startDir) {
  std::unordered_map<std::string, std::string> aliases;
  std::string dir = startDir;
  for (int depth = 0; depth < 10; ++depth) {
    std::string configPath = dir + "/oxide.config.json";
    std::ifstream f(configPath);
    if (f.is_open()) {
      std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
      size_t pathsPos = content.find("\"paths\"");
      if (pathsPos != std::string::npos) {
        size_t braceOpen = content.find('{', pathsPos);
        size_t braceClose = content.find('}', braceOpen);
        if (braceOpen != std::string::npos && braceClose != std::string::npos) {
          std::string pathsBlock = content.substr(braceOpen + 1, braceClose - braceOpen - 1);
          size_t pos = 0;
          while (pos < pathsBlock.size()) {
            size_t q1 = pathsBlock.find('"', pos);
            if (q1 == std::string::npos) break;
            size_t q2 = pathsBlock.find('"', q1 + 1);
            if (q2 == std::string::npos) break;
            std::string key = pathsBlock.substr(q1 + 1, q2 - q1 - 1);
            size_t colon = pathsBlock.find(':', q2);
            if (colon == std::string::npos) break;
            size_t q3 = pathsBlock.find('"', colon);
            if (q3 == std::string::npos) break;
            size_t q4 = pathsBlock.find('"', q3 + 1);
            if (q4 == std::string::npos) break;
            std::string val = pathsBlock.substr(q3 + 1, q4 - q3 - 1);
            if (val.rfind("./", 0) == 0)
              val = dir + "/" + val;
            aliases[key] = val;
            pos = q4 + 1;
          }
        }
      }
      break;
    }
    size_t slash = dir.find_last_of("/\\");
    if (slash == std::string::npos || dir == "/") break;
    dir = dir.substr(0, slash);
  }
  return aliases;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Oxide Compiler (oxc) — Phase 2\n";
    std::cerr << "Usage:\n";
    std::cerr << "  oxc <source.ox>              run the program\n";
    std::cerr << "  oxc <source.ox> --emit        print LLVM IR\n";
    std::cerr << "  oxc <source.ox> --emit -o f   write LLVM IR to file\n";
    return 1;
  }
  std::string inputPath;
  std::string outputPath;
  bool emitIR = false;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--emit") {
      emitIR = true;
    } else if (arg == "-o" && i + 1 < argc) {
      outputPath = argv[++i];
    } else if (arg[0] != '-') {
      inputPath = arg;
    }
  }
  if (inputPath.empty()) {
    std::cerr << "oxc: no input file specified\n";
    return 1;
  }
  std::string source = readFile(inputPath);
  if (source.empty() && !inputPath.empty()) return 1;
  std::string sourceDir = dirOf(inputPath);
  auto pathAliases = loadPathAliases(sourceDir);
  oxide::Lexer lexer(source, inputPath);
  auto tokens = lexer.tokenize();
  bool hasErrors = false;
  for (auto& e : lexer.errors()) { std::cerr << e << "\n"; hasErrors = true; }
  oxide::Parser parser(tokens, inputPath);
  auto program = parser.parse();
  for (auto& e : parser.errors()) { std::cerr << e << "\n"; hasErrors = true; }
  if (!program || hasErrors) {
    std::cerr << "oxc: aborted due to parse errors\n";
    return 1;
  }
  oxide::TypeChecker checker(inputPath);
  bool typeOk = checker.check(*program);
  for (auto& e : checker.errors()) { std::cerr << e << "\n"; }
  if (!typeOk) {
    std::cerr << "oxc: aborted due to type errors\n";
    return 1;
  }
  if (emitIR) {
    oxide::CodeGen codegen(inputPath);
    std::string ir = codegen.generate(*program);
    for (auto& e : codegen.errors()) { std::cerr << e << "\n"; hasErrors = true; }
    if (hasErrors) { std::cerr << "oxc: aborted due to codegen errors\n"; return 1; }
    if (outputPath.empty()) {
      std::cout << ir;
    } else {
      std::ofstream out(outputPath);
      if (!out.is_open()) {
        std::cerr << "oxc: cannot open output file '" << outputPath << "'\n";
        return 1;
      }
      out << ir;
      std::cout << "oxc: wrote LLVM IR to " << outputPath << "\n";
    }
  } else {
    oxide::Interpreter interp(inputPath);
    interp.setPathAliases(std::move(pathAliases));
    bool ok = interp.run(*program);
    for (auto& e : interp.errors()) std::cerr << e << "\n";
    return ok ? 0 : 1;
  }
  return 0;
}
