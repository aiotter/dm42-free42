/* Time display for the local Free42 fork; GPL version 2. */
#include "core_time_custom.h"
#include "core_commands_custom.h"
#include "core_commands2.h"
#include "core_display.h"
#include "core_helpers.h"
#include "core_keydown.h"
#include "core_main.h"
#include "shell.h"

#include <stdio.h>
#include <string.h>

static unsigned char time_precision = 1;
static bool time_truncate = false;

void custom_time_reset() {
    time_precision = 1;
    time_truncate = false;
}

bool custom_time_active() {
    return mode_appmenu == MENU_TIME && !flags.f.prgm_mode;
}

int docmd_time_calc(arg_struct *arg) {
    int error = set_menu_return_err(MENULEVEL_APP, MENU_TIME, false);
    if (error != ERR_NONE) return error;
    // Standalone radix commands can leave nondecimal input without a BASE menu.
    // TIME starts in decimal and preserves ENTER's disabled stack lift.
    set_base(10);
    mode_disable_stack_lift = flags.f.stack_lift_disable;
    return ERR_NONE;
}

bool custom_time_keydown(int shift, int key) {
    if (shift || flags.f.prgm_mode || mode_command_entry || mode_alpha_entry || !mode_number_entry
            || get_front_menu() != MENU_TIME)
        return false;
    int item = find_menu_key(key);
    if (item < 0 || item >= 6) return false;
    int command = menus[MENU_TIME].child[item].menuid & 0xfff;
    // Execute input controls before the ordinary menu path can finish number entry.
    int error = cmd_array[command].handler(NULL);
    if (error != ERR_NONE)
        display_error(error);
    else if (command == CMD_TIME_DAY) {
        keydown(0, 2048 + CMD_NULL);
        pending_command = CMD_NONE;
        print_menu_trace("1d", 2);
    }
    redisplay();
    return true;
}

void custom_time_menu_item(int command, int *selected) {
    switch (command) {
        case CMD_TIME_HM: *selected = time_precision == 0; break;
        case CMD_TIME_HMS: *selected = time_precision == 1; break;
        case CMD_TIME_HMSS: *selected = time_precision == 2; break;
        case CMD_TIME_ROUND: *selected = !time_truncate; break;
        case CMD_TIME_TRUNC: *selected = time_truncate; break;
    }
}

// Display settings leave the next number's stack-lift behavior unchanged.
int docmd_time_hm(arg_struct *arg) {
    time_precision = 0;
    mode_disable_stack_lift = flags.f.stack_lift_disable;
    return ERR_NONE;
}

int docmd_time_hms(arg_struct *arg) {
    time_precision = 1;
    mode_disable_stack_lift = flags.f.stack_lift_disable;
    return ERR_NONE;
}

int docmd_time_hmss(arg_struct *arg) {
    time_precision = 2;
    mode_disable_stack_lift = flags.f.stack_lift_disable;
    return ERR_NONE;
}

int docmd_time_round(arg_struct *arg) {
    time_truncate = false;
    mode_disable_stack_lift = flags.f.stack_lift_disable;
    return ERR_NONE;
}

int docmd_time_trunc(arg_struct *arg) {
    time_truncate = true;
    mode_disable_stack_lift = flags.f.stack_lift_disable;
    return ERR_NONE;
}

int docmd_time_day(arg_struct *arg) {
    if (mode_number_entry && custom_time_active()) {
        // A day suffix replaces unfinished input without lifting the stack or changing LAST X.
        phloat hours = ((vartype_real *) stack[sp])->x * 24;
        int inf = p_isinf(hours);
        if (inf) {
            if (!flags.f.range_error_ignore) return ERR_OUT_OF_RANGE;
            hours = inf > 0 ? POS_HUGE_PHLOAT : NEG_HUGE_PHLOAT;
        }
        ((vartype_real *) stack[sp])->x = hours;
        return ERR_NONE;
    }
    arg_struct number;
    number.type = ARGTYPE_DOUBLE;
    number.val_d = 24;
    return docmd_number(&number);
}

int custom_time_entry(const char *text, int length, char *buf, int buflen) {
    if (!custom_time_active()) return -1;
    // EEX keeps the existing numeric editor and its exponent sign semantics.
    for (int i = 0; i < length; i++)
        if (text[i] == 24) return -1;
    char decimal = flags.f.decimal_point ? '.' : ',';
    char separator = flags.f.decimal_point ? ',' : '.';
    char result[104];
    int len = 0, fractional_digits = -1;
    for (int i = 0; i < length; i++) {
        char c = text[i];
        if (c == separator) continue;
        if (c == decimal) {
            result[len++] = ':';
            fractional_digits = 0;
        } else {
            if (fractional_digits == 2) result[len++] = ':';
            if (fractional_digits == 4) result[len++] = '.';
            result[len++] = c;
            if (fractional_digits >= 0) fractional_digits++;
        }
        if (len > buflen || len >= (int) sizeof(result) - 2) return -1;
    }
    memcpy(buf, result, len);
    return len;
}

