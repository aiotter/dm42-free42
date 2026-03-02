/* Custom command definitions for the local Free42 fork.
 * Included with different CUSTOM_COMMAND definitions to generate command IDs
 * and cmd_array entries. No include guard: both expansions must read this list.
 * Append new commands without reordering existing entries to preserve their IDs.
 */

// ID name, handler, command name, flags, scode, code1, code2,
// name length, argument type, argument count, runtime argument types
CUSTOM_COMMAND(TO_INCH,    docmd_to_inch,    "\17INCH",  0x00, 0x00, 0xa7, 0xfe, 3, ARG_NONE, 1, 0x05)
CUSTOM_COMMAND(TO_METER,   docmd_to_meter,   "\17METER", 0x00, 0x00, 0xa7, 0xff, 2, ARG_NONE, 1, 0x05)
