// Touch frontal + botones de la Vita -> toques de UIKit (uikit.c) en coordenadas de la vista.
//
// La pantalla muestra el drawable girado/escalado (gles_present); acá se hace el mapeo inverso:
// punto de pantalla -> normalizado en el rectángulo destino -> coordenadas de la vista de iOS.
// Botones: cada uno "toca" un punto configurable en ux0:data/<slug>/controls.txt, en coordenadas
// lógicas de lo que se ve (p. ej. 480x320 para un juego apaisado):
//     CROSS 420 260
//     RTRIGGER 440 160
// Se edita por FTP sin recompilar (texto opcional al final de la línea: nombre de la acción). START+SELECT corto abre
// el menú de opciones (reasignar, controles en pantalla, opciones del port); sostenido 2 s cambia la rotación.
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/touch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vitaGL.h>

#include "gles.h"
#include "hle.h"
#include "input.h"
#include "log.h"

#define FINGERS 6
#define BTN_FINGER0 2 // los botones usan dedos virtuales 2..7 (los reales, 0..1)
extern int port_rotation;

static struct {
    int id, down;
} fingers[2];
static uint32_t prev_buttons;
static float stick_x, stick_y;
static uint64_t combo_since;

static const struct {
    const char *name;
    uint32_t mask;
} names[] = {
    {"CROSS", SCE_CTRL_CROSS},       {"CIRCLE", SCE_CTRL_CIRCLE}, {"SQUARE", SCE_CTRL_SQUARE},
    {"TRIANGLE", SCE_CTRL_TRIANGLE}, {"LTRIGGER", SCE_CTRL_LTRIGGER}, {"RTRIGGER", SCE_CTRL_RTRIGGER},
    {"START", SCE_CTRL_START},       {"SELECT", SCE_CTRL_SELECT}, {"UP", SCE_CTRL_UP},
    {"DOWN", SCE_CTRL_DOWN},         {"LEFT", SCE_CTRL_LEFT},     {"RIGHT", SCE_CTRL_RIGHT},
};
#define MAX_BIND 10
static struct {
    uint32_t mask;
    float x, y;     // lógicas (lo que se ve)
    char label[40]; // nombre de la acción (reasignación)
} binds[MAX_BIND];
static int nbinds;
// STICK x y r: stick izquierdo / cruceta = dedo virtual arrastrado alrededor de (x,y) lógicos (pad táctil).
static float pad_x, pad_y, pad_r;
static int pad_down;
#define PAD_FINGER (BTN_FINGER0 + MAX_BIND)

static void load_controls(void) {
    FILE *f = fopen(DATA_PATH "controls.txt", "r");
    if (!f) {
        l_info("sin controls.txt: solo touch + stick (acelerómetro)");
        return;
    }
    char line[128], n[32];
    float x, y, r;
    int end;
    while (fgets(line, sizeof(line), f))
        if (sscanf(line, "STICK %f %f %f", &x, &y, &r) == 3)
            pad_x = x, pad_y = y, pad_r = r;
        else if (line[0] != '#' && nbinds < MAX_BIND && sscanf(line, "%31s %f %f%n", n, &x, &y, &end) == 3)
            for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
                if (!strcmp(n, names[i].name)) {
                    binds[nbinds] = (typeof(binds[0])){names[i].mask, x, y, ""};
                    sscanf(line + end, " %39[^\r\n]", binds[nbinds].label);
                    nbinds++;
                }
    fclose(f);
    l_info("controls.txt: %d botones%s", nbinds, pad_r > 0 ? " + STICK" : "");
}

static const char *mask_name(uint32_t m) {
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (names[i].mask == m)
            return names[i].name;
    return "?";
}

