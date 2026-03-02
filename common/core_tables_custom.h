/* Custom command IDs for the local Free42 fork; GPL version 2. */
#ifndef CORE_TABLES_CUSTOM_H
#define CORE_TABLES_CUSTOM_H

// Keep upstream's CMD_SENTINEL unchanged in core_tables.h so upstream command
// additions do not conflict with custom ID definitions. Generate IDs and the
// entries at the end of cmd_array from the same list to keep their order aligned.
// An enum captures the original sentinel value before the macro is redefined;
// a macro alias would instead expand using the later definition.
enum {
    // Seed the enum one below the first ID so every list entry auto-increments.
    CMD_CUSTOM_BEFORE_FIRST = CMD_SENTINEL - 1,
#define CUSTOM_COMMAND(name, ...) CMD_##name,
#include "core_tables_custom.inc"
#undef CUSTOM_COMMAND
    CMD_CUSTOM_SENTINEL
};

// Include custom commands in the command count used by lookup and dispatch.
#undef CMD_SENTINEL
#define CMD_SENTINEL CMD_CUSTOM_SENTINEL

#endif
