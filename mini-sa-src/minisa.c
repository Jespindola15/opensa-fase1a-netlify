/* Mini-SA: ejecutable Win32 de 32 bits que reproduce los patrones de arranque de GTA San Andreas
   (ventana, Direct3D 9 con pipeline fijo, física x87 a 50 Hz con precisión de 24 bits, hilo de carga
   con CreateFile/ReadFile, llamadas indirectas tipo vtable y un switch con tabla de saltos).
   Sin CRT: solo kernel32, user32 y d3d9. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>

#define NCARS 64
#define W 960
#define H 540
#define WORLD_BYTES (8u*1024u*1024u)
#define CHUNK (256u*1024u)

typedef struct Car Car;
typedef struct { void (*update)(Car*, float); float (*mass)(const Car*); } CarVtbl;
struct Car { const CarVtbl* vt; float px, py, pz, vx, vy, vz, heading, steer, susp, suspv; int state, kind; unsigned color; };

static Car cars[NCARS];
static volatile LONG load_done; static volatile unsigned load_bytes, load_sum; static unsigned load_ms;
static LARGE_INTEGER qpf, qt0;
static HANDLE logf;

/* ---------- utilidades sin CRT ---------- */
static void* mset(void* d, int v, unsigned n){ unsigned char* p=d; while(n--) *p++=(unsigned char)v; return d; }
static unsigned slen(const char* s){ unsigned n=0; while(s[n]) n++; return n; }
static char* uhex(char* o, unsigned v){ for(int i=7;i>=0;i--){ unsigned d=(v>>(i*4))&15; *o++=d<10?'0'+d:'a'+d-10; } return o; }
static char* udec(char* o, unsigned v){ char t[12]; int n=0; do{ t[n++]='0'+v%10; v/=10; }while(v); while(n) *o++=t[--n]; return o; }
static void logline(const char* a, unsigned v, int hex){ char b[128]; char* o=b; unsigned n=slen(a); for(unsigned i=0;i<n;i++) *o++=a[i]; o=hex?uhex(o,v):udec(o,v); *o++='\r'; *o++='\n'; DWORD w; WriteFile(logf,b,(DWORD)(o-b),&w,0); *o=0; OutputDebugStringA(b); }
static unsigned now_ms(void){ LARGE_INTEGER t; QueryPerformanceCounter(&t); return (unsigned)((t.QuadPart-qt0.QuadPart)*1000/qpf.QuadPart); }

/* ---------- control de la FPU x87, como hace Direct3D 9 / RenderWare ---------- */
static void fpu_set_pc24(void){ unsigned short cw; __asm__ volatile("fnstcw %0":"=m"(cw)); cw=(unsigned short)((cw&~0x300)|0x000); __asm__ volatile("fldcw %0"::"m"(cw)); }
static int ftoi_trunc(float f){ unsigned short cw,t; int r; __asm__ volatile("fnstcw %0":"=m"(cw)); t=cw|0x0C00; __asm__ volatile("fldcw %0"::"m"(t)); __asm__ volatile("fistpl %0":"=m"(r):"t"(f):"st"); __asm__ volatile("fldcw %0"::"m"(cw)); return r; }

/* ---------- física (x87): suspensión, fricción, giro, choques ---------- */
static float fsin(float x){ /* reducción a [-pi,pi] + polinomio, todo en x87 */
  const float PI=3.14159265f, TP=6.2831853f; while(x>PI) x-=TP; while(x<-PI) x+=TP;
  float x2=x*x; return x*(1.0f-x2*(1.0f/6-x2*(1.0f/120-x2*(1.0f/5040)))); }
