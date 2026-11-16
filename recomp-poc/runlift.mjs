import fs from 'node:fs';
for (const pc of [0,2]) { const {instance}=await WebAssembly.instantiate(fs.readFileSync(`lift_pc${pc}.wasm`));
  const N=1000000,t0=performance.now(),p=instance.exports.run(N),ms=performance.now()-t0;
  fs.writeFileSync(`lift_pc${pc}.bin`,Buffer.from(new Uint8Array(instance.exports.memory.buffer,p,N*16))); console.log(`lift PC${pc} wasm ms`,ms.toFixed(1)); }
