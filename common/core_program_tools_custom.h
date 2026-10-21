/*
 * Program-editor navigation tools for the local Free42 fork.
 * This program is free software under the GNU General Public License, version 2.
 */
#ifndef CORE_PROGRAM_TOOLS_CUSTOM_H
#define CORE_PROGRAM_TOOLS_CUSTOM_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

bool core_program_label_jump(bool *enqueued, int *repeat);
bool core_program_follow(bool *enqueued, int *repeat);

#ifdef __cplusplus
}

bool core_program_find_upward(bool *enqueued, int *repeat);
bool core_program_find_downward(bool *enqueued, int *repeat);

// The input hooks consume FIND queries before native handlers can edit a program.
bool custom_program_find_keydown(int shift, int key);
bool custom_program_find_command(int command);
bool custom_program_find_finish_entry();
bool custom_program_find_draw_incomplete(int row);
void custom_program_find_draw_prompt();
void custom_program_tools_reset();
#endif

#endif
