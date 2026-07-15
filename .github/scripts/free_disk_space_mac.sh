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

get_active_xcode() {
  local path
  path=$(python3 -c "import os; print(os.path.realpath('/Applications/Xcode.app'))" 2>/dev/null || echo "")
  if [ -n "$path" ] && [ -d "$path" ]; then
    echo "$path"
    return
  fi

  local dev_dir
  dev_dir=$(xcode-select -p 2>/dev/null || echo "")
  if [ -n "$dev_dir" ]; then
    path=$(echo "$dev_dir" | sed 's/\/Contents\/Developer//')
    if [ -d "$path" ]; then
      echo "$path"
      return
    fi
  fi

  if [ -d "/Applications/Xcode.app" ]; then
    echo "/Applications/Xcode.app"
  fi
}

active_xcode=$(get_active_xcode)
echo "Active Xcode: $active_xcode"

if [ -n "$active_xcode" ]; then
  for xcode_dir in /Applications/Xcode_*.app; do
    if [ -d "$xcode_dir" ]; then
      real_dir=$(python3 -c "import os; print(os.path.realpath('$xcode_dir'))" 2>/dev/null || echo "$xcode_dir")
      if [ "$real_dir" != "$active_xcode" ]; then
        free_space "$xcode_dir"
      else
        echo "Keeping active Xcode: $xcode_dir (matches $active_xcode)"
      fi
    fi
  done
fi

free_space /Users/runner/Library/Android
free_space /usr/local/share/dotnet
free_space /usr/local/share/powershell
free_space /usr/local/share/mono
free_space /usr/local/share/julia
free_space /usr/local/share/boost
free_space /usr/local/lib/node_modules
free_space "/Applications/Firefox.app"
free_space "/Applications/Google Chrome.app"

toolcache="${AGENT_TOOLSDIRECTORY:-/opt/hostedtoolcache}"
if [ -d "$toolcache" ]; then
  echo "Cleaning $toolcache (preserving Python)..."
  for dir in "$toolcache"/*; do
    if [ -d "$dir" ] && [[ ! "$dir" =~ [Pp]ython ]]; then
      free_space "$dir"
    fi
  done
fi

free_space /Users/runner/.rustup
free_space /Users/runner/.cargo
free_space /Users/runner/.julia
free_space /Users/runner/.npm
free_space /Users/runner/.cocoapods
free_space /Users/runner/.fastlane
free_space /Users/runner/Library/Developer/Xcode/DerivedData
free_space /Users/runner/Library/Caches/com.apple.dt.Xcode

brew_cache=$(brew --cache 2>/dev/null || echo "")
if [ -n "$brew_cache" ] && [ -d "$brew_cache" ]; then
  free_space "$brew_cache"
fi

echo ""
echo "=== Disk usage after cleanup ==="
df -h /
