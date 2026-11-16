#!/usr/bin/env python3
"""Traductor mínimo x86-32 -> C (solo el subconjunto x87 de esta prueba). Lee el desensamblado Intel de objdump
y emite C que opera sobre un modelo explícito de CPU: memoria de invitado de 32 bits, registros enteros y la pila x87
de 8 registros con control de precisión (PC). No es un recompilador general: falla si encuentra una instrucción
que no conoce, en lugar de inventarla."""
import re,sys
lines=[l for l in open(sys.argv[1]).read().splitlines()[1:] if re.match(r'\s*[0-9a-f]+:',l)]
out=[]; E=out.append
mem=lambda s: re.sub(r'DWORD PTR \[(\w+)(?:\+0x([0-9a-f]+))?\]', lambda m: f'(c->{m.group(1)}+0x{m.group(2) or "0"})', s)
def sti(s): m=re.fullmatch(r'st(?:\((\d)\))?',s.strip()); return int(m.group(1) or 0)
E('void f_%s(Cpu* c){' % lines[0].split(':')[0].strip())
for l in lines:
  a,ins=l.split(':',1); a=a.strip(); ins=ins.strip()
  op,_,args=ins.partition(' '); args=[x.strip() for x in re.split(r',(?![^\[]*\])',args)] if args.strip() else []
  E(f' L_{a}: /* {ins} */')
  if op=='mov' and args[0]=='eax': E(f'  c->eax=LD32({mem(args[1])});')
  elif op=='fld' and 'PTR' in args[0]: E(f'  PUSH((double)LDF({mem(args[0])}));')
  elif op=='fld': E(f'  {{double t=ST({sti(args[0])}); PUSH(t);}}')
  elif op=='fld1': E('  PUSH(1.0);')
  elif op=='fldz': E('  PUSH(0.0);')
  elif op in('fmul','fadd','fdiv','fdivr','fsub','fsubr'):
    d,s=sti(args[0]),sti(args[1]); x=f'ST({d})'; y=f'ST({s})'
    expr={'fmul':f'{x}*{y}','fadd':f'{x}+{y}','fdiv':f'{x}/{y}','fsub':f'{x}-{y}','fdivr':f'{y}/{x}','fsubr':f'{y}-{x}'}[op]
    E(f'  ST({d})=R({expr});')
  elif op in('faddp','fmulp','fdivp','fdivrp','fsubp','fsubrp'):
    d=sti(args[0]); b=op[:-1]; x=f'ST({d})'; y='ST(0)'
    expr={'fadd':f'{x}+{y}','fmul':f'{x}*{y}','fdiv':f'{x}/{y}','fdivr':f'{y}/{x}','fsub':f'{x}-{y}','fsubr':f'{y}-{x}'}[b]
    E(f'  ST({d})=R({expr}); POP();')
  elif op=='fsqrt': E('  ST(0)=R(sqrt(ST(0)));')
  elif op=='fxch': i=sti(args[0]); E(f'  {{double t=ST(0); ST(0)=ST({i}); ST({i})=t;}}')
  elif op=='fstp' and 'PTR' in args[0]: E(f'  STF({mem(args[0])},(float)ST(0)); POP();')
  elif op=='fstp': i=sti(args[0]); E(f'  ST({i})=ST(0); POP();')
  elif op=='fucomip': i=sti(args[1]); E(f'  {{double a=ST(0),b=ST({i}); c->pf=(a!=a)||(b!=b); c->zf=c->pf||a==b; c->cf=c->pf||a<b;}} POP();')
  elif op=='jae': E(f'  if(!c->cf) goto L_{args[0].split()[0]};')
  elif op=='ret': E('  return;')
  else: sys.exit(f'instrucción no soportada en 0x{a}: {ins}')
E('}')
print('\n'.join(out))
