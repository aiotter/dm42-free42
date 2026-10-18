/* Custom BASE extension, which is called from existing Free42 functions. */
#ifndef CORE_BASE_CUSTOM_H
#define CORE_BASE_CUSTOM_H

#include "core_globals.h"

void custom_base_reset();
bool custom_base_keydown(int shift, int key);
void custom_base_shift_menu_changed();
void custom_base_menu_item(int menu, int command, const char **name, int *length, int *selected);
int custom_base_format(phloat value, char *buf, int buflen, int base);
int custom_vartype2string(const vartype *value, char *buf, int buflen, int *base);

extern "C" {

// A zero base suppresses the radix label; an empty X can still have a label.
int base_display_base();
// Keep the alternate pane visible even when an empty stack has no value to format.
int altbase_visible();
int altbase2str(char *buf, int buflen, int *base);

// Use one bit per radix in the low 4 bits; zero disables the alternate pane.
unsigned char base_display_save();
void base_display_restore(unsigned char data);

} // extern "C"

#endif
