# Inotia 4: Assassin of Berkel (PS Vita) — Beta Release v0.1

First public build of the native iOS-to-PS Vita wrapper port of *Inotia 4: Assassin of Berkel* (Com2uS). On real hardware the game boots, reaches the title screen, creates a character, plays in-game, and saves and loads. It adds an options menu with button remapping, game speed and a Spanish / Portuguese translation.

## 📦 Release Package Contents

- **`inotia4ios.vpk`** — the loader (TITLEID `PSVI4I001`, version `01.00`), with the LiveArea and `font.ttf`. No game code or assets.
- **`rebase.bin`** — relocation table for the 1.0.3 `Inotia4_global` executable (49,882 words).
- **`binds.bin`** — import slots resolved by the loader (6,201 slots).
- **`controls.txt`** — default button → touch mapping for the Inotia 4 HUD.
- **`inotia4-translation-es-pt.zip`** — Spanish and Portuguese translation pack (only the translated texts plus their string indices; no original game text). It is turned into `memorytext_es.dat` / `memorytext_pt.dat` from your own copy of the game.

## 🛠️ What are rebase.bin and binds.bin?

The loader maps the armv7 slice of the iOS executable at a fixed slide (`0x98000000`). `rebase.bin` lists every word holding an absolute address that must be moved, and `binds.bin` lists the slots the executable imports from Apple frameworks. Both are generated from your own executable by `tools/ios_prep.py` (run by `tools/stage_data.sh`); the attached copies match the 1.0.3 IPA.

## 🚀 Player Installation Guide

1. **Requirements**: Enso 3.60/3.65, **kubridge** in `ur0:tai/config.txt` (`*KERNEL` section), and `libshacccg.suprx` in `ur0:data/`.
2. **Game files**: a decrypted *Inotia 4* **1.0.3** IPA. With Python 3 (`lzma`, `capstone`), clone the repo, extract `Payload/Inotia4_global.app/` and run:
   ```bash
   TRANSLATION=path/to/inotia4-translation tools/stage_data.sh path/to/Inotia4_global.app
   ```
   (Leave out `TRANSLATION=...` for English only.)
3. **Copy** `ux0_data/inotia4ios/` to `ux0:data/inotia4ios/` on the Vita (app bundle, `rebase.bin`, `binds.bin`, `controls.txt`).
4. **Install** `inotia4ios.vpk` with VitaShell.
5. **Language**: open the options menu (**Start + Select**) and choose *Text language*. The game's own language must stay on English.

## 🎮 Controls

| Input | Action | Status |
| :---: | :--- | :---: |
| **Touch screen** | Game touch controls | ✅ Working |
| **Left stick / D-Pad** | Move (drags the on-screen D-pad) | ✅ Working |
| **Cross (✕)** | Attack | ✅ Working |
| **Square / Triangle / Circle / L / R** | Skill and item slots around the attack button | ✅ Working |
| **Select** | Potion | ✅ Working |
| **Start** | Game options (gear icon) | ✅ Working |
| **Start + Select** | Port options menu (remap, on-screen controls, speed, language) | ✅ Working |
| **L + R** | Show / hide the on-screen D-pad, attack and skill buttons | ✅ Working |

## 📋 Changelog (v0.1)

- **Loading**: Vita heap pointers (≥ `0x80000000`) failed the game's signed `< 0` checks after file and resource loads → 24 checks rewritten to test only real error codes (fonts, events, data files).
- **Touch**: touches were scaled twice (iOS `scaleFactor` plus the game's own screen scaling) → the extra multiplication is removed.
- **Text**: system-font text was drawn upside down and invisible in gray contexts → glyphs follow the context transform; gray fill colors implemented.
- **Translucent panels**: popup and stats backgrounds were missing (a white texture on texture unit 1 broke untextured fills on vitaGL) → unit 1 is turned off while the game draws untextured fills.
- **Saves**: the game saves into a per-account subfolder that only the online Hub login created → the folder is created when the save file is.
- **Game speed**: 16 / 23 / 25 / 30 FPS selector (as in the Inotia 3 port), with the frame's minimum wait lowered from 10 to 2 ms.
- **Options menu**: Start + Select opens a paused menu (as in the Zenonia 2 port) to remap buttons, toggle the on-screen controls, set the speed and the text language.
- **On-screen controls**: hidden with L + R (D-pad, attack, skill slots and potion only; portraits, HP and minimap stay).
- **Translation**: Spanish and Brazilian Portuguese for the whole game text (8,390 unique strings), switchable live.
- **Performance**: glyph cache for system-font text; the game thread runs alone on CPU core 0, audio (FMOD mixing) and the game's other threads on cores 1–2.
- **Terms screen**: skipped (it had no text and its OK button did not respond).

## ⚠️ Known Issues

- **"MORE GAMES" on the main menu crashes the game** (online Com2uS catalog). Don't press it.
- **Frame drops at 25–30 FPS**: the game's own logic takes about 30–40 ms per frame on the Vita; 16 and 23 FPS are the steady options. Drops also slow the game down, because its logic runs per frame.
- **Translation**: replaces the English text (the game language must be English); not proofread yet; some long names may not fit their boxes; each live switch keeps about 600 KB of the previous text in memory.
- **Online features disabled**: Com2uS Hub, news and in-app purchases.
- **Not verified on console yet**: the latest performance changes (glyph cache, core pinning) and the dialogue translation.

Installation and build instructions: see [README.md](README.md).
