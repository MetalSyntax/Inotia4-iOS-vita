// Parches de este juego. Vacío hasta la primera corrida en consola: cada parche se agrega con
// evidencia (log/dump + desensamblado con tools/annotate.py) y su dirección del Mach-O.
#include <stdio.h>
#include <string.h>

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
// (60 = ~16 FPS, lo pone APPINFO_Initialize). Se elige en el menú de opciones (input.c) y se guarda en
// config.txt. Más FPS = el juego entero más rápido.
#define APPINFO_nTimer 0x3a6e50
static const int speed_fps[] = {16, 23, 25, 30};
static int speed_idx;
static int speed_ms(void) { return speed_idx ? 1000 / speed_fps[speed_idx] : 60; }

// config.txt: "fps N" y "lang en|es|pt".
static const char *const langs[] = {"en", "es", "pt"};
static int lang_idx;
static void config_save(void) {
    FILE *f = fopen(DATA_PATH "config.txt", "w");
    if (f) {
        fprintf(f, "fps %d\nlang %s\n", speed_fps[speed_idx], langs[lang_idx]);
        fclose(f);
    }
}
static int config_fps(void) {
    int fps = 0;
    char line[64], l[8];
    FILE *f = fopen(DATA_PATH "config.txt", "r");
    if (!f)
        return 0;
    while (fgets(line, sizeof(line), f))
        if (sscanf(line, "fps %d", &fps) == 1)
            ;
        else if (sscanf(line, "lang %7s", l) == 1)
            for (unsigned i = 0; i < sizeof(langs) / sizeof(langs[0]); i++)
                if (!strcmp(l, langs[i]))
                    lang_idx = i;
    fclose(f);
    return fps;
}

static void speed_set(int dir) {
    int n = sizeof(speed_fps) / sizeof(speed_fps[0]);
    speed_idx = (speed_idx + dir + n) % n;
    *(volatile uint16_t *)(APPINFO_nTimer + MACHO_SLIDE) = speed_ms();
    l_info("velocidad -> %d FPS (%d ms)", speed_fps[speed_idx], speed_ms());
}
static void speed_init(void) {
    int fps = config_fps();
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
    l_info("velocidad: %d FPS (menú START+SELECT)", speed_fps[speed_idx]);
}

// HUD de juego oculto (L+R, input.c/gles.c): GAMESTATE_DrawPlay llama a UIPlayPorting_Draw (cruceta, ataque,
// habilidades, retratos, HP). Se desvía esa llamada a un envoltorio que marca gles_hud_scope mientras dibuja; gles.c
// descarta lo que cae entero en PORT_HUD_ZONES. Trampolín: CSFB_grpDrawLine (renderizador por software de Com2uS, sin
// llamadas: el juego usa CSOGL).
#define UIPlayPorting_Draw 0x193a4
#define HUD_CALL 0x59b0
#define HUD_TRAMPOLINE 0x85a90
static void hud_draw(void) {
    extern int gles_hud_scope;
    gles_hud_scope = 1;
    call_guest((void *)(UIPlayPorting_Draw + MACHO_SLIDE + 1), 0, 0, 0, 0);
    gles_hud_scope = 0;
}
static uint32_t thumb_bl(uint32_t from, uint32_t to) { // BL T1: halfwords en orden de memoria
    int32_t off = (int32_t)(to - (from + 4));
    uint32_t s = off < 0, i1 = (off >> 23) & 1, i2 = (off >> 22) & 1, j1 = !i1 ^ s, j2 = !i2 ^ s;
    uint16_t hi = 0xf000 | s << 10 | ((off >> 12) & 0x3ff), lo = 0xd000 | j1 << 13 | j2 << 11 | ((off >> 1) & 0x7ff);
    return hi | (uint32_t)lo << 16;
}
static void hud_init(void) {
    uint32_t bl_orig = thumb_bl(HUD_CALL, UIPlayPorting_Draw), bl_new = thumb_bl(HUD_CALL, HUD_TRAMPOLINE);
    uint32_t cur = (macho_read_text(HUD_CALL) & 0xffff) | macho_read_text(HUD_CALL + 2) << 16;
    if (cur != bl_orig || (macho_read_text(HUD_TRAMPOLINE) & 0xffff) != 0xb5b0) {
        l_error("patch: llamada a UIPlayPorting_Draw no coincide (0x%08X/0x%08X): HUD no se oculta", cur, bl_orig);
        return;
    }
    macho_hook_addr(HUD_TRAMPOLINE | 1, hud_draw);
    macho_patch_text(HUD_CALL, &bl_new, 4); // oculto al empezar (gles_hud_hidden = 1, como Inotia 3); L+R lo muestra
}

