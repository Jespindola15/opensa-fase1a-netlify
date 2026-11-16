#!/bin/bash
set -e
G=/home/claude/minisa2-game; K=/home/claude/recomp/recomp-kit
cp /home/claude/minisa/minisa2.exe $G/original/MINISA2.EXE; cp $G/original/MINISA2.EXE $G/install/MINISA2.EXE
SHA=$(sha256sum $G/original/MINISA2.EXE | cut -d' ' -f1); EP=$(i686-w64-mingw32-nm $G/original/MINISA2.EXE | awk '/ T _start@0/{print "0x"$1}')
sed -i "s/^sha256 = .*/sha256 = \"$SHA\"/; s/^entry_point = .*/entry_point = $EP/" $G/game.toml
rm -rf $G/analysis/MINISA2.EXE && $K/.venv/bin/python /home/claude/minisa/export_listings.py $G/original/MINISA2.EXE $G/analysis/MINISA2.EXE $K/tools/recomp
source /home/claude/emsdk/emsdk_env.sh >/dev/null 2>&1
cd $K && .venv/bin/python tools/build.py --game-dir $G --target ${TARGET:-web} --regenerate --jobs 8 > /tmp/build2.log 2>&1 || { grep -v "^\[" /tmp/build2.log | tail -40; exit 1; }
grep "translated " /tmp/build2.log
if [ "${TARGET:-web}" = web ]; then rm -rf /home/claude/minisa2-site && .venv/bin/python tools/web_launcher.py --game-dir $G --out /home/claude/minisa2-site --web-build minisa2=$G/build/web/recomp --stream minisa2=$G/install | tail -1; fi
