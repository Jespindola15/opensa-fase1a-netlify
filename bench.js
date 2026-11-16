// Medidor Fase 1A (no forma parte de OpenSA): FPS, frame time, memoria y tiempo de carga.
(()=>{const B=window.__bench={fps:0,avg:0,p99:0,low1:0,heapMB:0,loadS:null,samples:[]};let ft=[],last=performance.now(),clickT=null,acc=0,n=0;
document.addEventListener('click',e=>{if(/choose game folder/i.test(e.target.textContent||''))clickT=performance.now()},true);
const el=document.createElement('div');el.style.cssText='position:fixed;left:8px;bottom:8px;z-index:99999;background:#000c;color:#0f0;font:12px monospace;padding:6px 8px;pointer-events:none;white-space:pre';
addEventListener('DOMContentLoaded',()=>document.body.appendChild(el));
const loading=()=>/STREAMING|PAINTING|UNFOLDING|ALMOST THERE|LOADING TEXTURES|WAXING/i.test(document.body?document.body.innerText:'');
function f(t){const d=t-last;last=t;ft.push(d);if(ft.length>600)ft.shift();acc+=d;n++;
 if(clickT&&B.loadS===null&&t-clickT>3000&&!loading()&&document.querySelector('canvas'))B.loadS=((t-clickT)/1000).toFixed(1);
 if(acc>=1000){B.fps=(n*1000/acc).toFixed(0);acc=0;n=0;const s=[...ft].sort((a,b)=>a-b);B.avg=(s.reduce((a,b)=>a+b,0)/s.length).toFixed(1);B.p99=s[Math.floor(s.length*.99)].toFixed(1);B.low1=(1000/s[Math.floor(s.length*.99)]).toFixed(0);
  B.heapMB=performance.memory?(performance.memory.usedJSHeapSize/1048576).toFixed(0):'?';B.samples.push({t:Math.round(t/1000),fps:+B.fps,heap:+B.heapMB});if(B.samples.length>900)B.samples.shift();
  el.textContent=`FPS ${B.fps}  (1% low ${B.low1})\nframe ${B.avg} ms  p99 ${B.p99} ms\nJS heap ${B.heapMB} MB\ncarga ${B.loadS??(clickT?'…':'-')} s`;}
 requestAnimationFrame(f)}requestAnimationFrame(f)})();

// Diagnóstico Fase 1A: muestra en pantalla el selector de carpetas y cualquier error, sin abrir la consola.
(()=>{const D=window.__diag=[];const box=document.createElement('div');
box.style.cssText='position:fixed;left:8px;bottom:84px;z-index:99999;max-width:60vw;background:#300c;color:#fbb;font:12px monospace;padding:6px 8px;white-space:pre-wrap;pointer-events:none;display:none';
addEventListener('DOMContentLoaded',()=>document.body.appendChild(box));
const L=(...a)=>{const m=a.map(x=>x&&x.message?x.name+': '+x.message:String(x)).join(' ').slice(0,300);D.push(m);if(D.length>8)D.shift();box.textContent='DIAGNÓSTICO\n'+D.join('\n');box.style.display='block'};
addEventListener('error',e=>L('error',e.message));addEventListener('unhandledrejection',e=>L('rechazo',e.reason));
const ce=console.error.bind(console);console.error=(...a)=>{L('console.error',...a);ce(...a)};
if(window.showDirectoryPicker){const o=window.showDirectoryPicker.bind(window);window.showDirectoryPicker=async(op)=>{L('selector abierto');try{const h=await o(op);L('carpeta elegida:',h.name);return h}catch(e){L('selector:',e);throw e}};}
else L('Este navegador no tiene showDirectoryPicker (usá Chrome o Edge de escritorio)');
let last='';setInterval(()=>{const t=(document.body&&document.body.innerText||'').match(/folder access[^\n]*|denied[^\n]*|not found[^\n]*|error[^\n]*/i);if(t&&t[0]!==last){last=t[0];L('pantalla:',t[0])}},1000);
})();
