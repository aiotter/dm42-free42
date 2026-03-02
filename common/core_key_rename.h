/*
 * Custom extension for local Free42 fork.
 * Copyright (C) 2026
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 */

#ifndef CORE_KEY_RENAME_H
#define CORE_KEY_RENAME_H

#include "core_tables.h"

static inline void core_key_rename(int cmd_id, const char **name, int *name_len) {
    switch (cmd_id) {
        case CMD_SL:
            *name = "<<";
            *name_len = 2;
            return;
        case CMD_SR:
            *name = ">>";
            *name_len = 2;
            return;
        case CMD_HEXM:
            *name = "0xff";
            *name_len = 4;
            return;
        case CMD_DECM:
            *name = "99";
            *name_len = 2;
            return;
        case CMD_OCTM:
            *name = "0o77";
            *name_len = 4;
            return;
        case CMD_BINM:
            *name = "0b11";
            *name_len = 4;
            return;
        case CMD_TO_INCH:
            *name = "\17in";
            *name_len = 3;
            return;
        case CMD_TO_METER:
            *name = "\17m";
            *name_len = 2;
            return;
        case CMD_TO_HR:
            *name = "\17Hour";
            *name_len = 5;
            return;
        case CMD_TO_HMS:
            *name = "\17H.MS";
            *name_len = 5;
            return;
        default:
            *name = cmd_array[cmd_id].name;
            *name_len = cmd_array[cmd_id].name_length;
            return;
    }
}

#endif /* CORE_KEY_RENAME_H */
