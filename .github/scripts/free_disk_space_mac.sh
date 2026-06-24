#!/usr/bin/env bash
set -euo pipefail

echo "=== Disk usage before cleanup ==="
df -h /
echo ""

free_space() {
  local path="$1"
  if [ -e "$path" ]; then
    local size
    size=$(sudo du -sh "$path" 2>/dev/null | cut -f1 || echo "unknown")
    echo "Removing $path ($size)..."
    sudo rm -rf "$path" || true
  fi
}

free_space /Users/runner/Library/Android
free_space /usr/local/share/dotnet
free_space /usr/local/share/powershell
free_space /usr/local/lib/node_modules
free_space "${AGENT_TOOLSDIRECTORY:-/opt/hostedtoolcache}"
free_space "/Applications/Firefox.app"
free_space "/Applications/Google Chrome.app"

echo ""
echo "=== Disk usage after cleanup ==="
df -h /
