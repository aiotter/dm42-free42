/*
 * Custom extension for local Free42 fork.
 * Copyright (C) 2026
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 */

#include "core_commands_custom.h"

#include "core_helpers.h"
#include "core_sto_rcl.h"

static int mappable_to_inch(phloat x, phloat *y) {
    phloat r;
    int inf;
    r = x * 5000 / 127;
    if ((inf = p_isinf(r)) != 0) {
        if (flags.f.range_error_ignore)
            r = inf == 1 ? POS_HUGE_PHLOAT : NEG_HUGE_PHLOAT;
        else
            return ERR_OUT_OF_RANGE;
    }
    *y = r;
    return ERR_NONE;
}

int docmd_to_inch(arg_struct *arg) {
    vartype *v;
    int err = map_unary(stack[sp], &v, mappable_to_inch, NULL);
    if (err == ERR_NONE)
        unary_result(v);
    return err;
}

static int mappable_to_meter(phloat x, phloat *y) {
    phloat r;
    int inf;
    r = x * 127 / 5000;
    if ((inf = p_isinf(r)) != 0) {
        if (flags.f.range_error_ignore)
            r = inf == 1 ? POS_HUGE_PHLOAT : NEG_HUGE_PHLOAT;
        else
            return ERR_OUT_OF_RANGE;
    }
    *y = r;
    return ERR_NONE;
}

int docmd_to_meter(arg_struct *arg) {
    vartype *v;
    int err = map_unary(stack[sp], &v, mappable_to_meter, NULL);
    if (err == ERR_NONE)
        unary_result(v);
    return err;
}