static float fcos(float x){ return fsin(x+1.5707963f); }
static float sedan_mass(const Car* c){ (void)c; return 1400.0f; }
static float bike_mass(const Car* c){ (void)c; return 400.0f; }
static float truck_mass(const Car* c){ (void)c; return 5000.0f; }
static void drive(Car* c, float dt){
  float accel;
  switch(c->state){            /* switch denso: el compilador genera tabla de saltos */
    case 0: accel= 6.0f; c->steer= 0.30f; break;
    case 1: accel= 9.0f; c->steer=-0.20f; break;
    case 2: accel=-4.0f; c->steer= 0.00f; break;
    case 3: accel= 3.0f; c->steer= 0.55f; break;
    case 4: accel= 7.5f; c->steer=-0.45f; break;
    case 5: accel= 0.0f; c->steer= 0.10f; break;
    case 6: accel=12.0f; c->steer= 0.05f; break;
    default: accel=2.0f; c->steer=-0.6f; break; }
  float m=c->vt->mass(c), sp=__builtin_sqrtf(c->vx*c->vx+c->vy*c->vy)+1e-4f;
  c->heading+=c->steer*sp*0.02f*dt*(1400.0f/m+0.5f);
  float hx=fcos(c->heading), hy=fsin(c->heading);
  c->vx+=hx*accel*dt - c->vx*0.35f*dt; c->vy+=hy*accel*dt - c->vy*0.35f*dt;
  /* suspensión: muelle-amortiguador */
  float f=-90.0f*c->susp-9.0f*c->suspv; c->suspv+=f*dt; c->susp+=c->suspv*dt;
  c->vz-=9.81f*dt; c->pz+=c->vz*dt; if(c->pz<0.5f){ c->pz=0.5f; c->suspv-=c->vz*0.3f; c->vz=0; }
  c->px+=c->vx*dt; c->py+=c->vy*dt;
  if(c->px> 120){c->px= 120;c->vx=-c->vx*0.6f;} if(c->px<-120){c->px=-120;c->vx=-c->vx*0.6f;}
  if(c->py> 120){c->py= 120;c->vy=-c->vy*0.6f;} if(c->py<-120){c->py=-120;c->vy=-c->vy*0.6f;}
}
static const CarVtbl VT_SEDAN={drive,sedan_mass}, VT_BIKE={drive,bike_mass}, VT_TRUCK={drive,truck_mass};
static const CarVtbl* const VTS[3]={&VT_SEDAN,&VT_BIKE,&VT_TRUCK};

static void collide(void){
  for(int i=0;i<NCARS;i++) for(int j=i+1;j<NCARS;j++){
    Car *a=&cars[i],*b=&cars[j]; float dx=b->px-a->px, dy=b->py-a->py, d2=dx*dx+dy*dy;
    if(d2<9.0f && d2>1e-6f){ float d=__builtin_sqrtf(d2), nx=dx/d, ny=dy/d, ma=a->vt->mass(a), mb=b->vt->mass(b);
      float rv=(b->vx-a->vx)*nx+(b->vy-a->vy)*ny; if(rv<0){ float imp=-1.6f*rv/(1/ma+1/mb);
        a->vx-=imp/ma*nx; a->vy-=imp/ma*ny; b->vx+=imp/mb*nx; b->vy+=imp/mb*ny; a->susp+=0.05f; b->susp+=0.05f; } } } }

__attribute__((noinline)) void physics_step(int frame){
  const float dt=0.02f;                          /* 50 Hz, como CTimer de SA */
  for(int i=0;i<NCARS;i++){ Car* c=&cars[i]; if(((frame+i*7)%97)==0) c->state=(c->state+1+(frame&3))&7; c->vt->update(c,dt); }
  collide(); }

__attribute__((noinline)) unsigned state_hash(void){ unsigned h=2166136261u; const unsigned char* p=(const unsigned char*)cars;
  for(unsigned i=0;i<sizeof cars;i++){ if(i%sizeof(Car)<sizeof(void*)) continue; h=(h^p[i])*16777619u; } return h; }

__attribute__((noinline)) void init_cars(void){ unsigned s=12345u;
  for(int i=0;i<NCARS;i++){ Car* c=&cars[i]; s=s*1103515245u+12345u; c->kind=(int)((s>>16)%3); c->vt=VTS[c->kind];
    c->px=(float)ftoi_trunc((float)(int)((s>>8)%200u)-100.0f); c->py=(float)(int)((s>>4)%200u)-100.0f; c->pz=0.5f;
    c->heading=(float)(int)(s%628u)*0.01f; c->state=(int)(s%8u); c->color=0xff000000u|(s&0x00ffffffu); } }

