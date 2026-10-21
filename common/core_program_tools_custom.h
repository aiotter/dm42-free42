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

// These editor actions consume a key press; use the normal core_keyup() afterward.
bool core_program_label_jump(bool *enqueued, int *repeat);
bool core_program_follow(bool *enqueued, int *repeat);
bool core_program_find(bool reverse, bool *enqueued, int *repeat);

#ifdef __cplusplus
}

// Move to the adjacent program's 00 line.
// Call only after committing any pending program input.
void custom_program_step(bool reverse);

// The input hooks consume FIND queries before native handlers can edit a program.
bool custom_program_find_keydown(int shift, int key);
bool custom_program_find_command(int command);
bool custom_program_find_finish_entry();
bool custom_program_find_draw_incomplete(int row);
void custom_program_find_draw_prompt();
void custom_program_tools_reset();
#endif

#endif
