#!/usr/bin/env bash
# SmartComplete — Top-level one-click installer.
# Delegates to scripts/install.sh. Exists here so users can run `./install.sh`
# from the project root without needing to know the layout.

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "$SCRIPT_DIR/scripts/install.sh" "$@"
