/*
 * Program-editor navigation tools for the local Free42 fork.
 * This program is free software under the GNU General Public License, version 2.
 */
#include "core_program_tools_custom.h"
#include <string.h>
#include "core_main.h"
#include "core_keydown.h"
#include "core_globals.h"
#include "core_display.h"
#include "core_helpers.h"
#include "core_history_custom.h"
#include "shell.h"

extern int no_menu_key_this_time;

namespace {

enum find_input_mode { FIND_INACTIVE, FIND_COMMAND, FIND_TEXT };
static find_input_mode find_input = FIND_INACTIVE;
static bool find_reverse = false;

struct find_query {
    bool reverse;
    int command;
    int length;
    char text[22];
};
static find_query last_find;

static const char *find_prompt() {
    // Free42's character set encodes ↑ as 0x5e and ↓ as 0x0e.
    return find_reverse ? "FIND^" : "FIND\016";
}

static int stored_command_at(int position) {
    const unsigned char *text = prgms[current_prgm].text + position;
    return text[0] | ((text[1] & 112) << 4);
}

static void clear_find_menus() {
    // FIND starts at the home menu, so menus opened to choose a query are temporary.
    set_menu(MENULEVEL_APP, MENU_NONE);
}

static void finish_find_input() {
    if (find_input == FIND_TEXT)
        mode_command_entry = false;
    find_input = FIND_INACTIVE;
    find_reverse = false;
    pending_command = CMD_NONE;
    clear_find_menus();
}

static bool editor_action_ready(bool *enqueued, int *repeat) {
    *enqueued = false;
    *repeat = 0;
    if (!flags.f.prgm_mode || mode_running || mode_getkey
            || mode_interruptible != NULL || mode_clall)
        return false;
    if (find_input != FIND_INACTIVE)
        finish_find_input();
    // An unfinished instruction is not a selected stored program line.
    if (mode_number_entry || mode_alpha_entry || mode_command_entry || core_menu()) {
        squeak();
        return false;
    }
    no_keystrokes_yet = false;
    pending_command = CMD_NONE;
    flags.f.message = flags.f.two_line_message = 0;
    return true;
}

static void move_editor(int program, int position) {
    current_prgm = program;
    pc = position;
    clear_all_rtns();
    prgm_highlight_row = 1;
    redisplay();
}

static void find_not_found() {
    clear_row(0);
    draw_string(0, 0, "Not Found", 9);
    flags.f.message = true;
    flags.f.two_line_message = false;
    redisplay();
}

static bool contains_text(const char *text, int length, const char *query, int query_length) {
    for (int i = 0; i <= length - query_length; i++)
        if (memcmp(text + i, query, query_length) == 0)
            return true;
    return false;
}

static bool line_matches_text(int position, const char *query, int length) {
    int type = prgms[current_prgm].text[position + 1] & 15;
    if (type != ARGTYPE_STR && type != ARGTYPE_IND_STR && type != ARGTYPE_XSTR)
        return false;
    int command;
    arg_struct arg = {};
    int4 next = position;
    // Decode only textual operands: decoding NUMBER can normalize its saved decimal text.
    get_next_command(&next, &command, &arg, 0, NULL);
    const char *text = type == ARGTYPE_XSTR ? arg.val.xstr : arg.val.text;
    return contains_text(text, arg.length, query, length);
}

static void find_match(bool reverse, int command, const char *query = NULL, int length = 0) {
    int first = pc < 0 ? 0 : pc;
    if (!reverse && pc >= 0)
        first += get_command_length(current_prgm, pc);
    const int end = prgms[current_prgm].size;
    if (first >= end)
        first = 0;
    int position = first;
    int match = -1;
    // Reverse search visits the current line first and retains the last match
    // in one forward cycle, so variable-length instructions are never read backward.
    do {
        if (query != NULL ? line_matches_text(position, query, length)
                          : stored_command_at(position) == command) {
            match = position;
            if (!reverse)
                break;
        }
        position += get_command_length(current_prgm, position);
        if (position >= end)
            position = 0;
    } while (position != first);
    if (match >= 0)
        move_editor(current_prgm, match);
    else
        find_not_found();
}

static bool repeat_find(bool *enqueued, int *repeat) {
    // AGAIN must leave an unconfirmed FIND query open instead of cancelling it.
    if (find_input != FIND_INACTIVE) {
        *enqueued = false;
        *repeat = 0;
        squeak();
        return false;
    }
    if (editor_action_ready(enqueued, repeat))
        find_match(last_find.reverse, last_find.command,
            last_find.length > 0 ? last_find.text : NULL, last_find.length);
    return false;
}

static void search_and_remember(bool reverse, int command, const char *query = NULL, int length = 0) {
    last_find.reverse = reverse;
    last_find.command = command;
    last_find.length = length;
    if (length > 0)
        memcpy(last_find.text, query, length);
    custom_history_program_action(repeat_find);
    find_match(reverse, command, query, length);
}

} // namespace

void custom_program_step(bool reverse) {
    const bool wrapped = reverse ? current_prgm == 0 : current_prgm == prgms_count - 1;
    current_prgm = wrapped ? (reverse ? prgms_count - 1 : 0)
                           : current_prgm + (reverse ? -1 : 1);
    pc = -1;
    prgm_highlight_row = 0;
    if (wrapped) {
        shell_message(reverse ? "Back to Last" : "Back to First");
        // The modal consumed a dismissal key; do not resume the original key's repeat.
        repeating = 0;
    }
}

