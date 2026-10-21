# Free42 core extensions

A fork of SwissMicros' Free42 core with custom extensions for DM42/DM42n and a desktop GTK application.

For device controls and screenshots, see the [DM42PGM frontend](https://github.com/aiotter/DM42PGM).
Firmware build and [flashing instructions](https://github.com/aiotter/DM42PGM.nix#direct-usb-flashing) are maintained in DM42PGM.nix.

## Core extensions

- Alternate-radix display in `BASE` mode and clearer command labels.
- Meter/inch conversions in the `CONVERT` menu.
- Long-press stack clearing.
- Command replay, instruction reinsertion, and repeated searches for `AGAIN`.
- Calculation, program-edit, and cursor-position history for `UNDO` / `REDO`.
- Program-editor APIs for label navigation, following the `GTO` / `XEQ` at the cursor, and finding commands or text.
- Program-boundary navigation with wrap notifications for `SST` / `BST`.

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
