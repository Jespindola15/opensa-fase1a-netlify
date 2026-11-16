void* memset(void* d,int v,unsigned long n){unsigned char*p=d;while(n--)*p++=(unsigned char)v;return d;}
#include "gen.h"
#include "rt.h"
#include "lifted.c"
u8 G[65536];
static float out[4*1000000];
__attribute__((export_name("run"))) float* run(unsigned n){
  for(unsigned i=0;i<n;i++){
    Cpu cpu={0}; Cpu* c=&cpu; c->esp=0x8000; c->top=0;
    float v[3]={pick(i),pick(i+5),pick(i+11)}; __builtin_memcpy(G+0x1000,v,12);
    u32 p=0x1000; __builtin_memcpy(G+c->esp+4,&p,4);          /* argumento en la pila: [esp+4] = &vector */
    f_8049380(c);
    float r=(float)ST(0); __builtin_memcpy(&out[4*i],G+0x1000,12); out[4*i+3]=r; }
  return out; }
