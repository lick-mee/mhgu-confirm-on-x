# Confirm on X

<p align="center">
  <a href="https://github.com/lick-mee/mhgu-confirm-on-x/releases/latest"><b>Download the latest release</b></a>
</p>

A controls mod for [MHGU Recomp](https://github.com/morerokk/MHGURecomp), the PC port of Monster Hunter Generations Ultimate.

On a PlayStation-style layout the game confirms menus with Circle and goes back with Cross, the Nintendo way round. Swapping the two buttons in the recomp settings fixes the menus, but then the hunter rolls with Circle. Confirm on X swaps them **in menus only**:

| | Bottom button (✕ / Xbox A) | Right button (◯ / Xbox B) |
|---|---|---|
| **Menus** (shops, item box, quest counter, Yes/No) | confirm | back / cancel |
| **Everything else** | roll / dodge | talk, gather, carve, open boxes, climb |

## Install

1. Download `confirm-on-x-v1.0.0.zip` from [Releases](https://github.com/lick-mee/mhgu-confirm-on-x/releases).
2. Extract the `confirm-on-x` folder into the `mods` folder of [MHGU Recomp](https://github.com/morerokk/MHGURecomp).
3. Start the game.

There's nothing to run: it's a plugin the recomp loads by itself (`log.txt` shows `confirm-on-x: confirm on the bottom button`). To uninstall, delete the `confirm-on-x` folder.

Keep `FaceButtons = Position` in `mhgurecomp.ini` (the default). It works the same with PlayStation, Xbox (XInput) and Switch-style controllers, since it goes by where the button sits, not what is printed on it.

## Options

Press **Insert** in game and open **Confirm on X**:

- **Confirm with bottom button, cancel with right**: the mod itself; untick it to get the original menus back.
- **In towns, also talk/open boxes with the bottom button**: off by default. In the village, the Hub and the Pub the two buttons swap for the hunter too (the right one then rolls).
- **On quests, the same while a gather/carve/box prompt is shown**: off by default. While a prompt is on screen the bottom button interacts as well; nothing rolls then.

The overlay also shows whether the plugin sees you in town or on a quest. Options reset at each start.

## Compatibility

- Tested on MHGU Recomp 0.9.7, which runs the game at version 1.4.0.
- Doesn't replace any game files, so it works alongside texture, UI and font mods ([Pixel Pack v4](https://www.reddit.com/r/MonsterHunter/comments/1icfs4z/mhgu_hd_texture_mod_60_fps_and_multiplayer_2025/), [HD Hunter Font](https://github.com/lick-mee/mhgu-hd-hunter-font), …).
- On-screen button icons are the game's own. Field prompts (gather, carve…) still match, since the right button keeps doing those.
- Recomp only: it's a Forge plugin that hooks the recomp's game code. Emulators aren't supported.

## How it works

The game keeps a separate menu input object next to the controller state: every frame it takes the pad's held buttons, folds the left stick into the D-pad, and menu code reads "confirm" and "cancel" there. The plugin hooks the function that fills it and swaps the two face buttons in its result, so menus see the swap and the hunter, who reads the controller state directly, doesn't. `src/plugin.c` has the details and the game addresses, including those the optional modes use.

## Build

Needs [Zig](https://ziglang.org/download/) 0.17 (the C compiler and linker for the 32-bit ARM plugin) and Python 3:

```sh
sh build.sh                     # out/confirm_on_x.nro
sh build.sh /path/to/recomp     # and install it into that recomp's mods folder
sh build.sh --zip               # and pack the release zip into dist/
```

Set `ZIG=/path/to/zig` if Zig isn't on your `PATH`. `build/elf2nro.py` turns the linked library into an NRO, and `src/forge.h` lists the Forge API calls the plugin uses. `EXTRA_CFLAGS=-DCONFIRM_DEV` builds a dev variant whose number keys take memory snapshots and log what differs between two game states, which is how the game values were found.

`icons/make_icons.py` swaps the ✕ and ◯ button icons, for use with the optional modes. It patches your own copies of the archives (`python icons/make_icons.py [--town-only] /path/to/recomp`) and writes them as the mod `zzz-confirm-on-x-icons`, which has to load after other mods that replace those archives. Run it again after updating such a mod.

## Thanks

[MHGU Recomp](https://github.com/morerokk/MHGURecomp), which this mod is made for.

## License

MIT for the code. Monster Hunter Generations Ultimate © CAPCOM.
