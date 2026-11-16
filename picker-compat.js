// Fase 1A — selector de carpeta compatible (no es parte de OpenSA).
// Chrome cancelaba showDirectoryPicker con AbortError. Este reemplazo usa el selector clásico <input webkitdirectory>:
// los archivos se leen localmente y de forma diferida (File.slice), nada se sube a ningún servidor.
(()=>{
  class FH{constructor(f){this.kind='file';this.name=f.name;this._f=f}async getFile(){return this._f}async queryPermission(){return'granted'}async requestPermission(){return'granted'}}
  class DH{constructor(n){this.kind='directory';this.name=n;this._c=new Map()}
    async *values(){for(const c of this._c.values())yield c}async *entries(){for(const e of this._c)yield e}async *keys(){for(const k of this._c.keys())yield k}
    async getDirectoryHandle(n){const c=this._c.get(n);if(!c||c.kind!=='directory')throw new DOMException(n,'NotFoundError');return c}
    async getFileHandle(n){const c=this._c.get(n);if(!c||c.kind!=='file')throw new DOMException(n,'NotFoundError');return c}
    async queryPermission(){return'granted'}async requestPermission(){return'granted'}}
  const pick=()=>new Promise((resolve,reject)=>{
    const inp=document.createElement('input');inp.type='file';inp.webkitdirectory=true;inp.multiple=true;inp.style.display='none';
    document.body.appendChild(inp);
    inp.addEventListener('change',()=>{const files=[...inp.files];inp.remove();
      if(!files.length)return reject(new DOMException('No se eligió ninguna carpeta','AbortError'));
      const rootName=files[0].webkitRelativePath.split('/')[0];const root=new DH(rootName);
      for(const f of files){const parts=f.webkitRelativePath.split('/').slice(1);let d=root;
        for(const p of parts.slice(0,-1)){if(!d._c.has(p))d._c.set(p,new DH(p));d=d._c.get(p)}
        d._c.set(parts[parts.length-1],new FH(f))}
      resolve(root)});
    inp.addEventListener('cancel',()=>{inp.remove();reject(new DOMException('Selección cancelada','AbortError'))});
    inp.click();});
  window.showDirectoryPicker=pick;
  // El manejador de carpeta simulado no se puede guardar en IndexedDB: se guarda vacío y en la próxima visita se vuelve a elegir.
  const put=IDBObjectStore.prototype.put;IDBObjectStore.prototype.put=function(v,k){try{return put.call(this,v,k)}catch(e){return put.call(this,null,k)}};
})();
