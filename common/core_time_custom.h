/* Time display for the local Free42 fork. */
#ifndef CORE_TIME_CUSTOM_H
#define CORE_TIME_CUSTOM_H

#include "core_globals.h"

bool custom_time_active();
void custom_time_reset();
bool custom_time_keydown(int shift, int key);
void custom_time_menu_item(int command, int *selected);

int docmd_time_calc(arg_struct *arg);
int docmd_time_hm(arg_struct *arg);
int docmd_time_hms(arg_struct *arg);
int docmd_time_hmss(arg_struct *arg);
int docmd_time_round(arg_struct *arg);
int docmd_time_trunc(arg_struct *arg);
int docmd_time_day(arg_struct *arg);

// Return a character count without a terminator, or -1 to use ordinary formatting.
int custom_time_format(const vartype *value, char *buf, int buflen);
int custom_time_entry(const char *text, int length, char *buf, int buflen);
// Draw TIME input without flushing; return false to use the ordinary input display.
bool custom_time_draw_entry(int row);
// Decode the unfinished HMS input into hours without changing its editable text.
void custom_time_input(const char *text, int length, phloat *hours);

extern "C" {

// The platform stores precision, rounding and activation in 4 bits outside the core state.
unsigned char time_display_save();
void time_display_restore(unsigned char data);

}

#endif
