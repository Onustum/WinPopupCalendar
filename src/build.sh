#!/bin/sh
# Кросс-сборка DockCalendar под Windows x64 из Linux.
# Требуется пакет g++-mingw-w64-x86-64 (Debian/Ubuntu) или аналог.
set -e
cd "$(dirname "$0")"
CXX=${CXX:-x86_64-w64-mingw32-g++}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
"$WINDRES" resource.rc -O coff -o resource.res
"$CXX" -O2 -std=c++17 -municode -mwindows main.cpp resource.res \
    -o ../DockCalendar.exe -static -lgdiplus -ldwmapi -lshell32 -lgdi32 -luser32 -ladvapi32 -s
echo "[OK] Собрано: ../DockCalendar.exe"
