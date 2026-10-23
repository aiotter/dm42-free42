# TIME can display an adjacent value at a rounding or truncation boundary

TIME stores durations as decimal hours. Converting minutes and seconds to
hours can require a recurring fraction, and ordinary arithmetic can leave the
result slightly above or below an exact display boundary.

TIME uses the same numeric HMS conversion as the existing HMS command. It
then formats the clock portion at Free42's ordinary 12-significant-digit
display precision before applying the selected RND or TRNC precision.
These steps improve many boundary cases, but cannot recover every exact value
that the user intended before floating-point rounding occurred.

## Reproduction in the binary core

1. Open TIME with `H.MS` and `RND` selected.
2. Enter `17`, then press ENTER.
3. Enter `16.59595` (16 hours, 59 minutes, 59.5 seconds).
4. Subtract.

The exact result of these durations is half a second, so rounding to whole
seconds would display `00:00:01`. The binary core instead displays
`00:00:00`: the stored result is slightly below half a second. Its HMS value
is approximately `0.0000499999999995282`, which remains below the boundary
even after the normal 12-digit display rounding.

The decimal core displays `00:00:01` for this example. Host probes reproduce
the difference; hardware has not been tested.

## Truncation near a boundary

The shared HMS conversion and its 12-digit text rounding happen before TRNC.
A stored value immediately below an exact minute or hour can therefore become
the exact boundary in the HMS text, and TRNC can display that boundary rather
than the preceding unit. The calculation value in the stack is unchanged.

## Decision

Keep the remaining boundary cases as known bugs for now. Reuse the existing
HMS conversion and normal display precision without adding an arbitrary error
tolerance or changing input values and ordinary arithmetic.
