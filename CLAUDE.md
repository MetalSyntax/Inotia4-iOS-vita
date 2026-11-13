# CLAUDE.md

Port de **Inotia 4: Assassin of Berkel** (iOS 1.0.3, `Inotia4_global`, Mach-O fat armv6+armv7) a PS Vita
con el loader de Mach-O de la familia iOS (no soloader). Documentación en español; README en inglés.

## Leer primero

- `PORTING_PLAN.md`: qué se confirmó del binario y las fases.
- `port_progress.md`: estado y **PENDIENTE EN CONSOLA**.
- Mismo juego en Android: `../Inotia4-vita` (libgame.so, wrapper `com.com2us.wrapper`). El motor es el
  de Inotia 3; en iOS el delegate se llama `Inotia3AppDelegate`. Capa `CS_*` de Com2uS igual que en
  `../Inotia2-vita/source/hle_cs.c` (comparar por símbolo, no copiar a ciegas).

## Estructura

- `source/`: runtime iOS común (macho, darwin, objc, foundation, uikit, gles, audio, coretext...).
  **No editar a mano lo común**: se trae de `../Zenonia5-iOS-vita` con `tools/sync_runtime.sh`
  (corrido desde el origen). Propio de este port: `source/port_config.h`, `source/patch.c`,
  `source/cxx_imports.c` (regenerar con `tools/gen_cxx.py`).
- `game_app` → symlink a `inotia4ios_extract/Payload/Inotia4_global.app` (no versionado).
- `tools/ios_prep.py game_app/Inotia4_global build/prep --arch armv7`: rebase.bin/binds.bin/report.txt.
- `tools/annotate.py build/Inotia4_armv7 <símbolo>`: necesita el slice delgado
  (`lipo -thin armv7 game_app/Inotia4_global -output build/Inotia4_armv7`).
- `tools/stage_data.sh` arma `ux0_data/inotia4ios/`; `tools/deploy_data.py` lo sube (`--logs` baja logs).
- `extras/nib.txt`: nib escrito a mano (ver PORTING_PLAN §2).

## Reglas

- ABI iOS: softfp, r9 temporal (`-ffixed-r9`, `call_guest`), pila alineada a 4 (`ml_align_call`),
  int64 sin alinear a par de registros, variádicos con `darwin_vformat`.
- Compilar con `psvita-toolkit build --preset release` (la ruta tiene espacios).
- Las pruebas son en la Vita real del usuario. Sin push ni FTP salvo que lo pida.
- Nada del IPA en git. Commits terminan con `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Crash en consola → skill `so-crash-triage` con `tools/annotate.py` en vez de Ghidra.
