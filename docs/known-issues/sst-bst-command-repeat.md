# Holding a command-entry confirmation can change SST/BST navigation

In program mode with no core menu displayed, Shift+Down and Shift+Up move to
the next and previous program's 00 line. Ordinary Down and Up move by one line.

SST/BST selected as commands initially move by one line, but their automatic
key repeat is internally registered as Shift+Down/Up. If the menu is no longer
displayed when repeat begins, subsequent repeats move between programs instead.
This is key-hold repeat, separate from the custom AGAIN action.

## Reproduction on the DM42n input path

1. Have at least two programs and enter program mode.
2. With no core menu displayed, press XEQ, then ENTER to enter a command name.
3. Type SST or BST. There must be no global label with the same name.
4. Press the final ENTER to confirm and keep holding it for more than one second.

The initial confirmation moves by one line. The first repeat switches to the
next program for SST, or the previous program for BST, selecting its 00 line.
Releasing ENTER before repeat starts avoids the issue. Use ordinary Down/Up
for repeated line navigation.

The DM42n frontend starts a repeat timer when the core requests key repeat,
including when the held key is ENTER. Host tests using the real core and the
DM42n input entry point reproduced this path; it has not been verified on hardware.

## Decision

This remains a known bug. The unusual command-entry-and-hold operation does not
justify changing the upstream command-repeat registrations across multiple
execution paths. Keeping those paths unchanged reduces maintenance when merging
upstream changes. The Shift+Down/Up program-navigation extension is retained.
