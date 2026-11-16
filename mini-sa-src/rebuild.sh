#!/bin/bash
# Pipeline completo: exe -> listados (capstone) -> traducción recomp-kit -> WebAssembly -> sitio
set -e
G=/home/claude/minisa-game; K=/home/claude/recomp/recomp-kit
cp /home/claude/minisa/minisa.exe $G/original/MINISA.EXE; cp $G/original/MINISA.EXE $G/install/MINISA.EXE
SHA=$(sha256sum $G/original/MINISA.EXE | cut -d' ' -f1); EP=$(i686-w64-mingw32-nm $G/original/MINISA.EXE | awk '/ T _start@0/{print "0x"$1}')
sed -i "s/^sha256 = .*/sha256 = \"$SHA\"/; s/^entry_point = .*/entry_point = $EP/" $G/game.toml
rm -rf $G/analysis/MINISA.EXE && $K/.venv/bin/python /home/claude/minisa/export_listings.py $G/original/MINISA.EXE $G/analysis/MINISA.EXE $K/tools/recomp
source /home/claude/emsdk/emsdk_env.sh >/dev/null 2>&1
cd $K && .venv/bin/python tools/build.py --game-dir $G --target web --regenerate --jobs 8 > /tmp/webmini.log 2>&1 || { tail -30 /tmp/webmini.log; exit 1; }
grep "translated " /tmp/webmini.log
rm -rf /home/claude/minisa-site && .venv/bin/python tools/web_launcher.py --game-dir $G --out /home/claude/minisa-site --web-build minisa=$G/build/web/recomp --stream minisa=$G/install | tail -1
