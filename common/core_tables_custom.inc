/* Custom command definitions for the local Free42 fork.
 * Included with different CUSTOM_COMMAND definitions to generate command IDs
 * and cmd_array entries. No include guard: both expansions must read this list.
 * Append new commands without reordering existing entries to preserve their IDs.
 */

// ID name, handler, command name, flags, scode, code1, code2,
// name length, argument type, argument count, runtime argument types
CUSTOM_COMMAND(TO_INCH,    docmd_to_inch,    "\17INCH",  0x00, 0x00, 0xa7, 0xfe, 3, ARG_NONE, 1, 0x05)
CUSTOM_COMMAND(TO_METER,   docmd_to_meter,   "\17METER", 0x00, 0x00, 0xa7, 0xff, 2, ARG_NONE, 1, 0x05)
CUSTOM_COMMAND(TIME_HM,    docmd_time_hm,    "H.M",     0x00, 0x00, 0xa7, 0x80, 3, ARG_NONE, 0, NA_T)
CUSTOM_COMMAND(TIME_HMS,   docmd_time_hms,   "H.MS",    0x00, 0x00, 0xa7, 0x81, 4, ARG_NONE, 0, NA_T)
CUSTOM_COMMAND(TIME_HMSS,  docmd_time_hmss,  "H.MSs",   0x00, 0x00, 0xa7, 0x82, 5, ARG_NONE, 0, NA_T)
CUSTOM_COMMAND(TIME_ROUND, docmd_time_round, "TRND",    0x00, 0x00, 0xa7, 0x83, 4, ARG_NONE, 0, NA_T)
CUSTOM_COMMAND(TIME_TRUNC, docmd_time_trunc, "TRNC",    0x00, 0x00, 0xa7, 0x84, 4, ARG_NONE, 0, NA_T)
CUSTOM_COMMAND(TIME_DAY,   docmd_time_day,   "1d",      0x4c, 0x00, 0x00, 0x00, 2, ARG_NONE, 0, NA_T)
CUSTOM_COMMAND(TIME_CALC,  docmd_time_calc,  "T.CALC",  0x48, 0x00, 0x00, 0x00, 6, ARG_NONE, 0, NA_T)
