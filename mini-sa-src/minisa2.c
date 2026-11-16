/* Mini-SA 2: ejecutable Win32 de 32 bits propio que recorre los mismos caminos de API que gta_sa.exe:
   - Direct3D 9 cargado con LoadLibraryA/GetProcAddress (como RenderWare), vertex shader vs_1_1,
     vertex/index buffers, vertex declaration, texturas DXT1/DXT5, alpha blend/test, niebla,
     render target (reflejos/post-efectos) y ~1500 draw calls por cuadro.
   - DirectInput 8 por GetProcAddress: teclado (GetDeviceState) y mouse (DIMOUSESTATE2).
   - DirectSound 8 por CoCreateInstance(CLSID_DirectSound8): buffer secundario en streaming con
     Lock/Unlock desde un hilo, como CAEStreamThread; EAX.DLL ordinal 6; vorbisfile.dll (radio Ogg).
   - Hilos tipo CdStream: semáforos, eventos, sección crítica y lecturas aleatorias de sectores de 2048
     bytes de un archivo de 64 MiB con SetFilePointer/ReadFile.
   - Física x87 a 50 Hz con precisión de 24 bits (idéntica a Mini-SA 1) y mediciones de tiempo por cuadro.
   Sin CRT. No contiene código ni datos de Rockstar. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define INITGUID
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <d3d9.h>
#include <dinput.h>
#include <mmsystem.h>
#include <dsound.h>

#define NCARS 64
#define NBLD 1500
#define W 960
#define H 540
#define WORLD_BYTES (8u*1024u*1024u)
#define STREAM_BYTES (64u*1024u*1024u)
#define CHUNK (256u*1024u)

void* memset(void* d, int v, unsigned n){ unsigned char* p=d; while(n--) *p++=(unsigned char)v; return d; }
void* memcpy(void* d, const void* s, unsigned n){ unsigned char* p=d; const unsigned char* q=s; while(n--) *p++=*q++; return d; }

/* ---------------- log ---------------- */
static HANDLE logf; static LARGE_INTEGER qpf, qt0;
static unsigned slen(const char* s){ unsigned n=0; while(s[n]) n++; return n; }
static char* uhex(char* o, unsigned v){ for(int i=7;i>=0;i--){ unsigned d=(v>>(i*4))&15; *o++=d<10?'0'+d:'a'+d-10; } return o; }
static char* udec(char* o, unsigned v){ char t[12]; int n=0; do{ t[n++]='0'+v%10; v/=10; }while(v); while(n) *o++=t[--n]; return o; }
static void logline(const char* a, unsigned v, int hex){ char b[160]; char* o=b; unsigned n=slen(a); if(n>120) n=120; for(unsigned i=0;i<n;i++) *o++=a[i]; o=hex?uhex(o,v):udec(o,v); *o++='\r'; *o++='\n'; DWORD w; WriteFile(logf,b,(DWORD)(o-b),&w,0); *o=0; OutputDebugStringA(b); }
static unsigned now_us(void){ LARGE_INTEGER t; QueryPerformanceCounter(&t); return (unsigned)((t.QuadPart-qt0.QuadPart)*1000000/qpf.QuadPart); }
static unsigned now_ms(void){ return now_us()/1000; }

/* Estado de cada subsistema para el HUD: 0 pendiente, 1 ok, 2 falla */
enum { S_D3DDYN, S_VS, S_DXT, S_RT, S_VBIB, S_DIKBD, S_DIMOUSE, S_DSOUND, S_DSPLAY, S_EAX, S_VORBIS, S_CDSTREAM, S_WORLD, S_COUNT };
static volatile LONG sub[S_COUNT];
static const char* const SUBNAME[S_COUNT]={"d3d9 dinamico","vertex shader","DXT1/DXT5","render target","VB/IB+decl","DI8 teclado","DI8 mouse","DirectSound8","DSound play","EAX ord6","vorbisfile","CdStream","world.dat"};
static void setsub(int i, int ok, unsigned info){ sub[i]=ok?1:2; char b[64]; char* o=b; const char* p=SUBNAME[i]; while(*p) *o++=*p++; const char* r=ok?" OK info=":" FALLA info="; while(*r) *o++=*r++; *o=0; logline(b,info,1); }

/* ---------------- FPU / física (idéntica a Mini-SA 1) ---------------- */
typedef struct Car Car;
typedef struct { void (*update)(Car*, float); float (*mass)(const Car*); } CarVtbl;
struct Car { const CarVtbl* vt; float px, py, pz, vx, vy, vz, heading, steer, susp, suspv; int state, kind; unsigned color; };
static Car cars[NCARS];
static void fpu_set_pc24(void){ unsigned short cw; __asm__ volatile("fnstcw %0":"=m"(cw)); cw=(unsigned short)(cw&~0x300); __asm__ volatile("fldcw %0"::"m"(cw)); }
static int ftoi_trunc(float f){ unsigned short cw,t; int r; __asm__ volatile("fnstcw %0":"=m"(cw)); t=cw|0x0C00; __asm__ volatile("fldcw %0"::"m"(t)); __asm__ volatile("fistpl %0":"=m"(r):"t"(f):"st"); __asm__ volatile("fldcw %0"::"m"(cw)); return r; }
static float fsin(float x){ const float PI=3.14159265f, TP=6.2831853f; while(x>PI) x-=TP; while(x<-PI) x+=TP; float x2=x*x; return x*(1.0f-x2*(1.0f/6-x2*(1.0f/120-x2*(1.0f/5040)))); }
static float fcos(float x){ return fsin(x+1.5707963f); }
static float sedan_mass(const Car* c){ (void)c; return 1400.0f; }
static float bike_mass(const Car* c){ (void)c; return 400.0f; }
static float truck_mass(const Car* c){ (void)c; return 5000.0f; }
static void drive(Car* c, float dt){
  float accel;
  switch(c->state){
    case 0: accel= 6.0f; c->steer= 0.30f; break; case 1: accel= 9.0f; c->steer=-0.20f; break;
    case 2: accel=-4.0f; c->steer= 0.00f; break; case 3: accel= 3.0f; c->steer= 0.55f; break;
    case 4: accel= 7.5f; c->steer=-0.45f; break; case 5: accel= 0.0f; c->steer= 0.10f; break;
    case 6: accel=12.0f; c->steer= 0.05f; break; default: accel=2.0f; c->steer=-0.6f; break; }
  float m=c->vt->mass(c), sp=__builtin_sqrtf(c->vx*c->vx+c->vy*c->vy)+1e-4f;
  c->heading+=c->steer*sp*0.02f*dt*(1400.0f/m+0.5f);
  float hx=fcos(c->heading), hy=fsin(c->heading);
  c->vx+=hx*accel*dt - c->vx*0.35f*dt; c->vy+=hy*accel*dt - c->vy*0.35f*dt;
  float f=-90.0f*c->susp-9.0f*c->suspv; c->suspv+=f*dt; c->susp+=c->suspv*dt;
  c->vz-=9.81f*dt; c->pz+=c->vz*dt; if(c->pz<0.5f){ c->pz=0.5f; c->suspv-=c->vz*0.3f; c->vz=0; }
  c->px+=c->vx*dt; c->py+=c->vy*dt;
  if(c->px> 120){c->px= 120;c->vx=-c->vx*0.6f;} if(c->px<-120){c->px=-120;c->vx=-c->vx*0.6f;}
  if(c->py> 120){c->py= 120;c->vy=-c->vy*0.6f;} if(c->py<-120){c->py=-120;c->vy=-c->vy*0.6f;} }
