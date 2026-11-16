"""Sustituto de ExportProgram.java (Ghidra) para un ejecutable propio con símbolos: lista las funciones
desde la tabla de símbolos COFF y las decodifica con el conversor capstone->listado del propio translate.py."""
import subprocess, sys
from pathlib import Path
sys.path.insert(0, sys.argv[3])
import translate as T
exe, out = Path(sys.argv[1]), Path(sys.argv[2])
syms = subprocess.run(["i686-w64-mingw32-objdump", "-t", str(exe)], capture_output=True, text=True, check=True).stdout
img = T.Image(exe)
starts = {}
import re
for line in syms.splitlines():
    # [  n](sec  1)(fl 0x00)(ty   20)(scl   2) (nx 0) 0x00000a30 _physics_step   -> ty 20 = función COFF
    m = re.search(r"\(sec\s+1\)\(fl 0x00\)\(ty\s+20\)\(scl\s+\d+\) \(nx \d+\) 0x([0-9a-f]+) (\S+)", line)
    if not m: continue
    va = int(m.group(1), 16) + img.base + 0x1000
    if img.is_exec(va): starts.setdefault(va, m.group(2))
listed = set(starts)
(out / "functions").mkdir(parents=True, exist_ok=True)
rows = ["address\tname\tbytes\tstatus\tmessage"]
for va, name in sorted(starts.items()):
    insns = img.recover(va, listed - {va})
    if not insns:
        print("sin instrucciones:", hex(va), name, img.recover_errors[-1:] ); continue
    (out / "functions" / ("%08x.asm" % va)).write_text("\n".join(i.raw for i in insns) + "\n")
    (out / "functions" / ("%08x.c" % va)).write_text("/* %s %08x %s; listado generado con capstone */\n" % (exe.name, va, name))
    end = max(i.addr for i in insns)
    rows.append("%08x\t%s\t%d\tpseudocode\t" % (va, name.lstrip("_").split("@")[0], end - va + 1))
(out / "functions.tsv").write_text("\n".join(rows) + "\n")
(out / "summary.txt").write_text("discovered=%d\npseudocode=%d\nfailed=0\n" % (len(rows) - 1, len(rows) - 1))
print("funciones exportadas:", len(rows) - 1)
