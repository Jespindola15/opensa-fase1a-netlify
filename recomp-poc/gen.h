/* Generador determinista de entradas (xorshift), incluye casos borde. N vectores -> salida: 4 floats (x,y,z,ret) por caso */
static unsigned s=0x12345678u; static unsigned rnd(void){s^=s<<13;s^=s>>17;s^=s<<5;return s;}
static float pick(unsigned i){ union{unsigned u;float f;}c; unsigned k=i%16;
  if(k==0){c.u=rnd()&0x807FFFFFu;return c.f;}            /* subnormales */
  if(k==1){c.u=(rnd()&0x80FFFFFFu)|0x7E000000u;return c.f;} /* enormes (~1e37..3e38) */
  if(k==2){c.u=(rnd()&0x80FFFFFFu)|0x01000000u;return c.f;} /* minúsculos normales */
  if(k==3){return (float)(int)(rnd()%7)-3.0f;}              /* enteros chicos, ceros */
  c.u=rnd(); if(((c.u>>23)&0xFF)==0xFF) c.u&=0xBFFFFFFFu; return c.f; } /* bits aleatorios sin NaN/Inf */
