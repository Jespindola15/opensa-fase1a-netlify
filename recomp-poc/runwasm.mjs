import fs from 'node:fs';
const {instance}=await WebAssembly.instantiate(fs.readFileSync('fn.wasm'));
const N=1000000, t0=performance.now(); const p=instance.exports.run(N); const ms=performance.now()-t0;
fs.writeFileSync('wasm.bin',Buffer.from(new Uint8Array(instance.exports.memory.buffer,p,N*16)));
console.log('wasm run ms',ms.toFixed(1));
