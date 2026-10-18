/*
 * Custom extension for local Free42 fork.
 * This program is free software under the GNU General Public License, version 2.
 */
#include "core_base_custom.h"
#include "core_helpers.h"
#include "core_display.h"
#include "core_main.h"

static bool altbase_enabled = true;
static int altbase_radix = 0;

void custom_base_reset() {
    altbase_enabled = true;
    altbase_radix = 0;
}

static int alternate_base() {
    // Resolve the initial default once, then retain it across main-base changes.
    if (altbase_radix == 0) altbase_radix = get_base() == 2 ? 10 : 2;
    return altbase_radix;
}

static bool alternate_menu(int menu) {
    return baseapp && !flags.f.prgm_mode && !mode_command_entry
           && !mode_alpha_entry && menu == MENU_BASE1;
}

static int command_base(int command) {
    switch (command) {
        case CMD_BINM: return 2;
        case CMD_OCTM: return 8;
        case CMD_DECM: return 10;
        case CMD_HEXM: return 16;
        default: return 0;
    }
}

void custom_base_shift_menu_changed() {
    if (alternate_menu(get_front_menu())) redisplay();
}

void custom_base_menu_item(int menu, int command, const char **name, int *length, int *selected) {
    if (!mode_shift || !alternate_menu(menu)) return;
    int base = command_base(command);
    if (!base) return;
    // HP-42S character 14 is the downward arrow marking the alternate pane below.
    *name = base == 2 ? "BIN\016" : base == 8 ? "OCT\016" : base == 10 ? "DEC\016" : "HEX\016";
    *length = 4;
    *selected = altbase_enabled && alternate_base() == base;
}

bool custom_base_keydown(int shift, int key) {
    if (!shift || !alternate_menu(get_front_menu()))
        return false;
    int menukey = find_menu_key(key);
    if (menukey < 0 || menukey >= 6) return false;
    int base = command_base(menus[MENU_BASE1].child[menukey].menuid & 0xfff);
    if (!base) return false;
    // Selecting the visible alternate again hides it; any selection while hidden shows it.
    altbase_enabled = !altbase_enabled || alternate_base() != base;
    altbase_radix = base;
    redisplay();
    return true;
}

unsigned char base_display_save() {
    if (!altbase_enabled) return 0;
    switch (alternate_base()) {
        case 2: return 1;
        case 8: return 2;
        case 10: return 4;
        case 16: return 8;
        default: return 0;
    }
}

void base_display_restore(unsigned char data) {
    custom_base_reset();
    // Other display settings may use the upper 4 bits. Multiple radix bits do
    // not identify one pane, so treat them like the disabled zero encoding.
    switch (data & 15) {
        case 1: altbase_radix = 2; break;
        case 2: altbase_radix = 8; break;
        case 4: altbase_radix = 10; break;
        case 8: altbase_radix = 16; break;
        default: altbase_enabled = false; break;
    }
}

int base_display_base() {
    return baseapp && !flags.f.prgm_mode ? get_base() : 0;
}

int custom_base_format(phloat value, char *buf, int buflen, int base) {
    if (buflen < 2) {
        if (buflen > 0) buf[0] = 0;
        return 0;
    }
    // Follow the normal register's display settings, including its 12-digit precision limit.
    int dispmode = flags.f.fix_or_all ? (flags.f.eng_or_all ? 3 : 0)
                                    : (flags.f.eng_or_all ? 2 : 1);
    int digits = 0;
    if (flags.f.digits_bit3) digits += 8;
    if (flags.f.digits_bit2) digits += 4;
    if (flags.f.digits_bit1) digits += 2;
    if (flags.f.digits_bit0) digits += 1;
    int len = phloat2string(value, buf, buflen - 1, 1,
                           digits, dispmode, flags.f.thousands_separators, 12, NULL, base);
    // Lowercase hexadecimal digits without changing error descriptions.
    if (len > 0 && buf[0] != '<' && base == 16)
        for (int i = 0; i < len; i++)
            if (buf[i] >= 'A' && buf[i] <= 'F') buf[i] += 'a' - 'A';
    buf[len] = 0;
    return len;
}

int custom_vartype2string(const vartype *value, char *buf, int buflen, int *base) {
    *base = 0;
    int selected_base = base_display_base();
    if (selected_base && value->type == TYPE_REAL) {
        int len = custom_base_format(((const vartype_real *) value)->x, buf, buflen, selected_base);
        if (len && buf[0] != '<') *base = selected_base;
        return len;
    }
    return vartype2string(value, buf, buflen - 1);
}

int altbase_visible() {
    if (!base_display_base() || !altbase_enabled) return 0;
    if (sp < 0) return 1;
    if (stack[sp]->type != TYPE_REAL) return 0;
    // An exponent without digits does not yet describe the input's final value.
    if (mode_number_entry && (cmdline_length == 0
            || cmdline[cmdline_length - 1] == 24
            || (cmdline[cmdline_length - 1] == '-'
                && (cmdline_length == 1 || cmdline[cmdline_length - 2] == 24))))
        return 0;
    return 1;
}

int altbase2str(char *buf, int buflen, int *display_base) {
    *display_base = 0;
    if (buflen <= 0) return 0;
    buf[0] = 0;
    if (!altbase_visible()) return 0;
    int base = alternate_base();
    if (sp < 0) {
        *display_base = base;
        return 0;
    }
    int len = custom_base_format(((vartype_real *) stack[sp])->x, buf, buflen, base);
    if (len && buf[0] != '<') *display_base = base;
    return len;
}