// Idioma del texto (como Zenonia 2): español y portugués van en el lugar del inglés. GMAELOADER_LanguageLoad carga
// "memorytext_en.dat" para el idioma 0 (inglés); se cambian las dos letras del nombre en __cstring y el juego carga
// memorytext_es.dat / memorytext_pt.dat del bundle (tools/translate/: todo el texto, diálogo incluido). Se cambia en caliente desde el menú de opciones.
#define MEMORYTEXT_EN 0x2e346c // "memorytext_en.dat"
#define MEMORYTEXT_Create 0x47c78
#define GetConfigLanguage 0x3be70
static int lang_file_ok(int i) {
    char p[256];
    snprintf(p, sizeof(p), BUNDLE_PATH "memorytext_%s.dat", langs[i]);
    FILE *f = fopen(p, "rb");
    if (f)
        fclose(f);
    return f != NULL;
}
static void lang_write(int finalized) {
    char *name = (char *)(MEMORYTEXT_EN + MACHO_SLIDE);
    char cur[8];
    uint32_t w0 = macho_read_text(MEMORYTEXT_EN + 8), w1 = macho_read_text(MEMORYTEXT_EN + 12);
    memcpy(cur, &w0, 4), memcpy(cur + 4, &w1, 4); // "xt_en.da"
    cur[3] = langs[lang_idx][0], cur[4] = langs[lang_idx][1];
    memcpy(&w0, cur, 4), memcpy(&w1, cur + 4, 4);
    if (finalized) {
        macho_write_word((uint32_t *)(name + 8), w0);
        macho_write_word((uint32_t *)(name + 12), w1);
    } else {
        macho_patch_text(MEMORYTEXT_EN + 8, &w0, 4);
        macho_patch_text(MEMORYTEXT_EN + 12, &w1, 4);
    }
}
static void lang_init(void) {
    config_fps();
    char cur[20];
    for (int i = 0; i < 18; i += 4) {
        uint32_t w = macho_read_text(MEMORYTEXT_EN + i);
        memcpy(cur + i, &w, 4);
    }
    if (memcmp(cur, "memorytext_en.dat", 18)) {
        l_error("patch: nombre de memorytext_en.dat no coincide; idioma fijo en inglés");
        lang_idx = -1;
        return;
    }
    if (lang_idx && !lang_file_ok(lang_idx)) {
        l_error("idioma %s: falta memorytext_%s.dat en el bundle, queda en inglés", langs[lang_idx], langs[lang_idx]);
        lang_idx = 0;
    }
    if (lang_idx)
        lang_write(0);
    l_info("idioma del texto: %s (menú START+SELECT)", langs[lang_idx]);
}
// Menú de opciones (input.c, START+SELECT): velocidad e idioma. El idioma se aplica al cerrar el menú (recargar los
// textos cada vez que se pasa por un valor dejaría un búfer sin liberar por paso).
static int lang_menu = -1; // elegido en el menú, todavía sin aplicar
int port_menu_lang(void) { return lang_menu >= 0 ? lang_menu : lang_idx > 0 ? lang_idx : 0; }
int port_menu_rows(void) { return lang_idx < 0 ? 1 : 2; }
const char *port_menu_label(int row, int l) {
    static const char *const t[][3] = {{"Game speed", "Velocidad del juego", "Velocidade do jogo"},
                                       {"Text language", "Idioma del texto", "Idioma do texto"}};
    return t[row][l];
}
const char *port_menu_value(int row, int l) {
    static char v[48];
    static const char *const orig[] = {"original", "original", "original"};
    static const char *const shown[] = {"English", "Español", "Português"};
    if (row == 0) {
        if (speed_idx)
            snprintf(v, sizeof(v), "%d FPS", speed_fps[speed_idx]);
        else
            snprintf(v, sizeof(v), "%d FPS (%s)", speed_fps[0], orig[l]);
        return v;
    }
    return shown[port_menu_lang()];
}
void port_menu_change(int row, int dir) {
    if (row == 0) {
        speed_set(dir);
        return;
    }
    int n = sizeof(langs) / sizeof(langs[0]), k = port_menu_lang();
    do
        k = (k + dir + n) % n;
    while (k && !lang_file_ok(k));
    lang_menu = k;
}
void port_menu_closed(void) {
    if (lang_menu >= 0 && lang_menu != lang_idx && lang_idx >= 0) {
        lang_idx = lang_menu;
        lang_write(1);
        // Solo si el juego está en inglés (el lugar que se reemplaza). El búfer anterior queda sin liberar: puede haber
        // punteros vivos a sus textos (ponytail: ~600 KB por cambio de idioma).
        if (call_guest((void *)(GetConfigLanguage + MACHO_SLIDE + 1), 0, 0, 0, 0) == 0)
            call_guest((void *)(MEMORYTEXT_Create + MACHO_SLIDE + 1), MEMORYTEXT_EN + MACHO_SLIDE, 0, 0, 0);
        l_info("idioma -> %s", langs[lang_idx]);
    }
    lang_menu = -1;
    config_save();
}

void patch_init(void) {
    speed_init();
    lang_init();
    hud_init();
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
