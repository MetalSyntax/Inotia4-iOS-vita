// Parches de este juego. Vacío hasta la primera corrida en consola: cada parche se agrega con
// evidencia (log/dump + desensamblado con tools/annotate.py) y su dirección del Mach-O.
#include <stdio.h>

#include "hle.h"
#include "imports.h"
#include "log.h"
#include "macho.h"
#include "patch.h"

const import_entry patch_imports[] = {
    {"", 0, 0},
};
TABLE_END(patch)

void patch_objc(void) {}

// CS_fsOpen/FILE_Open, CS_knlGetResourceID y RES_LoadToHeap devuelven punteros (NSFileHandle*, NSString*,
// buffer) o un error negativo chico, y los llamadores lo prueban con "cmp rX,#0; blt" (o "it lt"): en iOS el
// heap está debajo de 0x80000000, en la Vita (0x81xxxxxx..) el puntero da negativo y la carga "falla"
// (font.fnt -> FONT_pFontData NULL -> crash en GRP_PutAsciiChar; FILE_Open -> instalación fallida).
// Se cambia por "cmn rX,rX; bmi": 2*rX es negativo para -1..-0x40000000 (errores) y positivo para
// 0x80000000..0xBFFFFFFF (punteros de la Vita). Lista: los cmp tras cada llamada a esas funciones
// (barrido del desensamblado: bl <func> y en las instrucciones siguientes cmp rX,#0 + condición lt).
// 0x37830: EXCELDATA_OpenFile (eventdata.dat) -> sin él EVTSYSTEM_LoadEvent falla y el juego no arranca.
static const uint32_t signed_ptr_checks[] = {
    0x7d40,  0xaee0,  0xaf2a,  0xaf92,  0xb046,  0x244d8, 0x24618, 0x3a45a, 0x3a500, 0x3a5fe, 0x3a74e, 0x3a83a,
    0x37830, 0x3ab7a, 0x42f8a, 0x42fea, 0x43178, 0x43272, 0x50b16, 0x50b68, 0x50d08, 0x50e24, 0x5b804, 0x5bde8,
};

static int patch_signed_check(uint32_t vm) {
    uint16_t h[5];
    for (int i = 0; i < 5; i++)
        h[i] = macho_read_text(vm + 2 * i) & 0xffff;
    int n; // índice del halfword con la condición
    if ((h[0] & 0xf8ff) == 0x2800) { // cmp rR,#0 (T1)
        unsigned r = (h[0] >> 8) & 7;
        h[0] = 0x42c0 | r << 3 | r; // cmn rR,rR
        n = 1;
    } else if ((h[0] & 0xfff0) == 0xf1b0 && h[1] == 0x0f00) { // cmp.w rR,#0
        unsigned r = h[0] & 15;
        h[0] = 0xeb10 | r, h[1] = 0x0f00 | r; // cmn.w rR,rR
        n = 2;
    } else {
        return -1;
    }
    if ((h[n] & 0xff00) == 0x4600) // mov rD,rM (no toca flags) entre el cmp y el salto (EVTSYSTEM_LoadEvent)
        n++;
    uint16_t c = h[n];
    if ((c & 0xff00) == 0xdb00) // blt (T1) -> bmi
        h[n] = (c & 0xf0ff) | 0x0400;
    else if (c == 0xbfb8) // it lt (una instrucción) -> it mi
        h[n] = 0xbf48;
    else if ((c & 0xfbc0) == 0xf2c0 && (h[n + 1] & 0xd000) == 0x8000) // blt.w (T3) -> bmi.w
        h[n] = (c & ~0x03c0) | 0x0100;
    else
        return -1;
    return macho_patch_text(vm, h, 2 * (n + 1));
}

