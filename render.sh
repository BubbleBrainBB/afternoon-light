#!/bin/sh
# Re-render "afternoon light" from scratch.
#   ./render.sh            -> 3840x2560, 400 samples/pixel (about 1 h on a 16-core laptop)
#   ./render.sh 64 1920    -> quick look: 64 spp at 1920x1280 (a few minutes)
set -e
cd "$(dirname "$0")"
SPP=${1:-400}
W=${2:-3840}
H=$((W * 2 / 3))
mkdir -p work
c++ -O3 -march=native -ffast-math -fno-finite-math-only -std=c++17 -o work/render src/main.cpp
./work/render "$W" "$H" "$SPP" work/final
python3 src/develop.py work/final afternoon_light.png --preview preview.jpg --full-w "$W"
