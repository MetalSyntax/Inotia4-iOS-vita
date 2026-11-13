# Port Plan — Inotia 4 iOS (PS Vita)

## 0. Confirmed from the binary (2026-11-13)

- IPA `Inotia4_global.ipa`: bundle `com.com2us.inotia4.normal.paidfull.apple.global.ios.universal`,
  version 1.0.3, MinimumOSVersion 4.3, universal (iPhone/iPad), landscape left/right.
- Executable `Inotia4_global`: fat armv6 + armv7, **cryptid 0** in both slices (already decrypted),
  PIE with `LC_DYLD_INFO_ONLY` (exact rebase/binds from `ios_prep.py`), entry via `LC_UNIXTHREAD`
  (0x33e0), **not stripped** (~19.8k symbols): hooks can go by name.
- `ios_prep.py`: 49,882 rebases, 6,201 bind slots / 686 symbols. C++: 13 imports, all mapped.
- Engine: Com2uS Inotia 3 codebase (`Inotia3AppDelegate`, `Inotia3AppDelegatee`), with the `CS_*`
  platform layer (138 `CS_` functions in the binary; 13 of the 28 names in Inotia2's `hle_cs.c` exist here).
  Same game as the Android port `../Inotia4-vita`.
- Audio: **FMOD Ex statically linked** (`Inotia_03.fev`, `*_bank0{0,1}.fsb`, ~3.8k FMOD symbols)
  outputting through AudioQueue (`OutputCoreAudioObjC`); runtime `audio.c` already covers AudioQueue.
  Missing: `AudioQueueOfflineRender`, `AudioQueueSetOfflineRenderFormat`, `AudioFileOpenWithCallbacks`.
- Startup: `main` → `UIApplicationMain(argc, argv, nil, nil)` → delegate comes from `MainWindow.nib`.
  That nib is **NIBArchive** (iOS 4+), which `tools/nib_prep.py` does not parse (it reads keyed-archive
  plists). Objects seen in it: `Inotia3AppDelegate` (outlets `window`, `glView`, `rootViewController`),
  `UIWindow`, `MyViewController`, `MyEAGLView`. `extras/nib.txt` is a hand-written guess.
- Bundled third-party SDKs (need offline stubs, not ports): Com2uS Hub, Flurry, GData (Google
  Contacts), Facebook, MGTwitter/OAuth, Tapjoy, GameKit, StoreKit, AddressBook.
- Base runtime: synced from `../Zenonia5-iOS-vita` (Com2uS, armv7, playable). Imports not covered:
  **143** (`docs/imports_sin_cubrir.txt`, ObjC classes excluded since `objc.c` resolves them):
  49 libxml2 (GData/Twitter/FB XML), CFStream/CFRunLoopTimer (network), NS*/UI* string constants,
  CFBitVector, a few CG drawing calls, `clock_get_time`/`host_get_clock_service`, `strptime`, `timegm`.

## 1. Reproducible prep

```sh
unzip -q ../IPA/Inotia4_global.ipa -d inotia4ios_extract
ln -sfn inotia4ios_extract/Payload/Inotia4_global.app game_app
python3 -I tools/ios_prep.py game_app/Inotia4_global build/prep --arch armv7
python3 -I tools/gen_cxx.py build/prep/report.txt source/cxx_imports.c
lipo -thin armv7 game_app/Inotia4_global -output build/Inotia4_armv7   # for annotate.py
tools/stage_data.sh && cp extras/nib.txt ux0_data/inotia4ios/nib.txt
```

## 2. Phases

- [x] Repo, runtime, port_config, first build (eboot + VPK) — 2026-11-13.
- [ ] Nib: confirm `extras/nib.txt` on console, or teach `nib_prep.py` NIBArchive
      (also needed for `rootViewController` = `MyViewController`).
- [ ] First boot on console: reach `applicationDidFinishLaunching:` → `MyEAGLView start:argc:argv:`.
- [ ] Uncovered imports hit at runtime (one per run, from `unimpl:` lines).
- [ ] Graphics (GLES1, 480x320 @2x, landscape rotation: `PORT_ROTATION`).
- [ ] FMOD audio over AudioQueue.
- [ ] Input (touch + buttons via `controls.txt`).
- [ ] Network/Hub/Flurry/Tapjoy offline (Reachability = no network or emulated, like Zenonia 5).
- [ ] Saves under `ux0:data/inotia4ios/Documents/`.
- [ ] LiveArea (reused from the Android port), README, release.

## 3. Risks

- `PORT_STAT_INO64 1` taken from Zenonia 5; confirm `st_size` offset in a `_stat` caller.
- Heavy UIKit social UI (Hub, table controllers) may be reached from menus: keep it unreachable.
- Universal binary: make sure iPhone paths are taken (`PORT_DEVICE_MACHINE "iPhone3,1"`, not iPad).
