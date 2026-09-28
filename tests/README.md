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
(for example, `game.gb` and `game.map`). Keep `include/data/game_globals.i` in
the compiler output too; menu tests resolve variables from that generated file.
Use the compiler's `build/rom` directory,
not just the exported ROM download. Leave the project's default start scene and
position unchanged for the title/start test.

```sh
SMBMINI_ROM=/absolute/path/to/build/rom/game.gb python -m unittest discover -s tests -v
```

Also set `SMBMINI_BUILD` to the compiler output root (containing `src/data` and
`build/rom`) to compare generated VM writes with the C field sizes in the linker
`.cdb` file. This audit works on both standard ROM and native Pocket builds;
the PyBoy runtime tests require the standard Game Boy ROM. Use a clean build
for this audit: cached engine objects can leave their field-size records out
of the new `.cdb` file.

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

The World 16-2 giant-block test starts through the normal menu, positions Mario
under each half of the opening brick/coin blocks, and jumps. It checks small-Mario
bumps, powered-up brick destruction, and coin blocks: every intermediate map
change must stay inside the original 2x2 footprint, the bump sprite must align,
and the final tiles must match. It reproduces neighboring blocks being created
in `644df40`. GB Studio 4.3.2 folds `(coordinate >> 1) << 1` to `coordinate`;
the script uses `coordinate & 254` to preserve even-tile alignment instead.

The tall-map check enters World 12-3 through the menu and checks Mario's initial
ground contact, camera position, jump and landing below the 1024-pixel boundary.
It fails on `7788559`, where an unconditional signed 16-bit cast misclassifies
the bottom of this 1536-pixel map as a negative, above-screen position. The
existing above-screen jump test protects the other side of that boundary.

Fresh-game tests seed leftover power-ups and score/life counters at the menu,
then press Start in normal, B Quest and Random modes. A separate pipe-transition
test verifies that Fire Mario/Yoshi are preserved within an existing game.

The piranha fixture runs the real first pipe plant in World 2-1 with Mario held
on the pipe, beside it on either side, and far away at two different heights.
It also checks that a hidden plant resumes its cycle when Mario walks away.

The axe test seeds Bowser alive or already defeated in World 1-4, moves Mario
onto the real axe trigger, and observes the requested sound effects. Only the
living boss should play the defeat effect; both cases must still collapse the
bridge and let Mario reach the exit. The sound hook reads the documented local
4.3.2 compiler's banked-call stack layout, not the audio output device.

The World 9-1 balance-platform fixture sends one member of the real pair below
the map while Mario is attached either to it or to its still-visible partner.
Only the departing platform's own rider should detach.

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
