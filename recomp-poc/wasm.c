#include "gen.h"
typedef struct { float x, y, z; } CVector; float NormaliseAndMag(CVector* v);
static float out[4*1000000];
__attribute__((export_name("run"))) float* run(unsigned n){ for(unsigned i=0;i<n;i++){ CVector v={pick(i),pick(i+5),pick(i+11)}; float r=NormaliseAndMag(&v);
  out[4*i]=v.x;out[4*i+1]=v.y;out[4*i+2]=v.z;out[4*i+3]=r; } return out; }
