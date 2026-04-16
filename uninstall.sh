#!/usr/bin/env bash
# SmartComplete — Top-level uninstaller wrapper.
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "$SCRIPT_DIR/scripts/uninstall.sh" "$@"