static const CarVtbl VT_SEDAN={drive,sedan_mass}, VT_BIKE={drive,bike_mass}, VT_TRUCK={drive,truck_mass};
static const CarVtbl* const VTS[3]={&VT_SEDAN,&VT_BIKE,&VT_TRUCK};
static void collide(void){
  for(int i=0;i<NCARS;i++) for(int j=i+1;j<NCARS;j++){
    Car *a=&cars[i],*b=&cars[j]; float dx=b->px-a->px, dy=b->py-a->py, d2=dx*dx+dy*dy;
    if(d2<9.0f && d2>1e-6f){ float d=__builtin_sqrtf(d2), nx=dx/d, ny=dy/d, ma=a->vt->mass(a), mb=b->vt->mass(b);
      float rv=(b->vx-a->vx)*nx+(b->vy-a->vy)*ny; if(rv<0){ float imp=-1.6f*rv/(1/ma+1/mb);
        a->vx-=imp/ma*nx; a->vy-=imp/ma*ny; b->vx+=imp/mb*nx; b->vy+=imp/mb*ny; a->susp+=0.05f; b->susp+=0.05f; } } } }
__attribute__((noinline)) void physics_step(int frame){ const float dt=0.02f;
  for(int i=0;i<NCARS;i++){ Car* c=&cars[i]; if(((frame+i*7)%97)==0) c->state=(c->state+1+(frame&3))&7; c->vt->update(c,dt); } collide(); }
__attribute__((noinline)) unsigned state_hash(void){ unsigned h=2166136261u; const unsigned char* p=(const unsigned char*)cars;
  for(unsigned i=0;i<sizeof cars;i++){ if(i%sizeof(Car)<sizeof(void*)) continue; h=(h^p[i])*16777619u; } return h; }
__attribute__((noinline)) void init_cars(void){ unsigned s=12345u;
  for(int i=0;i<NCARS;i++){ Car* c=&cars[i]; s=s*1103515245u+12345u; c->kind=(int)((s>>16)%3); c->vt=VTS[c->kind];
    c->px=(float)ftoi_trunc((float)(int)((s>>8)%200u)-100.0f); c->py=(float)(int)((s>>4)%200u)-100.0f; c->pz=0.5f;
    c->heading=(float)(int)(s%628u)*0.01f; c->state=(int)(s%8u); c->color=0xff000000u|(s&0x00ffffffu); } }