static void save_controls(void) {
    FILE *f = fopen(DATA_PATH "controls.txt", "w");
    if (!f)
        return;
    fprintf(f, "# Botón -> toque en coordenadas de lo que se ve (origen arriba a la izquierda) y nombre de la acción.\n"
               "# Escrito por la reasignación (L+R+SELECT); se puede editar por FTP.\n");
    if (pad_r > 0)
        fprintf(f, "STICK %g %g %g\n", pad_x, pad_y, pad_r);
    for (int i = 0; i < nbinds; i++)
        fprintf(f, "%s %g %g %s\n", mask_name(binds[i].mask), binds[i].x, binds[i].y, binds[i].label);
    fclose(f);
}
// ---------------------------------------------------------------- menú de opciones (START+SELECT)
// Como Zenonia 2: el juego queda en pausa (este bucle no vuelve al run loop) y se dibuja encima del último frame
// (gles_overlay_tex en gles_present). Filas: un botón por acción de controls.txt, controles táctiles y las opciones
// del port (port_menu_*: velocidad, idioma...). Arriba/abajo elige, X cambia (en una acción: apretar el botón nuevo;
// si ya se usa se intercambian), izquierda/derecha cambian el valor, O guarda y cierra.
#define MW 960
#define MH 544
extern int ct_text_rgba(uint32_t *px, int w, int h, int x, int y, float size, uint32_t abgr, const char *text);
extern GLuint gles_overlay_tex;
extern int gles_hud_hidden;
extern int port_menu_rows(void) __attribute__((weak));
extern const char *port_menu_label(int row, int lang) __attribute__((weak));
extern const char *port_menu_value(int row, int lang) __attribute__((weak));
extern void port_menu_change(int row, int dir) __attribute__((weak));
extern void port_menu_closed(void) __attribute__((weak));
extern int port_menu_lang(void) __attribute__((weak)); // 0 inglés, 1 español, 2 portugués
static const char *const ui_txt[][3] = {
    {"OPTIONS", "OPCIONES", "OPÇÕES"},
    {"On-screen controls (L+R)", "Controles en pantalla (L+R)", "Controles na tela (L+R)"},
    {"SHOWN", "VISIBLES", "VISÍVEIS"},
    {"HIDDEN", "OCULTOS", "OCULTOS"},
    {"Save and close", "Guardar y salir", "Salvar e sair"},
    {"Press the new button...  (START: cancel)", "Presiona el botón nuevo...  (START: cancelar)",
     "Pressione o novo botão...  (START: cancelar)"},
    {"Up/Down: choose   X: change   Left/Right: value   O: save and close",
     "Arriba/Abajo: elegir   X: cambiar   Izq./Der.: valor   O: guardar y salir",
     "Cima/Baixo: escolher   X: alterar   Esq./Dir.: valor   O: salvar e sair"},
    {"Buttons", "Botones", "Botões"},
};
enum { T_TITLE, T_HUD, T_SHOWN, T_HIDDEN, T_CLOSE, T_PRESS, T_HELP, T_BUTTONS };
static uint32_t *menu_px;
static GLuint menu_tex;

