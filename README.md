# Oxide Language

> A modern programming language with TypeScript-inspired syntax, Rust-grade safety, and native performance via LLVM.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Build](https://img.shields.io/badge/build-passing-brightgreen)]()
[![Tests](https://img.shields.io/badge/tests-65%20passed-brightgreen)]()
[![Version](https://img.shields.io/badge/version-0.2.0--alpha-blue)]()

```typescript
// hello.ox
fn greet(name: string) -> void {
    print("Hello, ${name}!");
}

struct Point { x: int; y: int; }

let p: Point = Point { x: 10, y: 20 };
greet("Oxide");
print("Point: (${p.x}, ${p.y})");
```

---

## Install

### Linux / macOS (one-liner)

```bash
curl -fsSL https://raw.githubusercontent.com/ppenter/oxide-lang/main/install.sh | bash
```

### Windows (PowerShell)

```powershell
iwr -useb https://raw.githubusercontent.com/ppenter/oxide-lang/main/install.ps1 | iex
```

> **Tip:** Windows users can also use WSL2 and run the Linux installer above.

### Custom install directory

```bash
INSTALL_DIR=~/.local/oxide curl -fsSL https://raw.githubusercontent.com/ppenter/oxide-lang/main/install.sh | bash
```

---

## Build from Source

**Requirements:** `g++` 9+ (C++17), `make`, `git`

```bash
git clone https://github.com/ppenter/oxide-lang.git
cd oxide-lang
make            # debug build
make release    # optimised build (recommended for production)
```

Binaries appear in `$HOME/oxide_build/` (Linux) or `build3/` (macOS):
- `oxc` — the Oxide compiler (tree-walking interpreter + LLVM IR emitter)
- `ox`  — the Oxide project manager (`ox run`, `ox dev`, `ox build`, `ox init`)

```bash
export PATH="$PATH:$HOME/oxide_build"
ox --help
```

---

## Quick Start

```bash
# Create and run a program
echo 'fn main() { print("Hello, Oxide!"); }' > hello.ox
ox run hello.ox

# Scaffold a new project
ox init my-app
cd my-app
ox run

# Start a web server
ox run website/server.ox
# → http://localhost:5555
```

---

## Language Features

| Feature | Status |
|---|---|
| Variables (`let` / `const`) | ✅ |
| Functions with typed params | ✅ |
| Structs | ✅ |
| `if` / `else` / `while` | ✅ |
| `for i in 0..10` loops | ✅ |
| `break` / `continue` | ✅ |
| String interpolation `${expr}` | ✅ |
| Built-in string functions | ✅ |
| HTTP server (`router_get`, `router_start`) | ✅ |
| Modules / imports | 🔄 In progress |
| Generic types | 🔄 In progress |
| Enums / match expressions | 📋 Planned |
| LLVM native compilation | 📋 Planned |

---

## HTTP Server

Oxide has a built-in HTTP runtime — no external packages needed:

```typescript
fn home() -> string {
    return "<h1>Hello from Oxide!</h1>";
}

fn api_status() -> string {
    return "{\"status\": \"ok\"}";
}

router_get("/",      "home");
router_get("/api",   "api_status");
router_start("0.0.0.0", 3000);
```

```bash
ox run server.ox
# [OK] Router started on port 3000 with 2 route(s).
```

---

## Deploy to VPS

Use the included deployment script to host the Oxide website on any Linux VPS:

```bash
./deploy-vps.sh YOUR_VPS_IP root
```

This will:
1. Install build dependencies (g++, make, nginx)
2. Clone and build Oxide on the VPS
3. Start the website as a systemd service
4. Configure nginx as a reverse proxy

---

## Project Structure

```
oxide-lang/
├── compiler/          # C++ compiler source
│   ├── lexer/         # Tokeniser
│   ├── parser/        # Recursive-descent parser
│   ├── ast/           # AST node definitions
│   ├── type_checker/  # Type checker & inference
│   ├── codegen/       # LLVM IR code generator
│   ├── interpreter/   # Tree-walking interpreter
│   └── oxx/           # .oxx (Oxide JSX) compiler
├── tests/             # Test runner + test cases
├── tools/             # ox CLI wrapper script
├── website/           # Oxide showcase website (written in Oxide!)
├── examples/          # Example .ox programs
├── lsp/               # Language Server (oxls)
├── vscode-extension/  # VS Code extension
├── install.sh         # Linux/macOS installer
├── install.ps1        # Windows installer
└── deploy-vps.sh      # VPS deployment script
```

---

## Testing

```bash
make test
# Results: 65 passed, 0 failed
```

---

## License

MIT © [Jarupak Srisuchat](https://github.com/ppenter)
