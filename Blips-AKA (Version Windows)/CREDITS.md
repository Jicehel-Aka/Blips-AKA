# Credits & licences

**Blips for Gamebuino AKA** is a port of **Blips**, written by **Willems Davy (joyrider3774)**, itself a
remake of the DOS games *Bips*, *Bips Gold* and *Bips Platinum* by **Bryant Brownell**. This file lists
everything that comes from somebody else, under which licence, and what was changed or deliberately
left out. If you spot a mistake or an omission, please open an issue.

## The original game

- **Title:** Blips
- **Author / copyright:** © 2024 Willems Davy (joyrider3774)
- **Source:** https://github.com/joyrider3774/blips
- **Licence:** MIT (see `LICENSE`, which keeps the original copyright notice)
- **Original idea:** *Bips*, *Bips Gold* and *Bips Platinum* (DOS), Bryant Brownell.

The game logic (`main/engine/blips_world.*`, `blips_game.*`) is **derived from the original sources**
(`CWorldPart`, `CPlayer`, `CWorldParts`, `CViewPort`, `Game.cpp`): the part model, the movement rules,
the explosions and the level format are kept on purpose so that every level plays exactly the same. What
changed:

- no SDL: the engine knows nothing of the screen, drawing and sound go through callbacks;
- 16 pixel tiles on a 320×240 screen (the original: 32 pixel tiles on 640×360). The world is stepped 30
  times per second instead of 60 and moves 2 px per step instead of 2 px at 60 Hz with tiles twice as
  big, so the speed **on screen is the same**;
- the pointers that the original keeps to deleted parts are cleaned up (`World::forget`);
- the screens (`main/app.cpp`) are new, written for the AKA buttons.

### What was added, changed or left out

- Added: Gamebuino AKA version (ESP32-S3), SDL2 desktop version that runs the *same* game code, five
  languages (FR, EN, DE, ES, IT), pause menu, screenshot, a "how to play" screen, an "all levels open"
  option, SD-card layout and release scripts.
- Level editor: re-implemented with the rules of the original editor (`World::EditPlace`,
  `CenterLevel`); it writes the same binary `.lev` files, into `BLIPS/mylevels/<NAME>/`.
- Not included: USB joystick set-up, skins, the intro and title pictures, the statistics screen.
- **Dynamite, bomb box, explosion and eraser sprites** and **six of the sound effects** were redrawn /
  re-synthesized for this port, see below and `assets/original/graphics/README.txt`.

## Graphics

| Asset | Author | Licence |
|-------|--------|---------|
| Wall (and its cracked variant) | [1001.com](https://opengameart.org/content/sokoban-pack) | [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) |
| Floor, player 1 | [Kenney – Sokoban 100 tiles](https://opengameart.org/content/sokoban-100-tiles) | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) |
| Coin | [Kenney – game assets all in 1](https://kenney.itch.io/kenney-game-assets) | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) |
| Boxes (plain, player 1, player 2, wall box) | [SpriteAttack – boxes and crates](https://opengameart.org/content/boxes-and-crates-svg-and-pngs) | [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) |
| Player 2 | colour variant of player 1; not documented upstream, treated as CC0 like player 1 | – |
| Dynamite, bomb box, explosion, eraser | drawn for this port by `tools/gen_tiles.py` | MIT |

The sprites are in `assets/original/graphics/`; `tools/gen_tiles.py` resizes them to 16 px into
`main/assets/tiles_data.cpp`. **The wall tile is a CC BY-SA 3.0 work and the resized walls in
`tiles_data.cpp` are an adaptation of it: that adaptation is shared under the same licence
(CC BY-SA 3.0).** The rest of the code is not affected.

**Not redistributed:** the original dynamite is from *GUI Icons* by Rexard (a **paid** asset, the
original author states "do not reuse"). It is therefore absent from this repository: `box.png` was
cut to its four free frames and the dynamite and bomb box are new drawings. The upstream explosion,
eraser ("empty"), title/background/intro pictures have no documented origin and are not used either.

## Music

| File on the SD card | Title | Author | Licence |
|---|---|---|---|
| `music/title.wav` (made from `assets/music/title.mod` at release time) | title.mod | donskeeto | **not stated** upstream (the README only says "Music was made by donskeeto") |

**Caveat:** the module file was made for the original game ("for willems soft", 2007) and no licence
is stated for it. It is included with this credit, because the original game ships it under its MIT
repository. If you redistribute this package and want to be strictly safe, delete `assets/music/title.mod`
(the game then runs without music) or ask the author. The conversion (`tools/make_sd_audio.py music`)
decodes the module with ffmpeg and writes mono 16-bit 44.1 kHz WAV, the only format the AKA plays.

## Sound effects (`SD_files/BLIPS/sound/`)

| File | Author | Licence |
|---|---|---|
| `move.wav` | Willems Davy (made with BXFR) | "feel free to use" (the original author's words) |
| `menu`, `select`, `menuback`, `error`, `collect`, `explode`, `stageend` | synthesized for this port by `tools/make_sd_audio.py` | MIT |

**Not redistributed:** the original `stageend.wav` is from a **paid** pack ("do not reuse"). The other
six upstream sound files have no documented origin (`sound/credits.txt` is empty), so they are not
shipped; `tools/make_sd_audio.py upstream <folder>` can convert them **for your own use** if you have
the original game.

## Level packs (`SD_files/BLIPS/levelpacks/`)

The four packs *bips* (26 levels), *bips gold* (9), *bips gold 2 players* (9) and *bips platinum*
(25) are the **unmodified** `.lev` files and `credits.dat` of the original game.

**The levels remain © their authors**: Bryant Brownell, Landon Brownell, Caryn Brownell and the
PocoMan team (as listed in the original README). No licence text for the levels is given in the original
repository: they are included because the original project distributes them. If you redistribute this
package yourself, check with the authors that this suits them, or remove the packs you are unsure
about: the game works with any set of packs in `BLIPS/levelpacks/`.

## Gamebuino AKA library — `components/gamebuino/`

- © Gamebuino 2026, author Jean-Marie Papillon — **GNU LGPL v3 or later** (texts in `THIRD_PARTY/`).
- Used here unchanged, except for the additions made earlier by the AKA project (the `drawImage`
  family in `include_lib/gb_graphics_image.cpp`, the case-fix script `tools/fix_gamebuino_case.py`).
- The PC build compiles this very library on the desktop and replaces only the hardware layer
  `gb_ll_*` by `pc/gb_ll_pc.cpp` (SDL2).
- Font: `font8x8_basic` by Daniel Hepper, based on public-domain VGA fonts — public domain. The
  accented glyphs (`main/assets/font_accents.*`, generated by `tools/gen_font.py`) are part of the AKA
  game collection.

## Desktop version

- **SDL2** — zlib licence (`THIRD_PARTY/SDL2-zlib.txt`); Linux uses the system library, the Windows
  package ships `SDL2.dll`.

## Port

- Gamebuino AKA / SDL2 port: **Jicehel**, 2026, with the help of Claude (Anthropic) for the code.
