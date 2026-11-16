#include "gen.h"
typedef struct { float x, y, z; } CVector; float NormaliseAndMag(CVector* v);
static void wr(const void*b,unsigned n){ int r; __asm__ volatile("int $0x80":"=a"(r):"a"(4),"b"(1),"c"(b),"d"(n):"memory"); }
static float buf[4*4096];
void _start(void){
  unsigned short cw; int pc = PC; /* 0=24 bits (single), 2=53 bits (double), 3=64 bits (extended) */
  __asm__ volatile("fnstcw %0":"=m"(cw)); cw=(cw&~0x300)|(pc<<8); __asm__ volatile("fldcw %0"::"m"(cw));
  unsigned n=0;
  for(unsigned i=0;i<N;i++){ CVector v={pick(i),pick(i+5),pick(i+11)}; float r=NormaliseAndMag(&v);
    buf[n++]=v.x;buf[n++]=v.y;buf[n++]=v.z;buf[n++]=r; if(n==4*4096){wr(buf,sizeof buf);n=0;} }
  if(n) wr(buf,n*4); __asm__ volatile("int $0x80"::"a"(1),"b"(0)); for(;;); }
