#!/usr/bin/env bash
# Cross-compile the interactive walk-through (gl_interactive.cpp) to a Windows
# .exe from Linux. Requires mingw-w64 and the SDL2 *mingw* devel libs.
#
#   sudo apt-get install -y mingw-w64
#   curl -L -o sdl2.tar.gz \
#     https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-devel-2.30.9-mingw.tar.gz
#   tar xzf sdl2.tar.gz
#   SDL_MINGW=$PWD/SDL2-2.30.9/x86_64-w64-mingw32 ./build_windows.sh
#
# The resulting .exe needs SDL2.dll (from $SDL_MINGW/bin) next to it.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SDL_MINGW="${SDL_MINGW:-/tmp/SDL2-2.30.9/x86_64-w64-mingw32}"
OUT="${1:-MURK_walk.exe}"

x86_64-w64-mingw32-g++ -O2 -std=c++17 \
  -I"$SDL_MINGW/include/SDL2" -I"$HERE/src" \
  "$HERE/src/gl_interactive.cpp" "$HERE/src/mesh.cpp" \
  -L"$SDL_MINGW/lib" -static-libgcc -static-libstdc++ \
  -lmingw32 -lSDL2main -lSDL2 -lopengl32 -mwindows \
  -o "$OUT"
echo "built $OUT  (ship SDL2.dll alongside it)"
