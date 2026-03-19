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

# ── Build-mode flags ──────────────────────────────────────────
# Debug mode (default): full debug info, no optimisation.
# Release mode (`make release`): -O2 + strip debug info for fast binaries.
OPTFLAGS ?= -g
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic $(OPTFLAGS) -I$(CURDIR)/compiler

SRCDIR   := compiler

# ── Platform-aware build directory ────────────────────────────
# On Linux (Cowork/CI): each session has its own writable home at
# $(HOME)/oxide_build — immune to cross-session and cross-OS permission
# issues that affect both the workspace mount and /tmp.
# On macOS we keep the traditional build3/ directory inside the project.
UNAME_S      := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  BUILDDIR     := $(HOME)/oxide_build
  LSP_BUILDDIR := $(HOME)/oxide_build_lsp
else
  BUILDDIR     := build3
  LSP_BUILDDIR := build_lsp
endif

# ── Compiler sources ──────────────────────────────────────────
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

# ── Test runner sources ───────────────────────────────────────
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

# ── OxLS Language Server ──────────────────────────────────────
# Compiled in a single g++ invocation (all sources → one binary).
# This avoids cross-platform .o file conflicts: make oxls always
# recompiles from source, so it works correctly on both macOS and Linux.
# (LSP_BUILDDIR is set above in the platform-aware block.)

LSP_ALL_SRCS := \
  $(SRCDIR)/lexer/token.cpp \
  $(SRCDIR)/lexer/lexer.cpp \
  $(SRCDIR)/parser/parser.cpp \
  $(SRCDIR)/type_checker/type_checker.cpp \
  lsp/oxls.cpp

# ── Targets ───────────────────────────────────────────────────

.PHONY: all release test clean oxls

all: $(BUILDDIR)/oxc $(BUILDDIR)/ox $(BUILDDIR)/run_tests

# ── Release build ─────────────────────────────────────────────
# Compiles with -O2 -DNDEBUG for maximum runtime performance.
# Binaries are significantly faster than the default debug build.
# BUILDDIR is forwarded so callers can override it (e.g. the install script).
release:
	$(MAKE) all OPTFLAGS="-O2 -DNDEBUG" BUILDDIR="$(BUILDDIR)"

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
# -MMD -MP generates a .d dependency file alongside each .o so that Make
# automatically knows when a header has changed and recompiles affected units.
$(BUILDDIR)/compiler/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Generic object compilation for tests/
$(BUILDDIR)/tests/test_runner.o: tests/test_runner.cpp
	@mkdir -p $(BUILDDIR)/tests
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Include auto-generated header dependency files (if they exist).
# The "-" prefix suppresses errors when no .d files exist yet (first build).
-include $(COMPILER_OBJS:.o=.d)
-include $(TEST_RUNNER_OBJ:.o=.d)

# Run tests
test: $(BUILDDIR)/run_tests
	@echo "\n── Running Oxide Test Suite ──"
	$(BUILDDIR)/run_tests

# Build the OxLS language server (single-pass: all sources compiled together)
oxls: $(LSP_ALL_SRCS)
	@mkdir -p $(LSP_BUILDDIR)
	$(CXX) $(CXXFLAGS) $^ -o $(LSP_BUILDDIR)/oxls
	@echo "[OK] Built OxLS language server → $(LSP_BUILDDIR)/oxls"

clean:
	rm -rf $(BUILDDIR) $(LSP_BUILDDIR)
	@echo "Cleaned build artifacts."
