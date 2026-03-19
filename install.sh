#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  Oxide Language Installer
#  curl -fsSL https://raw.githubusercontent.com/ppenter/oxide-lang/main/install.sh | bash
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

OXIDE_VERSION="${OXIDE_VERSION:-latest}"
INSTALL_DIR="${INSTALL_DIR:-$HOME/.oxide}"
GITHUB_REPO="ppenter/oxide-lang"
GITHUB_RAW="https://raw.githubusercontent.com/${GITHUB_REPO}/main"
GITHUB_API="https://api.github.com/repos/${GITHUB_REPO}"

# ── Colours ──────────────────────────────────────────────────────────────────
if [ -t 1 ]; then
  RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
  CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
else
  RED=''; GREEN=''; YELLOW=''; CYAN=''; BOLD=''; RESET=''
fi

info()    { printf "${CYAN}[oxide]${RESET} %s\n" "$*"; }
success() { printf "${GREEN}[oxide]${RESET} %s\n" "$*"; }
warn()    { printf "${YELLOW}[oxide]${RESET} %s\n" "$*" >&2; }
error()   { printf "${RED}[oxide] ERROR:${RESET} %s\n" "$*" >&2; exit 1; }

banner() {
  printf "${BOLD}${CYAN}"
  cat <<'EOF'
   ___          _     _
  / _ \__  __ (_) __| | ___
 | | | \ \/ / | |/ _` |/ _ \
 | |_| |>  <  | | (_| |  __/
  \___//_/\_\ |_|\__,_|\___|

  The Oxide Programming Language
EOF
  printf "${RESET}\n"
}

# ── OS / Architecture Detection ───────────────────────────────────────────────
detect_platform() {
  OS="$(uname -s)"
  ARCH="$(uname -m)"

  case "$OS" in
    Linux*)
      PLATFORM="linux"
      ;;
    Darwin*)
      PLATFORM="macos"
      ;;
    MINGW*|MSYS*|CYGWIN*)
      PLATFORM="windows"
      warn "Native Windows detected. Recommended: use WSL2."
      warn "See: https://learn.microsoft.com/en-us/windows/wsl/install"
      ;;
    *)
      error "Unsupported OS: $OS. Oxide supports Linux, macOS, and Windows (via WSL2)."
      ;;
  esac

  case "$ARCH" in
    x86_64|amd64)  ARCH_NAME="x86_64" ;;
    aarch64|arm64) ARCH_NAME="aarch64" ;;
    *)
      warn "Architecture $ARCH not officially supported. Building from source."
      ARCH_NAME="$ARCH"
      ;;
  esac

  info "Detected platform: ${PLATFORM}/${ARCH_NAME}"
}

# ── Dependency Check ──────────────────────────────────────────────────────────
check_deps() {
  local missing=()

  for cmd in git make g++ curl; do
    if ! command -v "$cmd" &>/dev/null; then
      missing+=("$cmd")
    fi
  done

  if [ ${#missing[@]} -gt 0 ]; then
    warn "Missing required dependencies: ${missing[*]}"
    info "Installing dependencies..."

    if command -v apt-get &>/dev/null; then
      sudo apt-get update -qq
      sudo apt-get install -y -qq git make g++ curl build-essential
    elif command -v brew &>/dev/null; then
      brew install git make gcc curl
    elif command -v yum &>/dev/null; then
      sudo yum install -y git make gcc-c++ curl
    elif command -v pacman &>/dev/null; then
      sudo pacman -S --noconfirm git make gcc curl base-devel
    else
      error "Cannot install dependencies automatically. Please install: ${missing[*]}"
    fi
  fi

  success "All dependencies satisfied."
}

# ── Try pre-built binary download ─────────────────────────────────────────────
try_prebuilt() {
  info "Checking for pre-built binaries..."

  if [ "$OXIDE_VERSION" = "latest" ]; then
    local release_url
    release_url="$(curl -fsSL "${GITHUB_API}/releases/latest" 2>/dev/null \
      | grep '"tag_name"' | sed 's/.*"tag_name": *"\([^"]*\)".*/\1/' || true)"
    [ -n "$release_url" ] && OXIDE_VERSION="$release_url"
  fi

  local bin_name="oxide-${PLATFORM}-${ARCH_NAME}.tar.gz"
  local download_url="https://github.com/${GITHUB_REPO}/releases/download/${OXIDE_VERSION}/${bin_name}"

  mkdir -p "$INSTALL_DIR/bin"

  if curl -fsSL --head "$download_url" &>/dev/null; then
    info "Downloading Oxide ${OXIDE_VERSION} for ${PLATFORM}/${ARCH_NAME}..."
    curl -fsSL "$download_url" | tar -xz -C "$INSTALL_DIR/bin"
    success "Pre-built binaries installed."
    return 0
  else
    warn "No pre-built binary found for ${PLATFORM}/${ARCH_NAME}. Building from source..."
    return 1
  fi
}

# ── Build from Source ─────────────────────────────────────────────────────────
build_from_source() {
  info "Cloning Oxide repository..."
  local src_dir="$INSTALL_DIR/src"

  if [ -d "$src_dir/.git" ]; then
    info "Repository already exists, updating..."
    git -C "$src_dir" pull --ff-only
  else
    git clone "https://github.com/${GITHUB_REPO}.git" "$src_dir"
  fi

  info "Building Oxide compiler (this may take ~30 seconds)..."
  cd "$src_dir"

  BUILDDIR="$INSTALL_DIR/build" make release 2>&1 | grep -E '^\[|^g\+\+|^error' || true

  mkdir -p "$INSTALL_DIR/bin"
  cp "$INSTALL_DIR/build/oxc" "$INSTALL_DIR/bin/oxc"
  cp "$INSTALL_DIR/build/ox"  "$INSTALL_DIR/bin/ox" 2>/dev/null || \
    cp "$src_dir/tools/ox"    "$INSTALL_DIR/bin/ox"
  chmod +x "$INSTALL_DIR/bin/oxc" "$INSTALL_DIR/bin/ox"

  success "Oxide built from source successfully."
}

# ── Shell Profile Update ───────────────────────────────────────────────────────
update_path() {
  local bin_dir="$INSTALL_DIR/bin"
  local export_line="export PATH=\"\$PATH:${bin_dir}\""

  local shell_profiles=()
  if [ -n "${BASH_VERSION:-}" ] || [ "${SHELL:-}" = "/bin/bash" ]; then
    shell_profiles+=("$HOME/.bashrc" "$HOME/.bash_profile")
  fi
  if [ -n "${ZSH_VERSION:-}" ] || [ "${SHELL:-}" = "/bin/zsh" ]; then
    shell_profiles+=("$HOME/.zshrc")
  fi
  [ ${#shell_profiles[@]} -eq 0 ] && shell_profiles+=("$HOME/.profile")

  for profile in "${shell_profiles[@]}"; do
    if [ -f "$profile" ]; then
      if ! grep -qF "$bin_dir" "$profile"; then
        printf '\n# Oxide Language\n%s\n' "$export_line" >> "$profile"
        info "Added Oxide to PATH in $profile"
      fi
    fi
  done

  export PATH="$PATH:${bin_dir}"
}

# ── Verify Installation ────────────────────────────────────────────────────────
verify() {
  if command -v oxc &>/dev/null || "$INSTALL_DIR/bin/oxc" --version &>/dev/null; then
    local ver
    ver="$("$INSTALL_DIR/bin/oxc" --version 2>/dev/null || echo "0.2.0-alpha")"
    success "Oxide ${ver} installed successfully!"
  else
    error "Installation verification failed. Please check $INSTALL_DIR/bin/oxc"
  fi
}

# ── Print next steps ───────────────────────────────────────────────────────────
print_next_steps() {
  printf "\n${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}\n"
  printf "${BOLD}  Oxide installed to: ${CYAN}%s${RESET}\n" "$INSTALL_DIR/bin"
  printf "${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}\n\n"
  printf "  Reload your shell:\n"
  printf "    ${CYAN}source ~/.bashrc${RESET}   (bash)\n"
  printf "    ${CYAN}source ~/.zshrc${RESET}    (zsh)\n\n"
  printf "  Then try:\n"
  printf "    ${CYAN}ox --help${RESET}\n"
  printf "    ${CYAN}oxc --version${RESET}\n\n"
  printf "  Documentation: ${CYAN}https://github.com/ppenter/oxide-lang${RESET}\n\n"
}

# ── Main ──────────────────────────────────────────────────────────────────────
main() {
  banner
  detect_platform
  check_deps
  mkdir -p "$INSTALL_DIR"
  if ! try_prebuilt 2>/dev/null; then
    build_from_source
  fi
  update_path
  verify
  print_next_steps
}

main "$@"
