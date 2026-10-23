# Large integer hour counts can show incorrect days and hours in the binary core

TIME converts the integer-hour part to a decimal string before splitting it
into 365-day years, days, and hours. The binary core uses 17 significant decimal
digits for that conversion. A binary floating-point value can represent some
larger integers exactly, but their lower decimal digits are lost in the string.

## Reproduction

1. Use the binary core and calculate `2^60` in ordinary mode.
2. Open TIME and select `H.MS`.

The value is exactly `1152921504606846976` hours. Its correct display is:

```text
131612043904891y  75d 16:00:00
```

The binary core instead displays:

```text
131612043904891y  72d 12:00:00
```

The stored number is unchanged; the error occurs while formatting its integer
hours. Host probes reproduced the binary result and verified that the decimal
core displays this example correctly. Hardware has not been tested.

## Decision

Keep this as a known bug for now. A future fix should preserve the integer-hour
digits without reducing them to the binary core's 17-digit decimal string.
