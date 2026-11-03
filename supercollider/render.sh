#!/usr/bin/env bash
# Render ASH without opening the SuperCollider IDE.
#   ./render.sh            -> ash.wav (seed 1)
#   ./render.sh 7          -> ash.wav with seed 7
#   SOLO=metal ./render.sh -> one layer only (drone metal hits noise ghost sub detritus grains)
set -euo pipefail
cd "$(dirname "$0")"
export SEED="${1:-1}" OUT="${OUT:-$PWD/ash.wav}"
# headless: no display, and sclang's Qt must be allowed to run as root in containers
export QT_QPA_PLATFORM=offscreen QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox"
sclang ash.scd
