# Engine regression checks

Run the patch-metadata and project-script memory-width checks with Python 3
(no additional dependencies), and editor-event checks with Node.js:

```sh
python -m unittest discover -s tests -v
node --test tests/test_platformer_event_widths.cjs
```

To also run the emulator checks, install PyBoy in a separate Python environment
and point `SMBMINI_ROM` at a ROM compiled from this checkout using GB Studio
4.3.2. Keep the corresponding linker `.map` beside the ROM with the same stem
(for example, `game.gb` and `game.map`). Use the compiler's `build/rom` directory,
not just the exported ROM download. Leave the project's default start scene and
position unchanged for the title/start test.

```sh
SMBMINI_ROM=/absolute/path/to/build/rom/game.gb python -m unittest discover -s tests -v
```

Also set `SMBMINI_BUILD` to the compiler output root (containing `src/data` and
`build/rom`) to compare generated VM writes with the C field sizes in the linker
`.cdb` file. This audit works on both standard ROM and native Pocket builds;
the PyBoy runtime tests require the standard Game Boy ROM.

```sh
SMBMINI_BUILD=/absolute/path/to/compiler-output python -m unittest discover -s tests -v
```

The checks cover World 16 map/collision SRAM integrity, camera behavior during
an above-screen jump, repeated elevator loops in both directions, detachment
when an elevator wraps, disabled/inactive platform detachment and valid carrying
in ground/crouch/swim states, the title/demo-to-play transition, and an enemy stomp
in World 1-1 that previously corrupted the script context and triggered a kernel
panic. The stomp route uses only normal controller input and asserts both a stomp
and continued script execution, including during the normal death animation.

These are controlled regression probes, not full playthroughs. Some change the
emulator's in-memory start scene or seed player/platform state using function
hooks; the ROM on disk and the project are never changed. Assertions wait for
the movement update to finish rather than assuming one emulated frame equals
one game update. The fixtures use GB Studio 4.3.2's actor layout and this game's
variable indices, so review them if either changes. Runtime checks are skipped
unless `SMBMINI_ROM` is set; when set, a missing PyBoy installation, ROM, or map
is an error.

Validated with GB Studio 4.3.2's installed compiler and PyBoy 2.7.0. The camera,
elevator visibility, and invalid-platform tests also reproduce the failures in
the pre-fix ROM (built after correcting its malformed collision patch). The stomp
test reproduces the panic in commit `aa5d8c9` at gameplay frame 349, with ROM bank
`0xC9`. The memory-width audit rejects that build's oversized state/bounce writes.