bool custom_time_draw_entry(int row) {
    if (!mode_number_entry || !custom_time_active()) return false;
    char input[104], line[128];
    int input_length = custom_time_entry(cmdline, cmdline_length, input, sizeof(input));
    if (input_length < 0) return false;
    int prefix_length = 0;
    xlabel2buf(line, sizeof(line), &prefix_length);
    int length = prefix_length;
    string2buf(line, sizeof(line), &length, input, input_length);
    char2buf(line, sizeof(line), &length, '_');
    clear_row(row);
    if (length <= 22)
        draw_string(0, row, line, length);
    else {
        draw_char(0, row, 26);
        draw_string(1, row, line + length - 21, 21);
    }
#ifdef ARM
    thell_edit_number(line, prefix_length, input, input_length);
#endif
    return true;
}

void custom_time_input(const char *text, int length, phloat *hours) {
    if (!custom_time_active()) return;
    char decimal = flags.f.decimal_point ? '.' : ',';
    char separator = flags.f.decimal_point ? ',' : '.';
    char whole[100], fraction[100];
    int whole_length = 0, fraction_length = 0;
    bool negative = false, after_decimal = false;
    for (int i = 0; i < length; i++) {
        char c = text[i];
        // Exponent input retains the ordinary numeric editor's hours value.
        if (c == 24) return;
        if (c == '-') negative = true;
        else if (c == separator) continue;
        else if (c == decimal) after_decimal = true;
        else if (c >= '0' && c <= '9') {
            if (after_decimal) fraction[fraction_length++] = c;
            else whole[whole_length++] = c;
        }
    }
    if (whole_length == 0) whole[whole_length++] = '0';
    phloat value;
    if (string2phloat(whole, whole_length, &value) != 0) return;
    int minutes = (fraction_length > 0 ? fraction[0] - '0' : 0) * 10
                + (fraction_length > 1 ? fraction[1] - '0' : 0);
    char seconds_text[100];
    int seconds_length = 0;
    seconds_text[seconds_length++] = fraction_length > 2 ? fraction[2] : '0';
    seconds_text[seconds_length++] = fraction_length > 3 ? fraction[3] : '0';
    if (fraction_length > 4) {
        seconds_text[seconds_length++] = decimal;
        memcpy(seconds_text + seconds_length, fraction + 4, fraction_length - 4);
        seconds_length += fraction_length - 4;
    }
    phloat seconds;
    if (string2phloat(seconds_text, seconds_length, &seconds) != 0) return;
    value += phloat(minutes) / 60 + seconds / 3600;
    *hours = negative ? -value : value;
}

// Read decimal digits and their exponent at the requested display precision.
static int decimal_digits(phloat value, char *digits, int *exponent, int precision) {
    char scientific[100];
    int n = phloat2string(value, scientific, sizeof(scientific), 0,
                         precision - 1, 1, 0, precision);
    int count = 0, pos = 0;
    for (; pos < n && scientific[pos] != 24 && scientific[pos] != 'e'; pos++)
        if (scientific[pos] >= '0' && scientific[pos] <= '9')
            digits[count++] = scientific[pos];
    *exponent = 0;
    bool exponent_negative = false;
    if (++pos < n && scientific[pos] == '-') {
        exponent_negative = true;
        pos++;
    }
    for (; pos < n; pos++)
        if (scientific[pos] >= '0' && scientific[pos] <= '9')
            *exponent = *exponent * 10 + scientific[pos] - '0';
    if (exponent_negative) *exponent = -*exponent;
    return count;
}

// Extract whole-hour digits without converting a potentially huge count to int.
static int decimal_digit(const char *digits, int length, int exponent, int power) {
    int index = exponent - power;
    return index < 0 || index >= length ? 0 : digits[index] - '0';
}

