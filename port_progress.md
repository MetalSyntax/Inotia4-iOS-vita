# Progreso — Inotia 4 iOS

## 2026-11-13 — inicio

- Repo creado desde `IPA/Inotia4_global.ipa`. `psvita-toolkit init --ipa` dejó un scaffold de Android
  (FalsoJNI/soloader) y lo reemplacé por el runtime iOS de `../Zenonia5-iOS-vita` (`sync_runtime.sh`).
- Build OK (`psvita-toolkit build --preset release`): `build/eboot.bin`, `build/inotia4ios.vpk`.
- `ux0_data/inotia4ios/` armado con `tools/stage_data.sh` (sin `nib.txt`: NIBArchive no soportado).
- Imports sin cubrir: 143 (`docs/imports_sin_cubrir.txt`).

## PENDIENTE EN CONSOLA

- 2026-11-13 (logs 001-018, ciclo automático con vitacompanion): llega a la pantalla del logo con botón
  "OK" (aviso previo al título), estable, 270 frames sin crash. **Desde acá, prueba manual.**
- Causas resueltas, en orden:
  1. `MyEAGLView initWithCoder:` nil: `AudioComponentFindNext` sin enlazar -> `coreaudio.c` (de Inotia 2)
     en CMakeLists e `imports.[ch]`.
  2. Lectura de recursos: NSFileHandle, `-[NSDictionary fileSize]`, `+[NSBundle
     preferredLocalizationsFromArray:forPreferences:]`, `-[NSBundle
     pathForResource:ofType:inDirectory:forLocalization:]`, `-[NSData getBytes:]` (CS_fsRead),
     `-[NSString compare:options:]`, `+[NSArray arrayWithContentsOfFile:]` (foundation.c).
  3. Bucle del juego: CFRunLoopTimer (Create/Add/Remove/Invalidate/SetNextFireDate) sobre NSTimer.
     `CS_knlSetTimer` lo rearma con SetNextFireDate; intervalo 0.001 s repetitivo.
  4. **Punteros de la Vita >= 0x80000000**: `FILE_Open`/`CS_fsOpen`/`RES_LoadToHeap`/
     `CS_knlGetResourceID` devuelven punteros y el juego los prueba con `cmp rX,#0; blt`. patch.c cambia
     23 sitios a `cmn rX,rX; bmi` (lista sacada barriendo `bl <func>` + cmp/lt). Si aparece otra carga
     que "falla" sin motivo, buscar el mismo patrón tras otras funciones que devuelvan punteros.
  5. `sysctl({CTL_HW, HW_PAGESIZE})` (darwin.c) para `CS_knlGetFreeMemory`.
  6. coretext.c: font.ttf en RAM + caché de avances (TEXTCTRL2 mide prefijo por prefijo: segundos/frame).
  7. `CGBitmapContextGetWidth/Height/BytesPerRow` (uikit.c).
- **Cambios en archivos comunes** (los pisa `sync_runtime.sh` desde Zenonia 5; subirlos al origen):
  CMakeLists.txt, imports.[ch], foundation.c, darwin.c, coretext.c, uikit.c.
- Log 022: **llega al título** (SETTINGS / START / MORE GAMES, STATE_ProcessMainMenu). La pantalla de
  términos (STATE_ProcessAgree: sin texto, OK sin respuesta) se salta en patch.c: GMAELOADER_CheckAgree
  devuelve 1 (equivale a tener Documents/agree.dat). Toques: llegan bien a MyEAGLView (EAGLView los
  multiplica por _scaleFactor = [UIScreen scale]); falta confirmar en el título a mano.
- Log 022 (manual): ni táctil ni botones. Todos los toques iban al ImageButton del ícono flotante del Hub
  de Com2uS (`-[HubManager getHubIcon]`, hijo directo de la ventana): `hit_view` aceptaba cualquier hija
  de la ventana sin mirar el frame. Ahora solo las de pantalla completa (o sin frame). UIButton no existe
  en el runtime (el ImageButton no hace nada al tocarlo).