// Velocidad (como Inotia 3): la lógica va atada al frame, MainProcess rearma el timer cada APPINFO_nTimer ms
// (60 = ~16 FPS, lo pone APPINFO_Initialize). START+SELECT (pulsación corta, input.c) la cicla y se guarda en
// config.txt. Más FPS = el juego entero más rápido.
#define APPINFO_nTimer 0x3a6e50
static const int speed_fps[] = {16, 23, 25, 30};
static int speed_idx;
static int speed_ms(void) { return speed_idx ? 1000 / speed_fps[speed_idx] : 60; }
void port_speed_cycle(void) {
    speed_idx = (speed_idx + 1) % (int)(sizeof(speed_fps) / sizeof(speed_fps[0]));
    *(volatile uint16_t *)(APPINFO_nTimer + MACHO_SLIDE) = speed_ms();
    FILE *f = fopen(DATA_PATH "config.txt", "w");
    if (f) {
        fprintf(f, "fps %d\n", speed_fps[speed_idx]);
        fclose(f);
    }
    l_info("velocidad -> %d FPS (%d ms)", speed_fps[speed_idx], speed_ms());
}
static void speed_init(void) {
    int fps = 0;
    FILE *f = fopen(DATA_PATH "config.txt", "r");
    if (f) {
        if (fscanf(f, "fps %d", &fps) != 1)
            fps = 0;
        fclose(f);
    }
    for (unsigned i = 0; i < sizeof(speed_fps) / sizeof(speed_fps[0]); i++)
        if (speed_fps[i] == fps)
            speed_idx = i;
    // APPINFO_Initialize: movs r1,#60 -> movs r1,#<ms>
    uint16_t mov = 0x2100 | speed_ms();
    if ((macho_read_text(0x243dc) & 0xffff) == 0x213c)
        macho_patch_text(0x243dc, &mov, 2);
    else
        l_error("patch: APPINFO_Initialize nTimer no coincide");
    // MainProcess espera al menos 10 ms aunque el frame ya venga atrasado: con 30 FPS y ~30 ms de juego por
    // frame eso lo dejaba en ~24. Mínimo 2 ms (deja correr input y el timer de FMOD).
    static const struct { uint32_t vm; uint16_t orig, rep; } minwait[] = {{0x21f4c, 0x290a, 0x2902}, {0x21f54, 0x210a, 0x2102}};
    for (unsigned i = 0; i < 2; i++)
        if ((macho_read_text(minwait[i].vm) & 0xffff) == minwait[i].orig)
            macho_patch_text(minwait[i].vm, &minwait[i].rep, 2);
        else
            l_error("patch: espera mínima de MainProcess en 0x%X no coincide", minwait[i].vm);
    l_info("velocidad: %d FPS (START+SELECT para cambiar)", speed_fps[speed_idx]);
}

void patch_init(void) {
    speed_init();
    // Términos de uso (STATE_ProcessAgree): sin texto y el OK no responde. GMAELOADER_CheckAgree devuelve 1 si
    // existe Documents/agree.dat (ya aceptados) -> STATE_ProcessInit salta al estado 5 en vez del 3 (Agree).
    static const uint16_t ret1[] = {0x2001, 0x4770}; // movs r0,#1; bx lr
    if ((macho_read_text(0x3aa90) & 0xffff) == 0xb580) // push {r7,lr}
        macho_patch_text(0x3aa90, ret1, sizeof(ret1));
    else
        l_error("patch: GMAELOADER_CheckAgree no coincide");

    // -[EAGLView touches*]: multiplican el punto por _scaleFactor (= [UIScreen scale] = 2), pero el resto
    // (SGL_ChangeOrientationPosition: x*960/480) espera el punto en el espacio del dispositivo 480x320 que
    // ya da la vista; con x2 el toque cae en (764,1104) y ningún botón responde. Se quita la multiplicación.
    static const struct { uint32_t vm; uint16_t orig, rep; } no_scale[] = {
        {0x8e1ba, 0x4341, 0xbf00}, {0x8e1c8, 0x4358, 0x4618}, // Began: muls r1,r0,r1 -> nop; muls r0,r3,r0 -> mov r0,r3
        {0x8e43a, 0x4341, 0xbf00}, {0x8e448, 0x4358, 0x4618}, // Ended: igual
        {0x8e78a, 0x4351, 0xbf00}, {0x8e78c, 0x4342, 0x4602}, // Moved: muls r2,r0,r2 -> mov r2,r0
        {0x8e8e8, 0x4351, 0xbf00}, {0x8e8ea, 0x4350, 0xbf00}, // Moved (2º toque)
    };
    for (unsigned i = 0; i < sizeof(no_scale) / sizeof(no_scale[0]); i++)
        if ((macho_read_text(no_scale[i].vm) & 0xffff) == no_scale[i].orig)
            macho_patch_text(no_scale[i].vm, &no_scale[i].rep, 2);
        else
            l_error("patch: escala de toque en 0x%X no coincide", no_scale[i].vm);

    for (unsigned i = 0; i < sizeof(signed_ptr_checks) / sizeof(signed_ptr_checks[0]); i++)
        if (patch_signed_check(signed_ptr_checks[i]))
            l_error("patch: check con signo en 0x%X no coincide (0x%08X)", signed_ptr_checks[i],
                    macho_read_text(signed_ptr_checks[i]));
}
