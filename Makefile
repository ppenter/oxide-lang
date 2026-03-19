# Oxide Compiler — Makefile
# Phase 2: Language Expansion + Module System
#
# Requires: g++ (C++17)
# Optional: clang (to compile emitted LLVM IR)
#
# Targets:
#   make              — build oxc compiler, ox CLI, and test runner (debug)
#   make release      — build with -O2 optimisations (production)
#   make test         — run all tests
#   make clean        — remove build artifacts

CXX      := g++

# ── Build-mode flags ───────────────────────────────────────────────────────────────────────────────
OPTFLAGS ?= -g
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic $(OPTFLAGS) -I$(CURDIR)/compiler

SRCDIR   := compiler

# ── Platform-aware build directory ───────────────────────────────────────────────────────────────────────────────
UNAME_S      := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  BUILDDIR     := $(HOME)/oxide_build
  LSP_BUILDDIR := $(HOME)/oxide_build_lsp
else
  BUILDDIR     := build3
  LSP_BUILDDIR := build_lsp
endif

# ── Compiler sources ───────────────────────────────────────────────────────────────────────────────
COMPILER_SRCS := \
  $(SRCDIR)/lexer/token.cpp \
  $(SRCDIR)/lexer/lexer.cpp \
  $(SRCDIR)/parser/parser.cpp \
  $(SRCDIR)/type_checker/type_checker.cpp \
  $(SRCDIR)/codegen/codegen.cpp \
  $(SRCDIR)/oxx/ox_to_js.cpp \
  $(SRCDIR)/oxx/sfc_parser.cpp \
  $(SRCDIR)/oxx/oxx_preprocessor.cpp \
  $(SRCDIR)/interpreter/interpreter.cpp \
  $(SRCDIR)/main.cpp

COMPILER_OBJS := $(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/compiler/%.o, $(COMPILER_SRCS))

# ── Test runner sources ───────────────────────────────────────────────────────────────────────────────
TEST_SRCS := \
  $(SRCDIR)/lexer/token.cpp \
  $(SRCDIR)/lexer/lexer.cpp \
  $(SRCDIR)/parser/parser.cpp \
  $(SRCDIR)/type_checker/type_checker.cpp \
  $(SRCDIR)/codegen/codegen.cpp \
  tests/test_runner.cpp

TEST_COMPILER_SRCS := \
  $(SRCDIR)/lexer/token.cpp \
  $(SRCDIR)/lexer/lexer.cpp \
  $(SRCDIR)/parser/parser.cpp \
  $(SRCDIR)/type_checker/type_checker.cpp \
  $(SRCDIR)/codegen/codegen.cpp \
  $(SRCDIR)/oxx/ox_to_js.cpp \
  $(SRCDIR)/oxx/sfc_parser.cpp \
  $(SRCDIR)/oxx/oxx_preprocessor.cpp \
  $(SRCDIR)/interpreter/interpreter.cpp

TEST_COMPILER_OBJS := $(patsubst $(SRCDIR)/%.cpp, $(BUILDDIR)/compiler/%.o, $(TEST_COMPILER_SRCS))
TEST_RUNNER_OBJ    := $(BUILDDIR)/tests/test_runner.o
TEST_OBJS          := $(TEST_COMPILER_OBJS) $(TEST_RUNNER_OBJ)

# ── Targets ───────────────────────────────────────────────────────────────────────────────

.PHONY: all release test clean oxls

all: $(BUILDDIR)/oxc $(BUILDDIR)/ox $(BUILDDIR)/run_tests

# ── Release build ───────────────────────────────────────────────────────────────────────────────
release:
	$(MAKE) all OPTFLAGS="-O2 -DNDEBUG"

# Link the oxc compiler
$(BUILDDIR)/oxc: $(COMPILER_OBJS)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@
	@echo "[OK] Built oxc compiler → $@"

# Install the ox CLI tool (shell script wrapper around oxc)
$(BUILDDIR)/ox: tools/ox $(BUILDDIR)/oxc
	@mkdir -p $(BUILDDIR)
	cp tools/ox $(BUILDDIR)/ox
	chmod +x $(BUILDDIR)/ox
	@echo "[OK] Installed ox CLI → $@"

# Link the test runner
$(BUILDDIR)/run_tests: $(TEST_OBJS)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $^ -o $@
	@echo "[OK] Built test runner → $@"

# Generic object compilation for compiler/
$(BUILDDIR)/compiler/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Generic object compilation for tests/
$(BUILDDIR)/tests/test_runner.o: tests/test_runner.cpp
	@mkdir -p $(BUILDDIR)/tests
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run tests
test: $(BUILDDIR)/run_tests
	@echo "\n── Running Oxide Test Suite ──"
	$(BUILDDIR)/run_tests

clean:
	rm -rf $(BUILDDIR) $(LSP_BUILDDIR)
	@echo "Cleaned build artifacts."
