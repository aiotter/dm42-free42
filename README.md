# Free42 core extensions

A fork of SwissMicros' Free42 core with custom extensions for DM42/DM42n and a desktop GTK application.

For device settings, USB shortcuts, and screenshots, see the [DM42PGM frontend](https://github.com/aiotter/DM42PGM).
Firmware build and [flashing instructions](https://github.com/aiotter/DM42PGM.nix#direct-usb-flashing) are maintained in DM42PGM.nix.

## Core extensions

- Alternate-radix display in `BASE` mode and clearer command labels.
- Meter/inch conversions in the `CONVERT` menu.
- Stack clearing with a long press of `←`.
- Program-boundary navigation for `SST` / `BST`.

## HOME menu

The home menu automatically shows calculation controls in normal mode and editing tools in program mode.
The key assignments are defined in [core_home_menu_custom.cc](common/core_home_menu_custom.cc).

### Normal mode

- `TIME` (F3) opens time display and entry mode.
  Use `H.M`, `H.MS`, or `H.MSs` to set precision and `RND` or `TRNC` to set display rounding.
  During number entry, `1d` multiplies the entered duration by 24. Outside number entry, `1d` enters 24 hours.

### Program mode

- `LBLs` (F1) lets you select a label and move the cursor to it.
- `JUMP` (F2) jumps to the label targeted by the `GTO` or `XEQ` instruction at the cursor.
- `FIND` (F3) moves to the next matching command or text in the current program.
- `NewPG` (F4) inserts an empty program immediately after the current one and opens it at line 00.
- `DelPG` (Shift+F4) deletes the current unlocked program.

### History controls

These keys are available in normal mode and program mode.

- `AGAIN` (F5) repeats the last command in normal mode.
  In program mode, it reinserts the last program instruction or repeats the last search.
- `UNDO` (F6) and `REDO` (Shift+F6) undo and redo calculations, program edits, and cursor movements.

## Run the GTK application

With Nix flakes enabled, run the decimal GTK application directly from GitHub:

```sh
nix run github:aiotter/dm42-free42
```

The GTK package can also be selected explicitly with `nix run github:aiotter/dm42-free42#gtk`.

## Development

Custom modules use `*_custom.cc`; implementation fragments included by another source use `.inc`.

- [Host tests](tests/README.md)
- [Known issues](docs/known-issues/)