- Log 025: los toques llegaban pero caían fuera de pantalla. `-[EAGLView touches*]` multiplica por
  `_scaleFactor` (2) y después `SGL_ChangeOrientationPosition` escala del espacio del dispositivo
  480x320 (`OriginalDisplay_ConfigInfo`) a 960x640: (191,276) -> (764,1104). patch.c quita las 8
  multiplicaciones (Began/Ended/Moved); la escala sigue en 2 para el render. Probar en consola.
- Log 026: táctil OK, llega a la creación de personaje. (a) Texto del sistema boca abajo y cortado:
  UIStringDrawing en un CGBitmapContext sin voltear (PushContext) deja el glifo espejado en iOS y el juego
  lo endereza en la textura; coretext.c `draw_glyph(..., uikit)` ahora espeja según el CTM. (b) Watchdog:
  8 s sin presentar, hilo principal ocioso en el run loop (ningún hilo del juego trabado) -> el timer del
  bucle dejó de disparar. TMP: el watchdog loguea timers/IME y se loguea CFRunLoopTimer quitado.
  Logs 027-029: el timer sí dispara; STATE_ProcessGame con GAMESTATE 0 y sin funciones (no dibuja nada).
  GAME_StartNewGame -> EVTSYSTEM_SetReady -> EVTSYSTEM_LoadEvent fallaba: `EXCELDATA_OpenFile` (eventdata.dat)
  + `cmp r0,#0; mov r8,r0; blt` en 0x37830, el mismo bug de punteros >= 0x80000000 que el barrido no veía
  por el mov intermedio. Agregado a signed_ptr_checks (patch_signed_check salta un mov). Probar en consola.
- Log 030: **ingame OK**. Guardado: save<N>.dat va en `<md5 HubSave>/` (como Inotia 3), carpeta que solo crea
  el login del Hub -> `createFileAtPath:` (foundation.c) crea la carpeta padre y reintenta. Barrido completo
  de llamadas a FILE_Open/RES_LoadToHeap/CS_fsOpen/EXCELDATA_OpenFile: sin checks con signo pendientes.
  Velocidad (como Inotia 3): APPINFO_nTimer (0x3a6e50, 60 ms = 16 FPS). START+SELECT corto cicla
  16/23/25/30 (config.txt), hook débil `port_speed_cycle` en input.c; espera mínima de MainProcess 10 -> 2 ms.
  Probar: guardar, salir, cargar; FPS reales a 30 (el juego gasta ~30 ms/frame en log 030).
- Log 033: guardado/carga OK. Botones físicos: `extras/controls.txt` (copiado a ux0:data) mapea la cruceta/stick
  a la cruceta del juego (STICK) y los botones a ataque, ranuras, poción y opciones, medido sobre la captura.
- Log 034: fondos translúcidos ausentes (popup de misión, paneles de stats). SGL_InitState deja una textura
  blanca 1x1 activa en la unidad 1 y SGL_grpFillRect solo apaga la 0 (glColor4x + draw). gles.c (común): mientras
  la unidad 0 está apagada se apaga la 1 en el draw (lo mismo que hizo Inotia 3 en su hook). Probar en consola.
- Texto con fuente del sistema invisible (USER AGREEMENT, barra del popup): SGL_RenderFontToAlphaImage
  dibuja en contexto gris con `CGContextSetGrayFillColor(1)`, que era no-op -> texto negro = alfa 0.
  Implementado + gris sin alfa ponderado por cobertura (uikit.c). Probar en consola.
- Pendiente visto: `NSUserDefaults initWithUser:`,
  `NSBundle initWithPath:`, `SKPaymentQueue`, `CFBitVector*`, `NSKeyedArchiver/Unarchiver` sin implementar.

1. `cp extras/nib.txt ux0_data/inotia4ios/nib.txt`, subir datos (`python3 -I tools/deploy_data.py`) e
   instalar `build/inotia4ios.vpk` (TITLEID `PSVI4I001`).
2. Abrir, bajar `ux0:data/inotia4ios/logs/` (`deploy_data.py --logs`): primer `unimpl:` / `nib:` / crash.
