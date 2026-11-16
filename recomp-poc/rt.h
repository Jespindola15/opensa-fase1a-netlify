/* Runtime mínimo del traductor: CPU x86 de invitado + memoria plana de 32 bits dentro de la memoria lineal de WASM */
typedef unsigned int u32; typedef unsigned char u8;
typedef struct { u32 eax, esp; int cf, zf, pf; double st[8]; int top; } Cpu;
extern u8 G[];                                   /* memoria del invitado: dirección x86 == índice */
static inline u32 LD32(u32 a){u32 v;__builtin_memcpy(&v,G+a,4);return v;}
static inline float LDF(u32 a){float v;__builtin_memcpy(&v,G+a,4);return v;}
static inline void STF(u32 a,float v){__builtin_memcpy(G+a,&v,4);}
#define ST(i) c->st[(c->top+(i))&7]
#define PUSH(v) (c->top=(c->top-1)&7, c->st[c->top]=(v))
#define POP() (c->top=(c->top+1)&7)
static inline double sqrt(double x){return __builtin_sqrt(x);}
#if PC==0   /* control de precisión x87 = 24 bits, exponente extendido: redondeo de la mantisa con división de Veltkamp */
static inline double R(double x){ double t=x*536870913.0; return t-(t-x); }
#else       /* 53 bits: double nativo */
static inline double R(double x){ return x; }
#endif