bool core_program_label_jump(bool *enqueued, int *repeat) {
    custom_history_scope history;
    *enqueued = false;
    *repeat = 0;
    if (!flags.f.prgm_mode || core_menu() || mode_running
            || mode_interruptible != NULL || mode_getkey || mode_command_entry)
        return false;

    // The normal command path commits an edited number and saves the position
    // that EXIT must restore, without inserting a GTO instruction in the program.
    bool running = core_keydown_command_id(CMD_GTODOT, enqueued, repeat);
    if (!*enqueued && mode_command_entry && incomplete_command == CMD_GTODOT) {
        set_catalog_menu(CATSECT_PGM_ONLY);
        redisplay();
    }
    return running;
}

bool core_program_follow(bool *enqueued, int *repeat) {
    custom_history_scope history;
    if (!editor_action_ready(enqueued, repeat))
        return false;
    if (pc < 0 || pc >= prgms[current_prgm].size) {
        display_error(ERR_INVALID_TYPE);
        redisplay();
        return false;
    }
    int command = stored_command_at(pc);
    if (command != CMD_GTO && command != CMD_XEQ) {
        display_error(ERR_INVALID_TYPE);
        redisplay();
        return false;
    }
    arg_struct arg = {};
    int4 next = pc;
    get_next_command(&next, &command, &arg, 0, NULL);
    int program = current_prgm;
    int4 position = -2;
    if (arg.type == ARGTYPE_STR)
        find_global_label(&arg, &program, &position);
    else if (arg.type == ARGTYPE_NUM || arg.type == ARGTYPE_LCLBL)
        position = find_local_label(&arg);
    else {
        // Follow literal named, numeric, and letter labels, without resolving
        // indirect operands from runtime register values.
        display_error(ERR_INVALID_TYPE);
        redisplay();
        return false;
    }
    if (position < 0) {
        display_error(ERR_LABEL_NOT_FOUND);
        redisplay();
    } else
        move_editor(program, position);
    return false;
}

bool core_program_find(bool reverse, bool *enqueued, int *repeat) {
    custom_history_scope history;
    if (!editor_action_ready(enqueued, repeat))
        return false;
    find_reverse = reverse;
    find_input = FIND_COMMAND;
    set_menu(MENULEVEL_PLAIN, MENU_BLANK);
    redisplay();
    return false;
}

bool custom_program_find_keydown(int shift, int key) {
    if (find_input == FIND_INACTIVE)
        return false;
    if (!flags.f.prgm_mode) {
        custom_program_tools_reset();
        return false;
    }
    if (shift && (key == KEY_RUN || key == KEY_EXIT)) {
        finish_find_input();
        return false;
    }
    if (find_input == FIND_TEXT)
        return false;
    if (key == KEY_EXIT || (!shift && key == KEY_BSP)) {
        finish_find_input();
        redisplay();
        return true;
    }
    if (shift && key == KEY_ENTER) {
        clear_find_menus();
        find_input = FIND_TEXT;
        incomplete_saved_pc = pc;
        incomplete_saved_highlight_row = prgm_highlight_row;
        start_incomplete_command(CMD_XSTR);
        return true;
    }
    const int menu = get_front_menu();
    if (menu == MENU_BLANK && !no_menu_key_this_time && find_menu_key(key) >= 0)
        return true;
    if ((!shift && ((key >= KEY_0 && key <= KEY_DOT)
            || (key >= KEY_1 && key <= KEY_3) || (key >= KEY_4 && key <= KEY_6)
            || (key >= KEY_7 && key <= KEY_9) || key == KEY_E))
            || ((menu == MENU_NONE || menu == MENU_BLANK)
                && (key == KEY_UP || key == KEY_DOWN))) {
        squeak();
        return true;
    }
    return false;
}

bool custom_program_find_command(int command) {
    if (find_input != FIND_COMMAND)
        return false;
    const bool reverse = find_reverse;
    finish_find_input();
    // In the editor, the R/S key inserts STOP rather than the RUN pseudo-command.
    if (command == CMD_RUN)
        command = CMD_STOP;
    if (command >= 0 && command < CMD_SENTINEL)
        search_and_remember(reverse, command);
    else
        redisplay();
    return true;
}

bool custom_program_find_finish_entry() {
    if (find_input != FIND_TEXT)
        return false;
    const bool accepted = pending_command == CMD_XSTR;
    const bool reverse = find_reverse;
    char query[22];
    const int length = accepted ? incomplete_length : 0;
    if (length > 0)
        memcpy(query, incomplete_str, length);
    finish_find_input();
    if (length > 0)
        search_and_remember(reverse, CMD_NONE, query, length);
    else
        redisplay();
    return true;
}

bool custom_program_find_draw_incomplete(int row) {
    if (find_input != FIND_TEXT)
        return false;
    char text[32];
    int length = 0;
    const char *prompt = find_prompt();
    string2buf(text, sizeof(text), &length, prompt, strlen(prompt));
    string2buf(text, sizeof(text), &length, " \"", 2);
    string2buf(text, sizeof(text), &length, incomplete_str, incomplete_length);
    char2buf(text, sizeof(text), &length, '_');
    clear_row(row);
    if (length <= 22)
        draw_string(0, row, text, length);
    else {
        draw_char(0, row, 26);
        draw_string(1, row, text + length - 21, 21);
    }
    return true;
}

void custom_program_find_draw_prompt() {
    if (find_input != FIND_COMMAND || !flags.f.prgm_mode || flags.f.message)
        return;
    const int row = core_menu() ? 0 : prgm_highlight_row;
    clear_row(row);
    const char *prompt = find_prompt();
    draw_string(0, row, prompt, strlen(prompt));
    draw_string(strlen(prompt), row, " _", 2);
}

void custom_program_tools_reset() {
    find_input = FIND_INACTIVE;
    find_reverse = false;
}
