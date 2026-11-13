# Progreso — Inotia 4 iOS

## 2026-11-13 — inicio

- Repo creado desde `IPA/Inotia4_global.ipa`. `psvita-toolkit init --ipa` dejó un scaffold de Android
  (FalsoJNI/soloader) y lo reemplacé por el runtime iOS de `../Zenonia5-iOS-vita` (`sync_runtime.sh`).
- Build OK (`psvita-toolkit build --preset release`): `build/eboot.bin`, `build/inotia4ios.vpk`.
- `ux0_data/inotia4ios/` armado con `tools/stage_data.sh` (sin `nib.txt`: NIBArchive no soportado).
- Imports sin cubrir: 143 (`docs/imports_sin_cubrir.txt`).

## PENDIENTE EN CONSOLA

1. `cp extras/nib.txt ux0_data/inotia4ios/nib.txt`, subir datos (`python3 -I tools/deploy_data.py`) e
   instalar `build/inotia4ios.vpk` (TITLEID `PSVI4I001`).
2. Abrir, bajar `ux0:data/inotia4ios/logs/` (`deploy_data.py --logs`): primer `unimpl:` / `nib:` / crash.
