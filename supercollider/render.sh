#!/usr/bin/env bash
# Render a piece without opening the SuperCollider IDE.
#   ./render.sh                 -> rite.wav (seed 1)
#   ./render.sh 7               -> rite.wav with seed 7
#   ./render.sh 1 ash           -> ash.wav
#   SOLO=bass ./render.sh       -> one layer only (stems)
set -euo pipefail
cd "$(dirname "$0")"
PIECE="${2:-rite}"
export SEED="${1:-1}" OUT="${OUT:-$PWD/$PIECE.wav}"
# headless: no display, and sclang's Qt must be allowed to run as root in containers
export QT_QPA_PLATFORM=offscreen QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox" XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/xdg}"
mkdir -p "$XDG_RUNTIME_DIR"
sclang "$PIECE.scd"