static int menu_lang(void) {
    int l = port_menu_lang ? port_menu_lang() : 0;
    return l >= 0 && l < 3 ? l : 0;
}
static void menu_rect(int x, int y, int w, int h, uint32_t c) {
    for (int j = y; j < y + h && j < MH; j++)
        for (int i = x; i < x + w && i < MW; i++)
            menu_px[j * MW + i] = c;
}
static void menu_draw(int cur, int rows, int capture) {
    const int L = menu_lang(), top = 64, rh = rows > 14 ? 28 : 32;
    menu_rect(0, 0, MW, MH, 0xd8100804);      // fondo oscuro translúcido (ABGR)
    menu_rect(40, 20, MW - 80, 2, 0xff40a0e0);
    ct_text_rgba(menu_px, MW, MH, 48, 52, 26, 0xff60c8ff, ui_txt[T_TITLE][L]);
    char val[64];
    for (int r = 0; r < rows; r++) {
        int y = top + r * rh;
        if (r == cur)
            menu_rect(40, y, MW - 80, rh - 2, 0xff704020);
        const char *label, *value = NULL;
        if (r < nbinds) {
            label = binds[r].label[0] ? binds[r].label : mask_name(binds[r].mask);
            value = r == cur && capture ? ui_txt[T_PRESS][L] : mask_name(binds[r].mask);
        } else if (r == nbinds) {
            label = ui_txt[T_HUD][L], value = ui_txt[gles_hud_hidden ? T_HIDDEN : T_SHOWN][L];
        } else if (r < rows - 1) {
            label = port_menu_label(r - nbinds - 1, L);
            value = port_menu_value(r - nbinds - 1, L);
            if (value) {
                snprintf(val, sizeof(val), "<  %s  >", value);
                value = val;
            }
        } else {
            label = ui_txt[T_CLOSE][L];
        }
        ct_text_rgba(menu_px, MW, MH, 64, y + rh - 10, 20, 0xffe8e8e8, label ? label : "");
        if (value)
            ct_text_rgba(menu_px, MW, MH, 560, y + rh - 10, 20, r == cur ? 0xff80ffff : 0xffc0c0c0, value);
    }
    ct_text_rgba(menu_px, MW, MH, 48, MH - 18, 17, 0xffa0a0a0, ui_txt[T_HELP][L]);
    glBindTexture(GL_TEXTURE_2D, menu_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MW, MH, 0, GL_RGBA, GL_UNSIGNED_BYTE, menu_px);
}
static uint32_t pad_buttons(void) {
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(0, &pad, 1);
    uint32_t b = pad.buttons;
    if (pad.ly < 40) b |= SCE_CTRL_UP;
    if (pad.ly > 215) b |= SCE_CTRL_DOWN;
    if (pad.lx < 40) b |= SCE_CTRL_LEFT;
    if (pad.lx > 215) b |= SCE_CTRL_RIGHT;
    return b;
}
static void options_menu(void) {
    for (int i = 0; i < nbinds; i++) // suelta los dedos virtuales de los botones apretados
        if (prev_buttons & binds[i].mask)
            ui_touch_event(BTN_FINGER0 + i, 3, -1, -1);
    if (pad_down)
        ui_touch_event(PAD_FINGER, 3, -1, -1), pad_down = 0;
    if (!menu_px && !(menu_px = malloc(MW * MH * 4)))
        return;
    if (!menu_tex) {
        glGenTextures(1, &menu_tex);
        glBindTexture(GL_TEXTURE_2D, menu_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    GLint tex;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
    int extra = port_menu_rows ? port_menu_rows() : 0, rows = nbinds + 1 + extra + 1, cur = 0, capture = 0, dirty = 1;
    uint32_t prev = pad_buttons(); // START+SELECT siguen apretados: no cuentan
    l_info("menú de opciones abierto");
    for (;;) {
        uint32_t b = pad_buttons(), press = b & ~prev;
        prev = b;
        if (capture && press) {
            if (!(press & SCE_CTRL_START))
                for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
                    if (press & names[i].mask) {
                        for (int j = 0; j < nbinds; j++)
                            if (binds[j].mask == names[i].mask)
                                binds[j].mask = binds[cur].mask; // ya usado: se intercambian
                        binds[cur].mask = names[i].mask;
                        break;
                    }
            capture = 0, dirty = 1;
        } else if (press) {
            dirty = 1;
            if (press & SCE_CTRL_UP)
                cur = (cur + rows - 1) % rows;
            if (press & SCE_CTRL_DOWN)
                cur = (cur + 1) % rows;
            int dir = press & SCE_CTRL_LEFT ? -1 : press & (SCE_CTRL_RIGHT | SCE_CTRL_CROSS) ? 1 : 0;
            if (press & SCE_CTRL_CIRCLE || (press & SCE_CTRL_CROSS && cur == rows - 1))
                break;
            if (cur < nbinds && press & SCE_CTRL_CROSS)
                capture = 1;
            else if (cur == nbinds && dir)
                gles_hud_hidden ^= 1;
            else if (cur > nbinds && cur < rows - 1 && dir)
                port_menu_change(cur - nbinds - 1, dir);
        }
        if (dirty)
            menu_draw(cur, rows, capture), dirty = 0;
        gles_overlay_tex = menu_tex;
        gles_present(); // último frame del juego + menú, espera el vblank
        extern volatile unsigned darwin_io_ops;
        darwin_io_ops++; // progreso para el watchdog de uikit.c (el juego no presenta mientras el menú está abierto)
    }
    gles_overlay_tex = 0;
    glBindTexture(GL_TEXTURE_2D, tex);
    save_controls();
    if (port_menu_closed)
        port_menu_closed();
    prev_buttons = pad_buttons() & ~(SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT); // sin toques fantasma
    l_info("menú de opciones cerrado");
}

void input_init(void) {
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    FILE *f = fopen(DATA_PATH "rotation.txt", "r");
    if (f) {
        int r;
        if (fscanf(f, "%d", &r) == 1 && r >= 0 && r <= 2)
            port_rotation = r;
        fclose(f);
    }
    load_controls();
}

void input_stick(float *x, float *y) {
    *x = stick_x;
    *y = stick_y;
}

// Normalizado [0,1] en lo que se ve -> coordenadas de la vista (ver uv_rot en gles.c).
static void to_view(float nx, float ny, float *vx, float *vy) {
    switch (gles_rotation()) {
    case 1:
        *vx = (1 - ny) * fb_w, *vy = nx * fb_h;
        break;
    case 2:
        *vx = ny * fb_w, *vy = (1 - nx) * fb_h;
        break;
    default:
        *vx = nx * fb_w, *vy = ny * fb_h;
    }
}

static int screen_to_view(int sx, int sy, float *vx, float *vy) {
    int dx, dy, dw, dh;
    gles_dest_rect(&dx, &dy, &dw, &dh);
    float nx = (sx - dx) / (float)dw, ny = (sy - dy) / (float)dh;
    if (nx < 0 || nx > 1 || ny < 0 || ny > 1)
        return 0;
    to_view(nx, ny, vx, vy);
    return 1;
}

static void logical_size(float *w, float *h) {
    *w = gles_rotation() ? fb_h : fb_w;
    *h = gles_rotation() ? fb_w : fb_h;
}

void input_poll(void) {
    SceTouchData t;
    if (sceTouchPeek(SCE_TOUCH_PORT_FRONT, &t, 1) < 1)
        t.reportNum = 0;
    // dos dedos como máximo (los juegos de 2009 rara vez usan más)
    for (int k = 0; k < 2; k++) {
        int found = -1;
        for (int i = 0; i < (int)t.reportNum; i++)
            if (fingers[k].down && t.report[i].id == fingers[k].id)
                found = i;
        if (fingers[k].down && found < 0) {
            fingers[k].down = 0;
            ui_touch_event(k, 3, -1, -1);
        }
    }
    for (int i = 0; i < (int)t.reportNum && i < 2; i++) {
        float vx, vy;
        if (!screen_to_view(t.report[i].x / 2, t.report[i].y / 2, &vx, &vy))
            continue;
        int k = -1;
        for (int j = 0; j < 2; j++)
            if (fingers[j].down && fingers[j].id == t.report[i].id)
                k = j;
        if (k < 0) {
            for (int j = 0; j < 2 && k < 0; j++)
                if (!fingers[j].down)
                    k = j;
            if (k < 0)
                continue;
            fingers[k].down = 1;
            fingers[k].id = t.report[i].id;
            ui_touch_event(k, 0, vx, vy);
        } else {
            ui_touch_event(k, 1, vx, vy);
        }
    }

    SceCtrlData pad;
    sceCtrlPeekBufferPositive(0, &pad, 1);
    stick_x = (pad.lx - 128) / 127.0f;
    stick_y = (pad.ly - 128) / 127.0f;
    if (stick_x > -0.12f && stick_x < 0.12f)
        stick_x = 0;
    if (stick_y > -0.12f && stick_y < 0.12f)
        stick_y = 0;
    // L+R juntos: muestra/oculta los controles táctiles (gles.c). Mientras están juntos, L y R no tocan nada.
    extern int gles_hud_hidden;
    const uint32_t LR = SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER;
    static int lr_held;
    const uint32_t SS = SCE_CTRL_SELECT | SCE_CTRL_START;
    if ((pad.buttons & LR) == LR && !lr_held && !(pad.buttons & SS)) {
        gles_hud_hidden ^= 1;
        l_info("controles táctiles %s (L+R)", gles_hud_hidden ? "ocultos" : "visibles");
    }
    lr_held = (pad.buttons & LR) == LR || (lr_held && (pad.buttons & LR)); // hasta soltar los dos
    uint32_t b = lr_held ? pad.buttons & ~LR : pad.buttons, changed = b ^ prev_buttons;
    // START y SELECT tocan al soltarlos, y solo si no formaron parte de START+SELECT (menú): el combo no toca la
    // poción ni las opciones del juego. El toque dura un frame (se suelta en la próxima pasada).
    static uint32_t ss_combo;
    static int ss_up = -1;
    if (ss_up >= 0)
        ui_touch_event(BTN_FINGER0 + ss_up, 3, -1, -1), ss_up = -1;
    if ((b & SS) == SS)
        ss_combo = SS;
    for (int i = 0; i < nbinds; i++) {
        if (!(changed & binds[i].mask))
            continue;
        if (binds[i].mask & SS) {
            if (!(b & binds[i].mask) && !(ss_combo & binds[i].mask)) {
                float lw, lh, vx, vy;
                logical_size(&lw, &lh);
                to_view(binds[i].x / lw, binds[i].y / lh, &vx, &vy);
                ui_touch_event(BTN_FINGER0 + i, 0, vx, vy);
                ss_up = i;
            }
            continue;
        }
        float lw, lh, vx, vy;
        logical_size(&lw, &lh);
        to_view(binds[i].x / lw, binds[i].y / lh, &vx, &vy);
        ui_touch_event(BTN_FINGER0 + i, (b & binds[i].mask) ? 0 : 3, vx, vy);
    }
    if (pad_r > 0) {
        float px = stick_x, py = stick_y;
        int dx = !!(b & SCE_CTRL_RIGHT) - !!(b & SCE_CTRL_LEFT), dy = !!(b & SCE_CTRL_DOWN) - !!(b & SCE_CTRL_UP);
        if (px * px + py * py < 0.35f * 0.35f) // zona muerta grande: un dedo fantasma bloquea el toque real
            px = py = 0;
        if (dx || dy)
            px = dx * (dy ? 0.707f : 1), py = dy * (dx ? 0.707f : 1);
        int on = px != 0 || py != 0;
        if (on || pad_down) {
            float lw, lh, vx, vy;
            logical_size(&lw, &lh);
            to_view((pad_x + px * pad_r) / lw, (pad_y + py * pad_r) / lh, &vx, &vy);
            ui_touch_event(PAD_FINGER, on ? (pad_down ? 1 : 0) : 3, vx, vy);
            pad_down = on;
        }
    }
    // SELECT+START sostenidos 2 s: rotación siguiente (se guarda).
    uint64_t now = sceKernelGetProcessTimeWide();
    if ((b & (SCE_CTRL_SELECT | SCE_CTRL_START)) == (SCE_CTRL_SELECT | SCE_CTRL_START)) {
        if (!combo_since)
            combo_since = now;
        else if (now - combo_since > 2000000) {
            port_rotation = (port_rotation + 1) % 3;
            combo_since = now + 60000000; // una vez por pulsación
            FILE *f = fopen(DATA_PATH "rotation.txt", "w");
            if (f) {
                fprintf(f, "%d\n", port_rotation);
                fclose(f);
            }
            l_info("rotación -> %d", port_rotation);
        }
    } else {
        // pulsación corta (soltada antes de los 2 s de la rotación): menú de opciones
        int open = combo_since && combo_since <= now;
        combo_since = 0;
        prev_buttons = b;
        if (!(b & SS))
            ss_combo = 0;
        if (open)
            options_menu();
        return;
    }
    prev_buttons = b;
}
