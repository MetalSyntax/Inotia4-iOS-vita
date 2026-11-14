#pragma once
// Configuración propia de este juego (el resto de source/ es común a los ports iOS; se sincroniza
// con tools/sync_runtime.sh desde Zenonia5-iOS-vita).
#define PORT_NAME "Inotia 4"
#define PORT_SLUG "inotia4ios"
#define PORT_EXEC "Inotia4_global"       // CFBundleExecutable (fat armv6+armv7: se carga el slice armv7)
#define PORT_ROTATION 0                  // Info.plist: LandscapeRight/Left; confirmar en consola
#define PORT_STRETCH 0
#define PORT_SYSTEM_VERSION "4.3.5"      // MinimumOSVersion 4.3
#define PORT_DEVICE_MACHINE "iPhone3,1"  // iPhone 4: Default@2x 640x960
#define GLES_POOL_MB 32
#define PORT_ACCEL_FROM_STICK 0
#define PORT_SCREEN_W 320
#define PORT_SCREEN_H 480
#define PORT_SCREEN_SCALE 2.0f
// ponytail: igual que Zenonia 5 (mismo publisher); confirmar el offset de st_size con annotate.py en un caller de _stat.
#define PORT_STAT_INO64 1
// HUD de juego oculto con L+R (gles.c, patch.c): cruceta abajo a la izquierda; ataque, habilidades y poción abajo a la
// derecha. Normalizadas 0..1 sobre lo que se ve (480x320), y hacia abajo. Medido en la captura ingame.
#define PORT_HUD_ZONES {{-0.05f, 0.6f, 0.35f, 1.05f}, {0.5f, 0.57f, 1.05f, 1.05f}} // log 036: draws hasta x=1.00x
#define PORT_GL_DIAG 0 // sin capturas TGA ni trazas de frame: escribir a ux0 traba el hilo principal
