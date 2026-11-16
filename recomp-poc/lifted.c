void f_8049380(Cpu* c){
 L_8049380: /* mov    eax,DWORD PTR [esp+0x4] */
  c->eax=LD32((c->esp+0x4));
 L_8049384: /* fld    DWORD PTR [eax] */
  PUSH((double)LDF((c->eax+0x0)));
 L_8049386: /* fld    DWORD PTR [eax+0x4] */
  PUSH((double)LDF((c->eax+0x4)));
 L_8049389: /* fld    st(0) */
  {double t=ST(0); PUSH(t);}
 L_804938b: /* fmul   st,st(1) */
  ST(0)=R(ST(0)*ST(1));
 L_804938d: /* fld    st(2) */
  {double t=ST(2); PUSH(t);}
 L_804938f: /* fmul   st,st(3) */
  ST(0)=R(ST(0)*ST(3));
 L_8049391: /* faddp  st(1),st */
  ST(1)=R(ST(1)+ST(0)); POP();
 L_8049393: /* fld    DWORD PTR [eax+0x8] */
  PUSH((double)LDF((c->eax+0x8)));
 L_8049396: /* fld    st(0) */
  {double t=ST(0); PUSH(t);}
 L_8049398: /* fmul   st,st(1) */
  ST(0)=R(ST(0)*ST(1));
 L_804939a: /* faddp  st(2),st */
  ST(2)=R(ST(2)+ST(0)); POP();
 L_804939c: /* fld1 */
  PUSH(1.0);
 L_804939e: /* fldz */
  PUSH(0.0);
 L_80493a0: /* fucomip st,st(3) */
  {double a=ST(0),b=ST(3); c->pf=(a!=a)||(b!=b); c->zf=c->pf||a==b; c->cf=c->pf||a<b;} POP();
 L_80493a2: /* fld    st(0) */
  {double t=ST(0); PUSH(t);}
 L_80493a4: /* jae    80493da <NormaliseAndMag+0x5a> */
  if(!c->cf) goto L_80493da;
 L_80493a6: /* fstp   st(1) */
  ST(1)=ST(0); POP();
 L_80493a8: /* fstp   st(0) */
  ST(0)=ST(0); POP();
 L_80493aa: /* fxch   st(1) */
  {double t=ST(0); ST(0)=ST(1); ST(1)=t;}
 L_80493ac: /* fsqrt */
  ST(0)=R(sqrt(ST(0)));
 L_80493ae: /* fld1 */
  PUSH(1.0);
 L_80493b0: /* fdivr  st(1),st */
  ST(1)=R(ST(0)/ST(1));
 L_80493b2: /* fxch   st(4) */
  {double t=ST(0); ST(0)=ST(4); ST(4)=t;}
 L_80493b4: /* fmul   st,st(1) */
  ST(0)=R(ST(0)*ST(1));
 L_80493b6: /* fxch   st(3) */
  {double t=ST(0); ST(0)=ST(3); ST(3)=t;}
 L_80493b8: /* fmul   st,st(1) */
  ST(0)=R(ST(0)*ST(1));
 L_80493ba: /* fstp   DWORD PTR [eax+0x4] */
  STF((c->eax+0x4),(float)ST(0)); POP();
 L_80493bd: /* fmul   st(1),st */
  ST(1)=R(ST(1)*ST(0));
 L_80493bf: /* fxch   st(1) */
  {double t=ST(0); ST(0)=ST(1); ST(1)=t;}
 L_80493c1: /* fstp   DWORD PTR [eax+0x8] */
  STF((c->eax+0x8),(float)ST(0)); POP();
 L_80493c4: /* fdivp  st(2),st */
  ST(2)=R(ST(2)/ST(0)); POP();
 L_80493c6: /* fldz */
  PUSH(0.0);
 L_80493c8: /* fldz */
  PUSH(0.0);
 L_80493ca: /* fldz */
  PUSH(0.0);
 L_80493cc: /* fldz */
  PUSH(0.0);
 L_80493ce: /* fxch   st(3) */
  {double t=ST(0); ST(0)=ST(3); ST(3)=t;}
 L_80493d0: /* fxch   st(5) */
  {double t=ST(0); ST(0)=ST(5); ST(5)=t;}
 L_80493d2: /* fxch   st(2) */
  {double t=ST(0); ST(0)=ST(2); ST(2)=t;}
 L_80493d4: /* fxch   st(4) */
  {double t=ST(0); ST(0)=ST(4); ST(4)=t;}
 L_80493d6: /* fxch   st(1) */
  {double t=ST(0); ST(0)=ST(1); ST(1)=t;}
 L_80493d8: /* fxch   st(2) */
  {double t=ST(0); ST(0)=ST(2); ST(2)=t;}
 L_80493da: /* fstp   st(5) */
  ST(5)=ST(0); POP();
 L_80493dc: /* fstp   st(3) */
  ST(3)=ST(0); POP();
 L_80493de: /* fstp   st(0) */
  ST(0)=ST(0); POP();
 L_80493e0: /* fstp   st(0) */
  ST(0)=ST(0); POP();
 L_80493e2: /* fstp   DWORD PTR [eax] */
  STF((c->eax+0x0),(float)ST(0)); POP();
 L_80493e4: /* ret */
  return;
}
