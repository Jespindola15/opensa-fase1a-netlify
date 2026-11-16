/* Oráculo nativo: ejecuta en un x86 real los bytes originales de MINISA.EXE (física, init y hash). */
typedef unsigned u32;
static long sys(long n,long a,long b,long c,long d,long e,long f){ long r; __asm__ volatile("push %%ebp; mov %7,%%ebp; int $0x80; pop %%ebp":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c),"S"(d),"D"(e),"m"(f):"memory"); return r; }
static void out(const char* s){ int n=0; while(s[n]) n++; sys(4,1,(long)s,n,0,0,0); }
static void hex(u32 v){ char b[11]; for(int i=0;i<8;i++){ u32 d=(v>>((7-i)*4))&15; b[i]=d<10?'0'+d:'a'+d-10; } b[8]='\n'; b[9]=0; out(b); }
static void dec(u32 v){ char t[12],b[13]; int n=0,k=0; do{t[n++]='0'+v%10;v/=10;}while(v); while(n) b[k++]=t[--n]; b[k]=0; out(b); }
typedef void (*init_t)(void); typedef void (*step_t)(int); typedef u32 (*hash_t)(void);
void _start(void){
  long fd=sys(5,(long)"image.bin",0,0,0,0,0); if(fd<0){ out("no image\n"); sys(1,1,0,0,0,0,0); }
  unsigned char* base=(unsigned char*)sys(192,0x400000,319488,7,0x32,-1,0); /* mmap2 RWX MAP_FIXED|PRIVATE|ANON */
  if((long)base!=0x400000){ out("mmap failed\n"); sys(1,1,0,0,0,0,0); }
  long got=0,r; while((r=sys(3,fd,(long)base+got,65536,0,0,0))>0) got+=r;
  unsigned short cw=0x027F; __asm__ volatile("fldcw %0"::"m"(cw));          /* CW inicial de Windows */
  __asm__ volatile("fnstcw %0":"=m"(cw)); cw&=~0x300; __asm__ volatile("fldcw %0"::"m"(cw)); /* = fpu_set_pc24 */
  init_t init=(init_t)0x00401bf0; step_t step=(step_t)0x004019e0; hash_t hash=(hash_t)0x00401ba0;
  init();
  u32 t0,t1; __asm__ volatile("rdtsc":"=a"(t0)::"edx");
  for(int f=0;f<3000;f++){ step(f); int fr=f+1; if(fr==1||fr==50||fr==500||fr==3000){ out("frame "); dec(fr); out("  hash="); hex(hash()); } }
  __asm__ volatile("rdtsc":"=a"(t1)::"edx"); out("ciclos/paso="); dec((t1-t0)/3000);
  sys(1,0,0,0,0,0,0); }
