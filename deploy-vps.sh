#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  Oxide VPS Deployment Script — Hostinger / any Linux VPS
#
#  USAGE:
#    ./deploy-vps.sh <VPS_IP> [SSH_USER]
#
#  EXAMPLE:
#    ./deploy-vps.sh 123.456.789.000 root
#    ./deploy-vps.sh 123.456.789.000 ubuntu
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

VPS_IP="${1:-}"
SSH_USER="${2:-root}"
GITHUB_REPO="ppenter/oxide-lang"
OXIDE_PORT=6003

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
info()    { printf "${CYAN}[deploy]${RESET} %s\n" "$*"; }
success() { printf "${GREEN}[deploy]${RESET} %s\n" "$*"; }
error()   { printf "${RED}[deploy] ERROR:${RESET} %s\n" "$*" >&2; exit 1; }

if [ -z "$VPS_IP" ]; then
  error "Usage: $0 <VPS_IP> [SSH_USER]"
fi

info "Deploying Oxide to ${SSH_USER}@${VPS_IP}..."

REMOTE_SETUP=$(cat <<'REMOTE_EOF'
set -euo pipefail
GITHUB_REPO="ppenter/oxide-lang"
INSTALL_DIR="$HOME/.oxide"
OXIDE_PORT=6003

echo "[vps] Updating system packages..."
if command -v apt-get &>/dev/null; then
  export DEBIAN_FRONTEND=noninteractive
  apt-get update -qq
  apt-get install -y -qq git make g++ build-essential nginx curl
elif command -v yum &>/dev/null; then
  yum install -y git make gcc-c++ nginx curl
fi

echo "[vps] Cloning / updating Oxide repository..."
if [ -d "$INSTALL_DIR/src/.git" ]; then
  git -C "$INSTALL_DIR/src" pull --ff-only
else
  mkdir -p "$INSTALL_DIR"
  git clone "https://github.com/${GITHUB_REPO}.git" "$INSTALL_DIR/src"
fi

echo "[vps] Building Oxide compiler..."
cd "$INSTALL_DIR/src"
mkdir -p "$INSTALL_DIR/build"
BUILDDIR="$INSTALL_DIR/build" make release
mkdir -p "$INSTALL_DIR/bin"
cp "$INSTALL_DIR/build/oxc" "$INSTALL_DIR/bin/oxc" || cp "$HOME/oxide_build/oxc" "$INSTALL_DIR/bin/oxc"
cp "$INSTALL_DIR/src/tools/ox" "$INSTALL_DIR/bin/ox" 2>/dev/null || true
chmod +x "$INSTALL_DIR/bin/oxc" "$INSTALL_DIR/bin/ox" 2>/dev/null || true
ln -sf "$INSTALL_DIR/bin/oxc" /usr/local/bin/oxc 2>/dev/null || sudo ln -sf "$HOME/oxide_build/oxc" /usr/local/bin/oxc

echo "[vps] Creating systemd service..."
cat > /etc/systemd/system/oxide-website.service <<SERVICE
[Unit]
Description=Oxide Language Website Server
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=${INSTALL_DIR}/src
Environment="OX_PORT=${OXIDE_PORT}"
ExecStart=/usr/local/bin/oxc --run ${INSTALL_DIR}/src/website/server.ox
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
SERVICE

systemctl daemon-reload
systemctl enable oxide-website
systemctl restart oxide-website
sleep 2
systemctl status oxide-website --no-pager || true

echo "[vps] Configuring nginx reverse proxy..."
cat > /etc/nginx/sites-available/oxide-lang <<NGINX
server {
    listen 80;
    server_name _;
    location / {
        proxy_pass http://127.0.0.1:${OXIDE_PORT};
        proxy_http_version 1.1;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_read_timeout 30;
    }
}
NGINX

ln -sf /etc/nginx/sites-available/oxide-lang /etc/nginx/sites-enabled/oxide-lang
rm -f /etc/nginx/sites-enabled/default
nginx -t && systemctl reload nginx

echo ""
echo "Oxide website deployed! Access at: http://$(curl -s ifconfig.me 2>/dev/null || echo '<your-vps-ip>')"
REMOTE_EOF
)

ssh -o StrictHostKeyChecking=no -o ConnectTimeout=15 "${SSH_USER}@${VPS_IP}" "bash -s" <<< "$REMOTE_SETUP"

success "Deployment complete! Website: http://${VPS_IP}"
info "Logs: ssh ${SSH_USER}@${VPS_IP} 'journalctl -u oxide-website -f'"
info "Update: ssh ${SSH_USER}@${VPS_IP} 'cd ~/.oxide/src && git pull && make release && systemctl restart oxide-website'"