int custom_time_format(const vartype *value, char *buf, int buflen) {
    if (!custom_time_active() || value->type != TYPE_REAL) return -1;
    phloat x = ((const vartype_real *) value)->x;
    if (p_isinf(x) || p_isnan(x)) return -1;
    bool negative = x < 0;
    if (negative) x = -x;
    char digits[MAX_MANT_DIGITS];
    int exponent;
    int count = decimal_digits(x, digits, &exponent, MAX_MANT_DIGITS);
    int hour_length = exponent < 0 ? 1 : exponent + 1;
    char hours[100], years[100];
    if (!count || hour_length >= (int) sizeof(hours) - 1 || hour_length > buflen)
        return -1;
    for (int i = 0; i < hour_length; i++)
        hours[i] = '0' + decimal_digit(digits, count, exponent, hour_length - i - 1);

    // Divide the decimal hour count by a fixed 365-day year. Only the remaining
    // days and hours need native integers, even for very large durations.
    const int hours_per_year = 365 * 24;
    int hour = 0, year_length = 0;
    for (int i = 0; i < hour_length; i++) {
        hour = hour * 10 + hours[i] - '0';
        int digit = hour / hours_per_year;
        hour %= hours_per_year;
        if (digit || year_length) years[year_length++] = '0' + digit;
    }
    int days = hour / 24;
    hour %= 24;

    // Keep years and days out of the packed HMS value to preserve subsecond
    // precision. Use the ordinary 12-digit display precision for the HMS text.
    phloat hms;
    mappable_to_hms(hour + (x - floor(x)), &hms);
    count = decimal_digits(hms, digits, &exponent, 12);
    hour = decimal_digit(digits, count, exponent, 1) * 10
         + decimal_digit(digits, count, exponent, 0);
    // Read the HMS decimal digits directly, so extracting fields does not
    // introduce another floating-point conversion at a display boundary.
    int minutes = decimal_digit(digits, count, exponent, -1) * 10
                + decimal_digit(digits, count, exponent, -2);
    int seconds = decimal_digit(digits, count, exponent, -3) * 10
                + decimal_digit(digits, count, exponent, -4);
    int hundredths = decimal_digit(digits, count, exponent, -5) * 10
                   + decimal_digit(digits, count, exponent, -6);
    int ticks;
    if (time_precision == 0)
        ticks = (minutes + (!time_truncate && seconds >= 30 ? 1 : 0)) * 6000;
    else if (time_precision == 1)
        ticks = (minutes * 60 + seconds
                + (!time_truncate && hundredths >= 50 ? 1 : 0)) * 100;
    else
        ticks = (minutes * 60 + seconds) * 100 + hundredths
                + (!time_truncate && decimal_digit(digits, count, exponent, -7) >= 5 ? 1 : 0);

    hour += ticks / 360000;
    ticks %= 360000;
    days += hour / 24;
    hour %= 24;
    int carry = days / 365;
    days %= 365;
    for (int i = year_length - 1; i >= 0 && carry; i--) {
        int digit = years[i] - '0' + carry;
        years[i] = '0' + digit % 10;
        carry = digit / 10;
    }
    if (carry) {
        memmove(years + 1, years, year_length++);
        years[0] = '1';
    }
    years[year_length] = 0;
    if (!year_length && !days && !hour && !ticks) negative = false;
    char result[128];
    int len = 0;
    if (negative) result[len++] = '-';
    if (year_length)
        len += snprintf(result + len, sizeof(result) - len, "%sy %3dd ", years, days);
    else if (days)
        len += snprintf(result + len, sizeof(result) - len, "%dd ", days);
    len += snprintf(result + len, sizeof(result) - len, "%02d:%02d", hour, ticks / 6000);
    if (time_precision > 0)
        len += snprintf(result + len, sizeof(result) - len, ":%02d", ticks / 100 % 60);
    if (time_precision > 1)
        len += snprintf(result + len, sizeof(result) - len, ".%02d", ticks % 100);
    if (len > buflen) return -1;
    memcpy(buf, result, len);
    return len;
}

// Return a 4-bit value: bits 0-1 store precision + 1, bit 2 activates time mode,
// and bit 3 selects truncation instead of rounding the displayed duration.
unsigned char time_display_save() {
    // An older zero field means HMS precision, rounding, and time mode off.
    return (time_precision + 1) | (mode_appmenu == MENU_TIME ? 4 : 0)
            | (time_truncate ? 8 : 0);
}

// Restore the setting from the low 4 bits; higher bits are ignored.
void time_display_restore(unsigned char data) {
    int precision = data & 3;
    time_precision = precision == 0 ? 1 : precision - 1;
    time_truncate = (data & 8) != 0;
    // Keep any plain-menu overlay and avoid replacing another saved application.
    if (mode_appmenu == MENU_NONE || mode_appmenu == MENU_TIME) {
        mode_appmenu = (data & 4) ? MENU_TIME : MENU_NONE;
        if (mode_appmenu == MENU_TIME) set_base(10);
    }
}
