#!/usr/bin/env bash
# Baut OpenLabCheckIn.exe mit MinGW-w64 (unter Linux oder MSYS2).
set -euo pipefail
cd "$(dirname "$0")"
CXX=${CXX:-x86_64-w64-mingw32-g++-posix}
command -v "$CXX" >/dev/null || CXX=x86_64-w64-mingw32-g++
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
mkdir -p build
$WINDRES -I src src/app.rc -O coff -o build/app.res
$CXX -std=c++17 -O2 -Wall -Wextra -DUNICODE -D_UNICODE -DNOMINMAX -municode -mwindows -static -s \
     src/main.cpp src/model.cpp build/app.res -lcomctl32 -lcomdlg32 -lbcrypt -o build/OpenLabCheckIn.exe
echo "Fertig: build/OpenLabCheckIn.exe"
