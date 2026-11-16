/* Cuerpo copiado literalmente de gta-reversed: CVector::NormaliseAndMag (0x59C970). Sustituto del código
   original (cifrado por SecuROM) para validar el arnés y medir la semántica x87 vs WebAssembly. */
typedef struct { float x, y, z; } CVector;
__attribute__((noinline)) float NormaliseAndMag(CVector* v) {
    const float fDot = v->x * v->x + v->y * v->y + v->z * v->z;
    if (fDot <= 0.0F) { v->x = 1.0F; return 1.0F; }
    const float fRecip = 1.0F / __builtin_sqrtf(fDot);
    v->x *= fRecip; v->y *= fRecip; v->z *= fRecip;
    return 1.0F / fRecip;
}