/* ---------------- hilo world.dat (igual que Mini-SA 1) ---------------- */
static volatile LONG load_done; static volatile unsigned load_bytes, load_sum; static unsigned load_ms;
static DWORD WINAPI loader(LPVOID p){ (void)p; unsigned t0=now_ms();
  HANDLE f=CreateFileA("world.dat",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
  if(f==INVALID_HANDLE_VALUE){ setsub(S_WORLD,0,GetLastError()); InterlockedExchange(&load_done,2); return 1; }
  static unsigned char buf[CHUNK]; DWORD got; unsigned sum=0;
  while(ReadFile(f,buf,CHUNK,&got,0) && got){ for(DWORD i=0;i<got;i+=4) sum=sum*31u+*(unsigned*)(buf+i); load_bytes+=got; load_sum=sum; Sleep(0); }
  CloseHandle(f); load_ms=now_ms()-t0; setsub(S_WORLD,load_bytes==WORLD_BYTES,load_sum); InterlockedExchange(&load_done,1); return 0; }

/* ---------------- CdStream: 2 canales, semáforos, sectores de 2048 bytes ---------------- */
#define NCH 2
#define NREQ 3000
typedef struct { HANDLE sem, done; volatile unsigned sector, count; volatile LONG busy; unsigned char buf[16*2048]; } Channel;
static Channel ch[NCH]; static HANDLE imgf; static CRITICAL_SECTION cs; static volatile unsigned cd_reads, cd_bytes, cd_sum, cd_max_us, cd_tot_us;
static volatile LONG cd_quit; static volatile unsigned audio_busy_us, cd_busy_us; static unsigned t_present, prev_audio, prev_cd, prev_cdreads, prev_cdbytes;
static DWORD WINAPI cdstream_thread(LPVOID p){ Channel* c=(Channel*)p;
  for(;;){ WaitForSingleObject(c->sem,INFINITE); if(cd_quit) return 0; unsigned t0=now_us(); DWORD got=0;
    EnterCriticalSection(&cs);              /* el handle de archivo es compartido, como en CdStream */
    SetFilePointer(imgf,(LONG)(c->sector*2048u),0,FILE_BEGIN); ReadFile(imgf,c->buf,c->count*2048u,&got,0);
    LeaveCriticalSection(&cs);
    unsigned s=0; for(DWORD i=0;i<got;i+=4) s=s*31u+*(unsigned*)(c->buf+i);
    unsigned dt=now_us()-t0; InterlockedIncrement((LONG*)&cd_reads); cd_bytes+=got; cd_sum^=s+c->sector; cd_tot_us+=dt; cd_busy_us+=dt; if(dt>cd_max_us) cd_max_us=dt;
    InterlockedExchange(&c->busy,0); SetEvent(c->done); } }
static unsigned cd_rng=777u, cd_issued, cd_start_ms, cd_ms;
static void cdstream_pump(void){ /* el hilo principal emite pedidos como CStreaming::RequestModel */
  if(!imgf) return;
  HANDLE evs[NCH]; for(int i=0;i<NCH;i++) evs[i]=ch[i].done;
  for(int i=0;i<NCH;i++) if(!ch[i].busy && cd_issued<NREQ){ cd_rng=cd_rng*1664525u+1013904223u;
    ch[i].count=1+(cd_rng>>28); ch[i].sector=(cd_rng>>8)%(STREAM_BYTES/2048u-16u); InterlockedExchange(&ch[i].busy,1); cd_issued++; ReleaseSemaphore(ch[i].sem,1,0); }
  WaitForMultipleObjects(NCH,evs,FALSE,0);
  if(cd_issued>=NREQ && !ch[0].busy && !ch[1].busy && sub[S_CDSTREAM]==0){ cd_ms=now_ms()-cd_start_ms;
    setsub(S_CDSTREAM,cd_reads==NREQ,cd_sum); logline("cdstream reads=",cd_reads,0); logline("cdstream bytes=",cd_bytes,0);
    logline("cdstream ms=",cd_ms,0); logline("cdstream avg_us=",cd_tot_us/(cd_reads?cd_reads:1),0); logline("cdstream max_us=",cd_max_us,0); } }
static void cdstream_init(void){
  imgf=CreateFileA("stream.img",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(imgf==INVALID_HANDLE_VALUE){ imgf=0; setsub(S_CDSTREAM,0,GetLastError()); return; }
  InitializeCriticalSection(&cs); cd_start_ms=now_ms();
  for(int i=0;i<NCH;i++){ ch[i].sem=CreateSemaphoreA(0,0,2,0); ch[i].done=CreateEventA(0,FALSE,FALSE,0);
    HANDLE t=CreateThread(0,0x10000,cdstream_thread,&ch[i],CREATE_SUSPENDED,0); SetThreadPriority(t,THREAD_PRIORITY_ABOVE_NORMAL); ResumeThread(t); } }

/* ---------------- DirectInput 8 ---------------- */
typedef HRESULT (WINAPI *DI8Create_t)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
static IDirectInputDevice8A *kbd, *mouse; static unsigned char keys[256]; static DIMOUSESTATE2 ms2; static int curx=W/2, cury=H/2; static unsigned key_events, mouse_moves;
static void dinput_init(HWND hw, HINSTANCE hi){
  HMODULE m=LoadLibraryA("dinput8.dll"); DI8Create_t f=m?(DI8Create_t)GetProcAddress(m,"DirectInput8Create"):0;
  IDirectInput8A* di=0; HRESULT hr=f?f(hi,DIRECTINPUT_VERSION,&IID_IDirectInput8A,(void**)&di,0):E_FAIL;
  if(FAILED(hr)){ setsub(S_DIKBD,0,(unsigned)hr); setsub(S_DIMOUSE,0,(unsigned)hr); return; }
  hr=IDirectInput8_CreateDevice(di,&GUID_SysKeyboard,&kbd,0);
  if(SUCCEEDED(hr)) hr=IDirectInputDevice8_SetDataFormat(kbd,&c_dfDIKeyboard);
  if(SUCCEEDED(hr)) hr=IDirectInputDevice8_SetCooperativeLevel(kbd,hw,DISCL_FOREGROUND|DISCL_NONEXCLUSIVE);
  if(SUCCEEDED(hr)) IDirectInputDevice8_Acquire(kbd); if(FAILED(hr)){ kbd=0; setsub(S_DIKBD,0,(unsigned)hr); }
  hr=IDirectInput8_CreateDevice(di,&GUID_SysMouse,&mouse,0);
  if(SUCCEEDED(hr)) hr=IDirectInputDevice8_SetDataFormat(mouse,&c_dfDIMouse2);
  if(SUCCEEDED(hr)) hr=IDirectInputDevice8_SetCooperativeLevel(mouse,hw,DISCL_FOREGROUND|DISCL_NONEXCLUSIVE);
  if(SUCCEEDED(hr)) IDirectInputDevice8_Acquire(mouse); if(FAILED(hr)){ mouse=0; setsub(S_DIMOUSE,0,(unsigned)hr); } }
static void dinput_poll(void){
  if(kbd){ unsigned char k[256]; if(FAILED(IDirectInputDevice8_GetDeviceState(kbd,256,k))){ IDirectInputDevice8_Acquire(kbd); }
    else { for(int i=0;i<256;i++) if((k[i]^keys[i])&0x80){ key_events++; if(sub[S_DIKBD]==0) setsub(S_DIKBD,1,(unsigned)i); logline("tecla DIK=",(unsigned)i,0); } memcpy(keys,k,256); } }
  if(mouse){ DIMOUSESTATE2 m; if(FAILED(IDirectInputDevice8_GetDeviceState(mouse,sizeof m,&m))){ IDirectInputDevice8_Acquire(mouse); }
    else { ms2=m; if(m.lX||m.lY){ mouse_moves++; curx+=m.lX; cury+=m.lY; if(curx<0)curx=0; if(curx>W)curx=W; if(cury<0)cury=0; if(cury>H)cury=H; if(sub[S_DIMOUSE]==0) setsub(S_DIMOUSE,1,(unsigned)(m.lX&0xffff)); } } } }

/* ---------------- DirectSound 8 + EAX + vorbisfile ---------------- */
static IDirectSound8* ds; static IDirectSoundBuffer* sbuf; static unsigned ds_bufbytes=44100*4/2, ds_written, ds_wpos, ds_gen_sum, ds_start_ms, ds_cursor_bytes, ds_last_play; static volatile LONG ds_quit;
static unsigned tone_phase;
static void gen_audio(short* p, unsigned frames){ for(unsigned i=0;i<frames;i++){ tone_phase+=1;
    int v=(int)((tone_phase*7u)&0x3fff)-0x2000;           /* diente de sierra entero: determinista */
    if((tone_phase/11025u)&1) v>>=1; p[2*i]=(short)v; p[2*i+1]=(short)(-v); ds_gen_sum=ds_gen_sum*31u+(unsigned)(unsigned short)v; } }
static DWORD WINAPI audio_thread(LPVOID pv){ (void)pv; /* como CAEStreamThread: rellena el anillo detrás del cursor de reproducción */
  while(!ds_quit){ DWORD play=0, wr=0; if(FAILED(IDirectSoundBuffer_GetCurrentPosition(sbuf,&play,&wr))) break;
    if(play!=ds_last_play){ ds_cursor_bytes+=(play+ds_bufbytes-ds_last_play)%ds_bufbytes; ds_last_play=play; }
    unsigned ahead=(ds_wpos+ds_bufbytes-play)%ds_bufbytes;
    if(ahead<ds_bufbytes/2){ void *a,*b; DWORD na,nb; unsigned want=ds_bufbytes/4;
      unsigned ta=now_us(); if(SUCCEEDED(IDirectSoundBuffer_Lock(sbuf,ds_wpos,want,&a,&na,&b,&nb,0))){ gen_audio(a,na/4); if(b) gen_audio(b,nb/4); IDirectSoundBuffer_Unlock(sbuf,a,na,b,nb); audio_busy_us+=now_us()-ta;
        ds_wpos=(ds_wpos+want)%ds_bufbytes; ds_written+=want; } }
    Sleep(5); }
  return 0; }
typedef struct OggVorbis_File_ { unsigned char opaque[1024]; } OggVorbis_File_;
typedef struct { size_t (*read)(void*,size_t,size_t,void*); int (*seek)(void*,long long,int); int (*close)(void*); long (*tell)(void*); } ov_callbacks_;
typedef int (*ov_open_cb_t)(void*,OggVorbis_File_*,const char*,long,ov_callbacks_);
typedef long (*ov_read_t)(OggVorbis_File_*,char*,int,int,int,int,int*);
typedef double (*ov_time_t)(OggVorbis_File_*,int); typedef double (*ov_tell_t)(OggVorbis_File_*); typedef int (*ov_seek_t)(OggVorbis_File_*,double);
typedef struct { int version, channels; long rate; } vorbis_info_; typedef vorbis_info_* (*ov_info_t)(OggVorbis_File_*,int); typedef int (*ov_clear_t)(OggVorbis_File_*);
static HANDLE oggf;
static size_t cb_read(void* p,size_t s,size_t n,void* f){ DWORD got=0; ReadFile((HANDLE)f,p,(DWORD)(s*n),&got,0); return s?got/s:0; }
static int cb_seek(void* f,long long o,int w){ return SetFilePointer((HANDLE)f,(LONG)o,0,w)==INVALID_SET_FILE_POINTER?-1:0; }
static int cb_close(void* f){ (void)f; return 0; }
static long cb_tell(void* f){ return (long)SetFilePointer((HANDLE)f,0,0,FILE_CURRENT); }
static void vorbis_test(void){
  HMODULE m=LoadLibraryA("vorbisfile.dll"); if(!m){ setsub(S_VORBIS,0,0xdead0001); return; }
  const char* const names[7]={"ov_open_callbacks","ov_clear","ov_time_total","ov_time_tell","ov_read","ov_info","ov_time_seek"};
  void* fn[7]; unsigned missing=0; for(int i=0;i<7;i++){ fn[i]=(void*)GetProcAddress(m,names[i]); if(!fn[i]){ missing|=1u<<i; logline(names[i],0,0); } }
  logline("vorbisfile exports faltantes mask=",missing,1);
  if(!fn[0]||!fn[4]){ setsub(S_VORBIS,0,missing); return; }
  oggf=CreateFileA("radio.ogg",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
  static OggVorbis_File_ vf; ov_callbacks_ cb={cb_read,cb_seek,cb_close,cb_tell};
  int r=((ov_open_cb_t)fn[0])(oggf,&vf,0,0,cb); if(r){ setsub(S_VORBIS,0,(unsigned)r); return; }
  vorbis_info_* vi=fn[5]?((ov_info_t)fn[5])(&vf,-1):0; if(vi){ logline("ogg canales=",(unsigned)vi->channels,0); logline("ogg hz=",(unsigned)vi->rate,0); }
  if(fn[2]) logline("ogg ms_total=",(unsigned)(((ov_time_t)fn[2])(&vf,-1)*1000.0),0);
  static char pcm[4096]; unsigned total=0, sum=0; int bs; long n;
  while((n=((ov_read_t)fn[4])(&vf,pcm,sizeof pcm,0,2,1,&bs))>0){ for(long i=0;i<n;i++){ sum=sum*31u+(unsigned char)pcm[i]; } total+=(unsigned)n; }
  logline("ogg pcm_bytes=",total,0); logline("ogg pcm_sum=",sum,1);
  if(fn[6]&&fn[3]){ ((ov_seek_t)fn[6])(&vf,1.5); logline("ogg tell_ms tras seek 1.5s=",(unsigned)(((ov_tell_t)fn[3])(&vf)*1000.0),0); }
  if(fn[1]) ((ov_clear_t)fn[1])(&vf);
  setsub(S_VORBIS,missing==0 && total==44100u*3u*4u,sum); }
static void dsound_init(HWND hw){
  CoInitialize(0);
  HRESULT hr=CoCreateInstance(&CLSID_DirectSound8,0,CLSCTX_INPROC_SERVER,&IID_IDirectSound8,(void**)&ds);
  if(SUCCEEDED(hr)) hr=IDirectSound8_Initialize(ds,0);
  if(SUCCEEDED(hr)) hr=IDirectSound8_SetCooperativeLevel(ds,hw,DSSCL_PRIORITY);
  if(FAILED(hr)){ setsub(S_DSOUND,0,(unsigned)hr); return; }
  DSBUFFERDESC d; memset(&d,0,sizeof d); d.dwSize=sizeof d; d.dwFlags=DSBCAPS_PRIMARYBUFFER; IDirectSoundBuffer* prim=0;
  WAVEFORMATEX wf; memset(&wf,0,sizeof wf); wf.wFormatTag=WAVE_FORMAT_PCM; wf.nChannels=2; wf.nSamplesPerSec=44100; wf.wBitsPerSample=16; wf.nBlockAlign=4; wf.nAvgBytesPerSec=44100*4;
  if(SUCCEEDED(IDirectSound8_CreateSoundBuffer(ds,&d,&prim,0))) IDirectSoundBuffer_SetFormat(prim,&wf);
  memset(&d,0,sizeof d); d.dwSize=sizeof d; d.dwFlags=DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_CTRLVOLUME|DSBCAPS_CTRLFREQUENCY|DSBCAPS_LOCSOFTWARE|DSBCAPS_GLOBALFOCUS;
  d.dwBufferBytes=ds_bufbytes; d.lpwfxFormat=&wf;
  hr=IDirectSound8_CreateSoundBuffer(ds,&d,&sbuf,0); if(FAILED(hr)){ setsub(S_DSOUND,0,(unsigned)hr); return; }
  void *a,*b; DWORD na,nb; if(SUCCEEDED(IDirectSoundBuffer_Lock(sbuf,0,ds_bufbytes,&a,&na,&b,&nb,0))){ gen_audio(a,na/4); IDirectSoundBuffer_Unlock(sbuf,a,na,b,nb); }
  ds_wpos=0; ds_written=ds_bufbytes;
  IDirectSoundBuffer_SetVolume(sbuf,-600); IDirectSoundBuffer_SetFrequency(sbuf,44100);
  hr=IDirectSoundBuffer_Play(sbuf,0,0,DSBPLAY_LOOPING); setsub(S_DSOUND,SUCCEEDED(hr),(unsigned)hr);
  ds_start_ms=now_ms(); CreateThread(0,0x10000,audio_thread,0,0,0);
  /* EAX: gta_sa.exe importa EAX.DLL por ordinal 6 */
  HMODULE e=LoadLibraryA("eax.dll"); FARPROC eaxf=e?GetProcAddress(e,(LPCSTR)6):0; setsub(S_EAX,eaxf!=0,(unsigned)(e!=0)); }

/* ---------------- Direct3D 9 ---------------- */
typedef IDirect3D9* (WINAPI *D3DCreate9_t)(UINT);
static IDirect3DDevice9* dev; static IDirect3DTexture9 *tdxt1, *tdxt5, *trt; static IDirect3DVertexShader9* vs; static IDirect3DVertexDeclaration9* decl;
static IDirect3DVertexBuffer9* vbb; static IDirect3DIndexBuffer9* ibb; static IDirect3DSurface9 *rtsurf, *backbuf;
typedef struct { float x,y,z; DWORD c; float u,v; } V;
#define FVF (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)
static void mident(D3DMATRIX* m){ memset(m,0,sizeof *m); m->_11=m->_22=m->_33=m->_44=1; }
static void mmul(D3DMATRIX* o,const D3DMATRIX* a,const D3DMATRIX* b){ D3DMATRIX r; for(int i=0;i<4;i++) for(int j=0;j<4;j++){ float s=0; for(int k=0;k<4;k++) s+=a->m[i][k]*b->m[k][j]; r.m[i][j]=s; } *o=r; }
static void proj(D3DMATRIX* m,float fov,float asp,float zn,float zf){ memset(m,0,sizeof *m); float ys=1.0f/(fsin(fov*0.5f)/fcos(fov*0.5f)); m->_11=ys/asp; m->_22=ys; m->_33=zf/(zf-zn); m->_34=1; m->_43=-zn*zf/(zf-zn); }
static void view(D3DMATRIX* m,float ex,float ey,float ez){
  float fx=-ex,fy=-ey,fz=-ez,fl=__builtin_sqrtf(fx*fx+fy*fy+fz*fz); fx/=fl;fy/=fl;fz/=fl;
  float rx=fy,ry=-fx,rz=0,rl=__builtin_sqrtf(rx*rx+ry*ry); rx/=rl;ry/=rl; float ux=ry*fz-rz*fy,uy=rz*fx-rx*fz,uz=rx*fy-ry*fx;
  memset(m,0,sizeof *m); m->_11=rx;m->_21=ry;m->_31=rz; m->_12=ux;m->_22=uy;m->_32=uz; m->_13=fx;m->_23=fy;m->_33=fz;
  m->_41=-(rx*ex+ry*ey+rz*ez); m->_42=-(ux*ex+uy*ey+uz*ez); m->_43=-(fx*ex+fy*ey+fz*ez); m->_44=1; }
static void quad(V* v,float x0,float y0,float x1,float y1,float z,DWORD c,float uv){ V q[6]={{x0,y0,z,c,0,0},{x1,y0,z,c,uv,0},{x1,y1,z,c,uv,uv},{x0,y0,z,c,0,0},{x1,y1,z,c,uv,uv},{x0,y1,z,c,0,uv}}; for(int i=0;i<6;i++) v[i]=q[i]; }
static void box(V* v,const Car* c){ float s=c->kind==2?2.2f:c->kind==1?0.8f:1.4f, hx=fcos(c->heading)*s, hy=fsin(c->heading)*s, z=c->pz+c->susp;
  V a={c->px-hx-hy*0.5f,c->py-hy+hx*0.5f,z,c->color,0,0}, b={c->px+hx-hy*0.5f,c->py+hy+hx*0.5f,z,c->color,1,0},
    d={c->px+hx+hy*0.5f,c->py+hy-hx*0.5f,z,c->color,1,1}, e={c->px-hx+hy*0.5f,c->py-hy-hx*0.5f,z,c->color,0,1},
    t={c->px+hx*1.2f,c->py+hy*1.2f,z+1.2f,0xffffffff,0.5f,0.5f};
  V q[12]={a,b,d,a,d,e,a,b,t,b,d,t}; for(int i=0;i<12;i++) v[i]=q[i]; }
/* vs_1_1 ensamblado a mano: oPos = v0 * c0..c3 ; oD0 = v1 * c4 ; oT0 = v2 */
static const DWORD VS11[]={ 0xFFFE0101,
  0x0000001F,0x80000000,0x900F0000, 0x0000001F,0x8000000A,0x900F0001, 0x0000001F,0x80000005,0x900F0002,
  0x00000009,0xC0010000,0x90E40000,0xA0E40000, 0x00000009,0xC0020000,0x90E40000,0xA0E40001,
  0x00000009,0xC0040000,0x90E40000,0xA0E40002, 0x00000009,0xC0080000,0x90E40000,0xA0E40003,
  0x00000005,0xD00F0000,0x90E40001,0xA0E40004, 0x00000001,0xE0030000,0x90E40002, 0x0000FFFF };
static const D3DVERTEXELEMENT9 DECL[]={ {0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},{0,12,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0},{0,16,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0}, D3DDECL_END() };
static unsigned dxt_sum;
static void make_dxt(void){ D3DLOCKED_RECT lr; HRESULT h1,h5;
  h1=IDirect3DDevice9_CreateTexture(dev,64,64,1,0,D3DFMT_DXT1,D3DPOOL_MANAGED,&tdxt1,0);
  if(SUCCEEDED(h1)&&SUCCEEDED(IDirect3DTexture9_LockRect(tdxt1,0,&lr,0,0))){ for(int by=0;by<16;by++){ unsigned* row=(unsigned*)((char*)lr.pBits+by*lr.Pitch); for(int bx=0;bx<16;bx++){
      unsigned short c0=(unsigned short)(((bx*2)<<11)|((by*4)<<5)|12), c1=(unsigned short)(((31-bx)<<11)|(10<<5)|(by*2)); unsigned idx=((bx^by)&1)?0x55AAFF00u:0x1B1B1B1Bu;
      row[bx*2]=c0|((unsigned)c1<<16); row[bx*2+1]=idx; dxt_sum=dxt_sum*31u+row[bx*2]+row[bx*2+1]; } } IDirect3DTexture9_UnlockRect(tdxt1,0); }
  h5=IDirect3DDevice9_CreateTexture(dev,32,32,1,0,D3DFMT_DXT5,D3DPOOL_MANAGED,&tdxt5,0);
  if(SUCCEEDED(h5)&&SUCCEEDED(IDirect3DTexture9_LockRect(tdxt5,0,&lr,0,0))){ for(int by=0;by<8;by++){ unsigned* row=(unsigned*)((char*)lr.pBits+by*lr.Pitch); for(int bx=0;bx<8;bx++){
      unsigned* blk=row+bx*4; blk[0]=0xffu|(0x20u<<8)|0x49240000u; blk[1]=0x92492492u; blk[2]=0xffe0u|(0x001fu<<16); blk[3]=0xE4E4E4E4u; dxt_sum=dxt_sum*31u+blk[0]+blk[2]; } } IDirect3DTexture9_UnlockRect(tdxt5,0); }
  setsub(S_DXT,SUCCEEDED(h1)&&SUCCEEDED(h5)&&tdxt1&&tdxt5,dxt_sum); }
static void make_city(void){ /* 1500 edificios en un VB/IB estático; se dibuja uno por llamada, como los modelos de SA */
  HRESULT h=IDirect3DDevice9_CreateVertexBuffer(dev,NBLD*8*sizeof(V),D3DUSAGE_WRITEONLY,FVF,D3DPOOL_MANAGED,&vbb,0);
  if(SUCCEEDED(h)) h=IDirect3DDevice9_CreateIndexBuffer(dev,36*2,D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_MANAGED,&ibb,0);
  if(FAILED(h)){ setsub(S_VBIB,0,(unsigned)h); return; }
  V* v; unsigned s=99u; if(SUCCEEDED(IDirect3DVertexBuffer9_Lock(vbb,0,0,(void**)&v,0))){ for(int i=0;i<NBLD;i++){ s=s*1103515245u+12345u;
      float cx=(float)(int)(s%4000u)-2000.0f, cy=(float)(int)((s>>12)%4000u)-2000.0f; if(cx>-140&&cx<140&&cy>-140&&cy<140) cx+=400; float w=6+(float)(s%17u), hgt=10+(float)((s>>5)%60u);
      DWORD col=0xff000000u|(0x606060u+((s>>3)&0x3f3f3fu)); for(int k=0;k<8;k++){ V* p=v+i*8+k; p->x=cx+((k&1)?w:-w); p->y=cy+((k&2)?w:-w); p->z=(k&4)?hgt:0; p->c=col; p->u=(k&1)?1.0f:0.0f; p->v=(k&4)?4.0f:0.0f; } }
    IDirect3DVertexBuffer9_Unlock(vbb); }
  static const WORD idx[36]={0,1,5,0,5,4, 1,3,7,1,7,5, 3,2,6,3,6,7, 2,0,4,2,4,6, 4,5,7,4,7,6, 0,2,3,0,3,1};
  WORD* ip; if(SUCCEEDED(IDirect3DIndexBuffer9_Lock(ibb,0,0,(void**)&ip,0))){ memcpy(ip,idx,sizeof idx); IDirect3DIndexBuffer9_Unlock(ibb); }
  setsub(S_VBIB,1,NBLD); }

static LRESULT CALLBACK wndproc(HWND h,UINT m,WPARAM w,LPARAM l){ if(m==WM_CLOSE||(m==WM_KEYDOWN&&w==VK_ESCAPE)){ PostQuitMessage(0); return 0; } return DefWindowProcA(h,m,w,l); }

static unsigned ft[600]; static unsigned ftn;
static void frame_stats(unsigned us){ ft[ftn%600]=us; ftn++;
  if(ftn%600==0){ unsigned s[600]; for(int i=0;i<600;i++) s[i]=ft[i];
    for(int i=1;i<600;i++){ unsigned x=s[i]; int j=i-1; while(j>=0&&s[j]>x){ s[j+1]=s[j]; j--; } s[j+1]=x; }
    unsigned long long tot=0; for(int i=0;i<600;i++) tot+=s[i];
    logline("frame_us mediana=",s[300],0); logline("frame_us p99=",s[594],0); logline("fps promedio=",(unsigned)(600000000ull/tot),0); } }

void __stdcall start(void){
  QueryPerformanceFrequency(&qpf); QueryPerformanceCounter(&qt0);
  logf=CreateFileA("minisa_log.txt",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
  logline("minisa2 start v4 ",4,0);
  HINSTANCE hi=GetModuleHandleA(0);
  WNDCLASSA wc; memset(&wc,0,sizeof wc); wc.lpfnWndProc=wndproc; wc.hInstance=hi; wc.lpszClassName="MiniSA2"; wc.hCursor=LoadCursorA(0,(LPCSTR)IDC_ARROW); RegisterClassA(&wc);
  HWND hw=CreateWindowExA(0,"MiniSA2","Mini-SA 2",WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,W,H,0,0,hi,0);
  /* Direct3D 9 cargado dinámicamente */
  HMODULE d3dm=LoadLibraryA("d3d9.dll"); D3DCreate9_t create=d3dm?(D3DCreate9_t)GetProcAddress(d3dm,"Direct3DCreate9"):0;
  IDirect3D9* d3d=create?create(D3D_SDK_VERSION):0; setsub(S_D3DDYN,d3d!=0,(unsigned)(create!=0));
  if(!d3d) ExitProcess(2);
  D3DCAPS9 caps; memset(&caps,0,sizeof caps); IDirect3D9_GetDeviceCaps(d3d,0,D3DDEVTYPE_HAL,&caps);
  logline("caps VertexShaderVersion=",(unsigned)caps.VertexShaderVersion,1); logline("caps MaxTextureWidth=",(unsigned)caps.MaxTextureWidth,0);
  logline("CheckDeviceFormat DXT1 hr=",(unsigned)IDirect3D9_CheckDeviceFormat(d3d,0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,0,D3DRTYPE_TEXTURE,D3DFMT_DXT1),1);
  D3DPRESENT_PARAMETERS pp; memset(&pp,0,sizeof pp); pp.Windowed=TRUE; pp.SwapEffect=D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat=D3DFMT_UNKNOWN;
  pp.EnableAutoDepthStencil=TRUE; pp.AutoDepthStencilFormat=D3DFMT_D24S8; pp.hDeviceWindow=hw; pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
  HRESULT hr=IDirect3D9_CreateDevice(d3d,0,D3DDEVTYPE_HAL,hw,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&dev);
  if(FAILED(hr)) hr=IDirect3D9_CreateDevice(d3d,0,D3DDEVTYPE_HAL,hw,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev);
  logline("CreateDevice hr=",(unsigned)hr,1); if(FAILED(hr)) ExitProcess(3);
  fpu_set_pc24();
  hr=IDirect3DDevice9_CreateVertexShader(dev,VS11,&vs); if(SUCCEEDED(hr)) hr=IDirect3DDevice9_CreateVertexDeclaration(dev,DECL,&decl); setsub(S_VS,SUCCEEDED(hr)&&vs&&decl,(unsigned)hr);
  make_dxt(); make_city();
  hr=IDirect3DDevice9_CreateTexture(dev,256,256,1,D3DUSAGE_RENDERTARGET,D3DFMT_X8R8G8B8,D3DPOOL_DEFAULT,&trt,0);
  if(SUCCEEDED(hr)) hr=IDirect3DTexture9_GetSurfaceLevel(trt,0,&rtsurf); IDirect3DDevice9_GetRenderTarget(dev,0,&backbuf);
  unsigned t_init=now_ms();
  init_cars();
  { /* benchmark de física sin GPU en juego: 3000 pasos sobre una copia, luego se restaura el estado */
    static Car saved[NCARS]; memcpy(saved,cars,sizeof cars); unsigned b0=now_us(); for(int f=0;f<3000;f++) physics_step(f);
    unsigned dt=now_us()-b0; logline("bench fisica 3000 pasos us=",dt,0); logline("bench hash=",state_hash(),1); memcpy(cars,saved,sizeof cars); }
  dinput_init(hw,hi); dsound_init(hw); vorbis_test(); cdstream_init();
  logline("init_ms (dispositivos+audio+vorbis)=",now_ms()-t_init,0);
  CreateThread(0,0x10000,loader,0,0,0);
  static V vb[NCARS*12+256]; MSG msg; unsigned frame=0, fps_frames=0, fps_t=now_ms(), start_ms=now_ms(); int loaded_logged=0; unsigned rt_ok=0, t_phys=0, t_draw=0;
  for(;;){
    unsigned tf0=now_us();
    while(PeekMessageA(&msg,0,0,0,PM_REMOVE)){ if(msg.message==WM_QUIT) goto out; TranslateMessage(&msg); DispatchMessageA(&msg); }
    dinput_poll(); cdstream_pump();
    int done=load_done;
    unsigned tp0=now_us();
    if(done==1){ physics_step((int)frame); frame++;
      if(!loaded_logged){ loaded_logged=1; logline("load_ms=",load_ms,0); logline("load_sum=",load_sum,1); logline("first_frame_ms=",now_ms()-start_ms,0); }
      if(frame==1||frame==50||frame==500||frame==3000){ logline("frame ",frame,0); logline("  hash=",state_hash(),1); } }
    t_phys+=now_us()-tp0; unsigned td0=now_us();
    D3DMATRIX m, vw, pr, vp; float a=(float)frame*0.004f;
    /* 1) render target: vista cenital (como el agua/espejos de SA) */
    if(rtsurf){ IDirect3DDevice9_SetRenderTarget(dev,0,rtsurf); IDirect3DDevice9_Clear(dev,0,0,D3DCLEAR_TARGET,0xff203040,1.0f,0);
      IDirect3DDevice9_BeginScene(dev); mident(&m); IDirect3DDevice9_SetTransform(dev,D3DTS_WORLD,&m); view(&vw,0.1f,0.1f,300.0f); IDirect3DDevice9_SetTransform(dev,D3DTS_VIEW,&vw);
      proj(&pr,0.9f,1.0f,1.0f,1000.0f); IDirect3DDevice9_SetTransform(dev,D3DTS_PROJECTION,&pr);
      IDirect3DDevice9_SetRenderState(dev,D3DRS_LIGHTING,FALSE); IDirect3DDevice9_SetRenderState(dev,D3DRS_ZENABLE,FALSE); IDirect3DDevice9_SetRenderState(dev,D3DRS_CULLMODE,D3DCULL_NONE);
      IDirect3DDevice9_SetTexture(dev,0,0); IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_SELECTARG2); IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
      IDirect3DDevice9_SetFVF(dev,FVF); if(done==1){ for(int i=0;i<NCARS;i++) box(vb+i*12,&cars[i]); IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLELIST,NCARS*4,vb,sizeof(V)); }
      IDirect3DDevice9_EndScene(dev); IDirect3DDevice9_SetRenderTarget(dev,0,backbuf); if(!rt_ok){ rt_ok=1; setsub(S_RT,1,256); } }
    else if(!rt_ok){ rt_ok=1; setsub(S_RT,0,0); }
    /* 2) escena principal */
    IDirect3DDevice9_Clear(dev,0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xff9ec8ec,1.0f,0);
    IDirect3DDevice9_BeginScene(dev);
    mident(&m); IDirect3DDevice9_SetTransform(dev,D3DTS_WORLD,&m);
    view(&vw,fcos(a)*190.0f,fsin(a)*190.0f,95.0f); IDirect3DDevice9_SetTransform(dev,D3DTS_VIEW,&vw);
    proj(&pr,0.9f,(float)W/H,1.0f,3000.0f); IDirect3DDevice9_SetTransform(dev,D3DTS_PROJECTION,&pr);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_LIGHTING,FALSE); IDirect3DDevice9_SetRenderState(dev,D3DRS_CULLMODE,D3DCULL_NONE); IDirect3DDevice9_SetRenderState(dev,D3DRS_ZENABLE,D3DZB_TRUE);
    float fs=300.0f, fe=1800.0f; IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGENABLE,TRUE); IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGCOLOR,0xff9ec8ec);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGTABLEMODE,D3DFOG_LINEAR); IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGSTART,*(DWORD*)&fs); IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGEND,*(DWORD*)&fe);
    IDirect3DDevice9_SetFVF(dev,FVF);
    IDirect3DDevice9_SetTexture(dev,0,(IDirect3DBaseTexture9*)tdxt1); IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG1,D3DTA_TEXTURE); IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
    IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP); IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
    IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR); IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
    quad(vb,-2200,-2200,2200,2200,0,0xffffffff,120); IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLELIST,2,vb,sizeof(V));
    /* ciudad: 1500 DrawIndexedPrimitive con VB/IB */
    if(vbb&&ibb){ IDirect3DDevice9_SetStreamSource(dev,0,vbb,0,sizeof(V)); IDirect3DDevice9_SetIndices(dev,ibb);
      for(int i=0;i<NBLD;i++) IDirect3DDevice9_DrawIndexedPrimitive(dev,D3DPT_TRIANGLELIST,i*8,0,8,0,12); }
    IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGENABLE,FALSE);
    /* autos con vertex shader + declaración */
    if(vs&&decl&&done==1){ mmul(&vp,&vw,&pr); float c[20]; for(int r=0;r<4;r++) for(int k=0;k<4;k++) c[r*4+k]=vp.m[k][r]; c[16]=1.0f; c[17]=0.9f+0.1f*fsin(a*10); c[18]=0.9f; c[19]=1.0f;
      IDirect3DDevice9_SetVertexShader(dev,vs); IDirect3DDevice9_SetVertexDeclaration(dev,decl); IDirect3DDevice9_SetVertexShaderConstantF(dev,0,c,5);
      IDirect3DDevice9_SetTexture(dev,0,0); IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_SELECTARG2);
      for(int i=0;i<NCARS;i++) box(vb+i*12,&cars[i]); IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLELIST,NCARS*4,vb,sizeof(V));
      IDirect3DDevice9_SetVertexShader(dev,0); IDirect3DDevice9_SetFVF(dev,FVF); }
    /* cartel con DXT5 y alpha blend/test, y el render target en un panel */
    IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHABLENDENABLE,TRUE); IDirect3DDevice9_SetRenderState(dev,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA); IDirect3DDevice9_SetRenderState(dev,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHATESTENABLE,TRUE); IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHAREF,8); IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHAFUNC,D3DCMP_GREATER);
    IDirect3DDevice9_SetTexture(dev,0,(IDirect3DBaseTexture9*)tdxt5); IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1); IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
    { V q[6]={{-60,-130,5,0xffffffff,0,0},{60,-130,5,0xffffffff,3,0},{60,-130,60,0xffffffff,3,3},{-60,-130,5,0xffffffff,0,0},{60,-130,60,0xffffffff,3,3},{-60,-130,60,0xffffffff,0,3}}; IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLELIST,2,q,sizeof(V)); }
    IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHABLENDENABLE,FALSE); IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHATESTENABLE,FALSE);
    IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);
    /* HUD en pantalla */
    mident(&m); IDirect3DDevice9_SetTransform(dev,D3DTS_VIEW,&m); IDirect3DDevice9_SetTransform(dev,D3DTS_WORLD,&m);
    memset(&m,0,sizeof m); m._11=2.0f/W; m._22=-2.0f/H; m._33=1; m._41=-1; m._42=1; m._44=1; IDirect3DDevice9_SetTransform(dev,D3DTS_PROJECTION,&m);
    IDirect3DDevice9_SetRenderState(dev,D3DRS_ZENABLE,D3DZB_FALSE);
    if(trt){ IDirect3DDevice9_SetTexture(dev,0,(IDirect3DBaseTexture9*)trt); IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1); quad(vb,W-190,10,W-10,190,0,0xffffffff,1); IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLELIST,2,vb,sizeof(V)); }
    IDirect3DDevice9_SetTexture(dev,0,0); IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_SELECTARG2);
    int n=0; unsigned hsh=done==1?state_hash():0;
    for(int b=0;b<32;b++,n++) quad(vb+n*6,10+b*12,10,19+b*12,22,0,(hsh>>(31-b))&1?0xff20d020:0xffd02020,1);
    for(int i=0;i<S_COUNT;i++,n++) quad(vb+n*6,10+i*22,32,28+i*22,50,0,sub[i]==1?0xff20e020:sub[i]==2?0xffe02020:0xff808080,1);
    static const unsigned char K[6]={DIK_W,DIK_A,DIK_S,DIK_D,DIK_SPACE,DIK_LSHIFT};
    for(int i=0;i<6;i++,n++) quad(vb+n*6,10+i*22,60,28+i*22,78,0,(keys[K[i]]&0x80)?0xffffff00:0xff404040,1);
    for(int i=0;i<3;i++,n++) quad(vb+n*6,150+i*22,60,168+i*22,78,0,(ms2.rgbButtons[i]&0x80)?0xff00ffff:0xff404040,1);
    quad(vb+n*6,(float)curx-6,(float)cury-6,(float)curx+6,(float)cury+6,0,0xffff00ff,1); n++;
    float pr1=(float)load_bytes/(float)WORLD_BYTES, pr2=(float)cd_issued/(float)NREQ, pr3=(float)(ds_cursor_bytes%(44100u*4u*10u))/(44100.0f*4.0f*10.0f);
    quad(vb+n*6,10,H-40,10+(W-20)*pr1,H-32,0,0xffffc000,1); n++; quad(vb+n*6,10,H-28,10+(W-20)*pr2,H-20,0,0xff00c0ff,1); n++; quad(vb+n*6,10,H-16,10+(W-20)*pr3,H-8,0,0xffff60ff,1); n++;
    IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLELIST,(UINT)(n*2),vb,sizeof(V));
    IDirect3DDevice9_EndScene(dev); unsigned tpr=now_us(); t_draw+=tpr-td0; IDirect3DDevice9_Present(dev,0,0,0,0); t_present+=now_us()-tpr;
    frame_stats(now_us()-tf0);
    fps_frames++; unsigned t=now_ms(); if(t-fps_t>=2000){ unsigned fps=fps_frames*1000/(t-fps_t);
      { unsigned el=t-fps_t; unsigned ab=audio_busy_us, cb=cd_busy_us, cr=cd_reads, cbt=cd_bytes;
        char b[200]; char* o=b; const char* h="PERF fps="; while(*h)*o++=*h++; o=udec(o,fps);
        h=" fisica_us="; while(*h)*o++=*h++; o=udec(o,t_phys/fps_frames); h=" envio_draw_us="; while(*h)*o++=*h++; o=udec(o,t_draw/fps_frames);
        h=" present_us="; while(*h)*o++=*h++; o=udec(o,t_present/fps_frames); h=" audio_ms_por_s="; while(*h)*o++=*h++; o=udec(o,(ab-prev_audio)/el);
        h=" stream_ms_por_s="; while(*h)*o++=*h++; o=udec(o,(cb-prev_cd)/el); h=" stream_lecturas_s="; while(*h)*o++=*h++; o=udec(o,(cr-prev_cdreads)*1000/el);
        h=" stream_KB_s="; while(*h)*o++=*h++; o=udec(o,(cbt-prev_cdbytes)/el); *o=0; logline(b,0,0);
        prev_audio=ab; prev_cd=cb; prev_cdreads=cr; prev_cdbytes=cbt; t_present=0; }
      if(sbuf){ unsigned el=t-ds_start_ms; logline("  audio bytes reproducidos/s=",(unsigned)((unsigned long long)ds_cursor_bytes*1000ull/(el?el:1)),0); }
      if(sub[S_DSPLAY]==0 && sbuf && t-ds_start_ms>3000){ unsigned rate=(unsigned)((unsigned long long)ds_cursor_bytes*1000ull/(t-ds_start_ms)); setsub(S_DSPLAY,rate>150000&&rate<200000,rate); logline("  audio gen_sum=",ds_gen_sum,1); }
      fps_frames=0; fps_t=t; t_phys=t_draw=0; }
  }
out:
  ds_quit=1; cd_quit=1; logline("exit frames=",frame,0); logline("teclas=",key_events,0); logline("movimientos mouse=",mouse_moves,0); CloseHandle(logf); ExitProcess(0);
}
