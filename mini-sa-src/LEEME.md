# Mini-SA: prueba de recompilación estática Win32 x86 -> WebAssembly

Mini-SA es un ejecutable Win32 de 32 bits propio (no contiene código ni datos de Rockstar) que imita el arranque
de GTA San Andreas: ventana, Direct3D 9 con pipeline fijo, textura con LockRect, física x87 a 50 Hz con precisión
de 24 bits (fldcw), hilo de carga con CreateFileA/ReadFile, llamadas indirectas tipo vtable y switch con tabla de saltos.

Pipeline (rebuild.sh, ~65 s):
1. `minisa.c` -> `minisa.exe` con i686-w64-mingw32-gcc (sin CRT; importa solo d3d9, kernel32, user32).
2. `export_listings.py` genera los listados que recomp-kit normalmente toma de Ghidra, usando el conversor
   capstone->listado del propio translate.py y la tabla de símbolos del exe.
3. recomp-kit (MIT, https://github.com/veritr1x/recomp-kit) traduce el código x86 a C y lo compila con
   Emscripten 6.0.11 (WebGPU, pthreads, WasmFS). El sitio queda en /mini-sa/ con los datos en streaming.
4. `oracle.c`: proceso x86 de 32 bits que mapea la imagen del exe en 0x400000 y ejecuta sus bytes originales
   (init_cars, physics_step, state_hash) para comparar checksums con la versión WebAssembly.

Cambio local en recomp-kit: cmake/Dependencies.cmake apunta SDL3/glslang a clones git (codeload.github.com
bloqueado en el entorno) y desactiva X11/Wayland para las pruebas nativas. No afecta al código traducido.