/* ---------- hilo de carga (streaming del mundo) ---------- */
static DWORD WINAPI loader(LPVOID p){ (void)p; unsigned t0=now_ms();
  HANDLE f=CreateFileA("world.dat",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
  if(f==INVALID_HANDLE_VALUE){ load_ms=0xffffffffu; InterlockedExchange(&load_done,2); return 1; }
  static unsigned char buf[CHUNK]; DWORD got; unsigned sum=0;
  while(ReadFile(f,buf,CHUNK,&got,0) && got){ for(DWORD i=0;i<got;i+=4) sum=sum*31u+*(unsigned*)(buf+i); load_bytes+=got; load_sum=sum; Sleep(0); }
  CloseHandle(f); load_ms=now_ms()-t0; InterlockedExchange(&load_done,1); return 0; }

/* ---------- Direct3D 9 ---------- */
typedef struct { float x,y,z; DWORD c; float u,v; } V;
#define FVF (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)
static IDirect3DDevice9* dev; static IDirect3DTexture9* tex;
static void mident(D3DMATRIX* m){ mset(m,0,sizeof *m); m->_11=m->_22=m->_33=m->_44=1; }
static void quad(V* v,float x0,float y0,float x1,float y1,float z,DWORD c,float uv){ V q[6]={{x0,y0,z,c,0,0},{x1,y0,z,c,uv,0},{x1,y1,z,c,uv,uv},{x0,y0,z,c,0,0},{x1,y1,z,c,uv,uv},{x0,y1,z,c,0,uv}}; for(int i=0;i<6;i++) v[i]=q[i]; }
static void box(V* v,const Car* c){ float s=c->kind==2?2.2f:c->kind==1?0.8f:1.4f, hx=fcos(c->heading)*s, hy=fsin(c->heading)*s, z=c->pz+c->susp;
  V a={c->px-hx-hy*0.5f,c->py-hy+hx*0.5f,z,c->color,0,0}, b={c->px+hx-hy*0.5f,c->py+hy+hx*0.5f,z,c->color,1,0},
    d={c->px+hx+hy*0.5f,c->py+hy-hx*0.5f,z,c->color,1,1}, e={c->px-hx+hy*0.5f,c->py-hy-hx*0.5f,z,c->color,0,1},
    t={c->px+hx*1.2f,c->py+hy*1.2f,z+1.2f,0xffffffff,0.5f,0.5f};
  V q[12]={a,b,d,a,d,e,a,b,t,b,d,t}; for(int i=0;i<12;i++) v[i]=q[i]; }
static void proj(D3DMATRIX* m,float fov,float asp,float zn,float zf){ mset(m,0,sizeof *m); float ys=1.0f/(fsin(fov*0.5f)/fcos(fov*0.5f)); m->_11=ys/asp; m->_22=ys; m->_33=zf/(zf-zn); m->_34=1; m->_43=-zn*zf/(zf-zn); }
static void view(D3DMATRIX* m,float ex,float ey,float ez){ /* mira al origen, Z arriba (como SA) */
  float fx=-ex,fy=-ey,fz=-ez,fl=__builtin_sqrtf(fx*fx+fy*fy+fz*fz); fx/=fl;fy/=fl;fz/=fl;
  float rx=fy,ry=-fx,rz=0,rl=__builtin_sqrtf(rx*rx+ry*ry); rx/=rl;ry/=rl; float ux=ry*fz-rz*fy,uy=rz*fx-rx*fz,uz=rx*fy-ry*fx;
  mset(m,0,sizeof *m); m->_11=rx;m->_21=ry;m->_31=rz; m->_12=ux;m->_22=uy;m->_32=uz; m->_13=fx;m->_23=fy;m->_33=fz;
  m->_41=-(rx*ex+ry*ey+rz*ez); m->_42=-(ux*ex+uy*ey+uz*ez); m->_43=-(fx*ex+fy*ey+fz*ez); m->_44=1; }

static LRESULT CALLBACK wndproc(HWND h,UINT m,WPARAM w,LPARAM l){ if(m==WM_CLOSE||(m==WM_KEYDOWN&&w==VK_ESCAPE)){ PostQuitMessage(0); return 0; } return DefWindowProcA(h,m,w,l); }

void __stdcall start(void){
  QueryPerformanceFrequency(&qpf); QueryPerformanceCounter(&qt0);
  logf=CreateFileA("minisa_log.txt",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
  logline("minisa start v2 ",2,0);
  WNDCLASSA wc; mset(&wc,0,sizeof wc); wc.lpfnWndProc=wndproc; wc.hInstance=GetModuleHandleA(0); wc.lpszClassName="MiniSA"; wc.hCursor=LoadCursorA(0,(LPCSTR)IDC_ARROW);
  RegisterClassA(&wc);
  HWND hw=CreateWindowExA(0,"MiniSA","Mini-SA (Win32 + D3D9 + x87)",WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,W,H,0,0,wc.hInstance,0);
  IDirect3D9* d3d=Direct3DCreate9(D3D_SDK_VERSION); if(!d3d){ logline("Direct3DCreate9 failed ",0,0); ExitProcess(2); }
  D3DPRESENT_PARAMETERS pp; mset(&pp,0,sizeof pp); pp.Windowed=TRUE; pp.SwapEffect=D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat=D3DFMT_UNKNOWN;
  pp.EnableAutoDepthStencil=TRUE; pp.AutoDepthStencilFormat=D3DFMT_D16; pp.hDeviceWindow=hw; pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
  HRESULT hr=IDirect3D9_CreateDevice(d3d,D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,hw,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev);
  logline("CreateDevice hr=",(unsigned)hr,1); if(FAILED(hr)) ExitProcess(3);
  fpu_set_pc24();                                   /* lo que D3D9 hace sin D3DCREATE_FPU_PRESERVE */
  /* textura procedural (asfalto a cuadros) vía LockRect */
  IDirect3DDevice9_CreateTexture(dev,64,64,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&tex,0);
  D3DLOCKED_RECT lr; if(tex && SUCCEEDED(IDirect3DTexture9_LockRect(tex,0,&lr,0,0))){ for(int y=0;y<64;y++){ DWORD* row=(DWORD*)((char*)lr.pBits+y*lr.Pitch); for(int x=0;x<64;x++) row[x]=((x/8+y/8)&1)?0xff505050:0xff3a3a3a; } IDirect3DTexture9_UnlockRect(tex,0); }
  init_cars();
  HANDLE th=CreateThread(0,0x10000,loader,0,0,0); (void)th;
  static V vb[NCARS*12+64]; MSG msg; unsigned frame=0, fps_frames=0, fps_t=now_ms(), fps=0, start_ms=now_ms(); int loaded_logged=0;
  for(;;){
    while(PeekMessageA(&msg,0,0,0,PM_REMOVE)){ if(msg.message==WM_QUIT) goto out; TranslateMessage(&msg); DispatchMessageA(&msg); }
    int done=load_done;
    if(done==1){ physics_step((int)frame); frame++;
      if(!loaded_logged){ loaded_logged=1; logline("load_ms=",load_ms,0); logline("load_bytes=",load_bytes,0); logline("load_sum=",load_sum,1); logline("first_frame_ms=",now_ms()-start_ms,0); }
      if(frame==1||frame==50||frame==500||frame==3000){ logline("frame ",frame,0); logline("  hash=",state_hash(),1); } }
    IDirect3DDevice9_Clear(dev,0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff7fb4e6,1.0f,0);
    IDirect3DDevice9_BeginScene(dev);
    D3DMATRIX m; mident(&m); IDirect3DDevice9_SetTransform(dev,D3DTS_WORLD,&m);
    float a=(float)frame*0.004f; view(&m,fcos(a)*170.0f,fsin(a)*170.0f,110.0f); IDirect3DDevice9_SetTransform(dev,D3DTS_VIEW,&m);
    proj(&m,0.9f,(float)W/H,1.0f,1000.0f); IDirect3DDevice9_SetTransform(dev,D3DTS_PROJECTION,&m);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_LIGHTING,FALSE); IDirect3DDevice9_SetRenderState(dev,D3DRS_CULLMODE,D3DCULL_NONE); IDirect3DDevice9_SetRenderState(dev,D3DRS_ZENABLE,D3DZB_TRUE);
    IDirect3DDevice9_SetFVF(dev,FVF);
    IDirect3DDevice9_SetTexture(dev,0,(IDirect3DBaseTexture9*)tex);
    IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_MODULATE); IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG1,D3DTA_TEXTURE); IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
    quad(vb,-125,-125,125,125,0,0xffffffff,16); IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLELIST,2,vb,sizeof(V));
    IDirect3DDevice9_SetTexture(dev,0,0); IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_SELECTARG2);
    if(done==1){ for(int i=0;i<NCARS;i++) box(vb+i*12,&cars[i]); IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLELIST,NCARS*4,vb,sizeof(V)); }
    /* HUD en espacio de pantalla: barra de carga y los 32 bits del hash como barras (verde=1, rojo=0) */
    mident(&m); IDirect3DDevice9_SetTransform(dev,D3DTS_VIEW,&m); IDirect3DDevice9_SetTransform(dev,D3DTS_WORLD,&m);
    mset(&m,0,sizeof m); m._11=2.0f/W; m._22=2.0f/H; m._33=1; m._41=-1; m._42=-1; m._44=1; IDirect3DDevice9_SetTransform(dev,D3DTS_PROJECTION,&m);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_ZENABLE,D3DZB_FALSE);
    float pr=(float)load_bytes/(float)WORLD_BYTES; quad(vb,20,H-30,20+(W-40)*pr,H-20,0,0xffffc000,1);
    unsigned hsh=done==1?state_hash():0; for(int b=0;b<32;b++) quad(vb+6+b*6,20+b*14,20,30+b*14,40,0,(hsh>>(31-b))&1?0xff20d020:0xffd02020,1);
    IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLELIST,2+64,vb,sizeof(V));
    IDirect3DDevice9_EndScene(dev); IDirect3DDevice9_Present(dev,0,0,0,0);
    fps_frames++; unsigned t=now_ms(); if(t-fps_t>=2000){ fps=fps_frames*1000/(t-fps_t); fps_frames=0; fps_t=t; logline("fps=",fps,0); }
  }
out:
  logline("exit frames=",frame,0); CloseHandle(logf); ExitProcess(0);
}
