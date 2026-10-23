/* Session-only command history and snapshots of program edits/positions.
 * Variables, settings, and the execution engine are outside normal-mode history.
 */
#include <stdlib.h>
#include <string.h>
#include <new>

#include "core_history_custom.h"
#include "core_globals.h"
#include "core_variables.h"
#include "core_main.h"
#include "core_keydown.h"
#include "core_display.h"
#include "core_helpers.h"

// These handlers replace X or consume Y/X for one result. This opt-in table is
// deliberately independent of command_spec::argcount, which describes validation.
#define UNDO_COMMANDS(X) \
    X(CMD_CLX, 1) \
    X(CMD_CHS, 1) \
    X(CMD_SIN, 1) \
    X(CMD_COS, 1) \
    X(CMD_TAN, 1) \
    X(CMD_ASIN, 1) \
    X(CMD_ACOS, 1) \
    X(CMD_ATAN, 1) \
    X(CMD_LOG, 1) \
    X(CMD_10_POW_X, 1) \
    X(CMD_LN, 1) \
    X(CMD_E_POW_X, 1) \
    X(CMD_SQRT, 1) \
    X(CMD_SQUARE, 1) \
    X(CMD_INV, 1) \
    X(CMD_TO_DEG, 1) \
    X(CMD_TO_RAD, 1) \
    X(CMD_TO_HR, 1) \
    X(CMD_TO_HMS, 1) \
    X(CMD_IP, 1) \
    X(CMD_FP, 1) \
    X(CMD_RND, 1) \
    X(CMD_ABS, 1) \
    X(CMD_SIGN, 1) \
    X(CMD_FACT, 1) \
    X(CMD_GAMMA, 1) \
    X(CMD_BASECHS, 1) \
    X(CMD_NOT, 1) \
    X(CMD_TO_INCH, 1) \
    X(CMD_TO_METER, 1) \
    X(CMD_PERCENT, 1) \
    X(CMD_DIV, 2) \
    X(CMD_MUL, 2) \
    X(CMD_SUB, 2) \
    X(CMD_ADD, 2) \
    X(CMD_Y_POW_X, 2) \
    X(CMD_MOD, 2) \
    X(CMD_COMB, 2) \
    X(CMD_PERM, 2) \
    X(CMD_BASEADD, 2) \
    X(CMD_BASESUB, 2) \
    X(CMD_BASEMUL, 2) \
    X(CMD_BASEDIV, 2) \
    X(CMD_AND, 2) \
    X(CMD_OR, 2) \
    X(CMD_XOR, 2) \
    X(CMD_ROTXY, 2)

#define EXECUTION_HISTORY_EXCLUDED_COMMANDS(X) \
    X(CMD_FIX) \
    X(CMD_SCI) \
    X(CMD_ENG) \
    X(CMD_ALL) \
    X(CMD_DEG) \
    X(CMD_RAD) \
    X(CMD_GRAD) \
    X(CMD_RECT) \
    X(CMD_POLAR) \
    X(CMD_QUIET) \
    X(CMD_CPXRES) \
    X(CMD_REALRES) \
    X(CMD_RDXDOT) \
    X(CMD_RDXCOMMA) \
    X(CMD_SF) \
    X(CMD_CF) \
    X(CMD_GTODOT) \
    X(CMD_GTODOTDOT) \
    X(CMD_CUSTOM) \
    X(CMD_MENU) \
    X(CMD_VIEW) \
    X(CMD_AVIEW) \
    X(CMD_TIME_HM) \
    X(CMD_TIME_HMS) \
    X(CMD_TIME_HMSS) \
    X(CMD_TIME_ROUND) \
    X(CMD_TIME_TRUNC) \
    X(CMD_TIME_CALC) \
    X(CMD_NULL) \
    X(CMD_SILENT_OFF) \
    X(CMD_SILENT_ON)

// Edits made outside a captured program-editing operation invalidate old snapshots.
#define PROGRAM_HISTORY_INVALIDATING_COMMANDS(X) \
    X(CMD_CLP) \
    X(CMD_CLALLb) \
    X(CMD_DEL) \
    X(CMD_GTODOTDOT)

// Keep history-owned allocation failures separate from allocations made by
// command handlers, so tests can exercise recovery without rewriting sources.
namespace history_memory {
#ifdef FREE42_HISTORY_TEST
void *allocate(size_t size);
void *allocate_zeroed(size_t count, size_t size);
void *resize(void *memory, size_t size);
vartype *duplicate(const vartype *value);
#else
inline void *allocate(size_t size) { return malloc(size); }
inline void *allocate_zeroed(size_t count, size_t size) { return calloc(count, size); }
inline void *resize(void *memory, size_t size) { return realloc(memory, size); }
inline vartype *duplicate(const vartype *value) { return dup_vartype(value); }
#endif
}

namespace {

const int history_limit = 16;
const size_t history_budget = 64 * 1024;
const size_t action_budget = 8 * 1024;
static unsigned long snapshot_serial;

struct history_snapshot {
    bool program;
    vartype *values[2];
    vartype *duplicate_t;
    int consumed, depth;
    bool undone, carry_recorded, carry;
    vartype *last;
    prgm_struct *programs;
    int program_count;
    label_struct *program_labels;
    int label_count;
    int selected_program;
    int4 position;
    int highlight;
    bool disable_lift;
    bool lift_disabled;
    char number_mode;
    bool alpha_mode;
    bool big_stack;
    bool numeric_input;
    int line_length;
    int line_row;
    char line[100];
    phloat number;
    int string_length;
    char string[15];
    size_t bytes;
    unsigned long serial;

    history_snapshot() : program(false), values(), duplicate_t(NULL), consumed(0), depth(0),
        undone(false), carry_recorded(false), carry(false), last(NULL),
        programs(NULL), program_count(0),
        program_labels(NULL), label_count(0), selected_program(0), position(-1),
        highlight(0), disable_lift(false), lift_disabled(false), number_mode(0),
        alpha_mode(false), big_stack(false), numeric_input(false), line_length(0), line_row(0),
        string_length(0), bytes(sizeof(history_snapshot)), serial(++snapshot_serial) {}

    ~history_snapshot() {
        free_vartype(values[0]); free_vartype(values[1]); free_vartype(duplicate_t);
        free_vartype(last);
        for (int i = 0; i < program_count; i++) free(programs[i].text);
        free(programs);
        free(program_labels);
    }
};

static history_snapshot *new_snapshot() {
    void *memory = history_memory::allocate(sizeof(history_snapshot));
    return memory == NULL ? NULL : new (memory) history_snapshot();
}

static void free_snapshot(history_snapshot *s) {
    if (s != NULL) { s->~history_snapshot(); free(s); }
}

// Account shared arrays conservatively, so retained COW data cannot grow without bound.
static size_t value_bytes(const vartype *value) {
    if (value == NULL) return 0;
    switch (value->type) {
        case TYPE_REAL: return sizeof(vartype_real);
        case TYPE_COMPLEX: return sizeof(vartype_complex);
        case TYPE_STRING: return sizeof(vartype_string) + ((const vartype_string *) value)->length;
        case TYPE_REALMATRIX: {
            const vartype_realmatrix *v = (const vartype_realmatrix *) value;
            size_t cells = size_t(v->rows) * v->columns;
            size_t bytes = sizeof(*v) + sizeof(realmatrix_data) + cells * (sizeof(phloat) + 1);
            for (size_t i = 0; i < cells && bytes <= history_budget; i++) {
                if (v->array->is_string[i] == 2) {
                    const char *text;
                    int4 length;
                    get_matrix_string(v, i, &text, &length);
                    bytes += sizeof(int4) + length;
                }
            }
            return bytes;
        }
        case TYPE_COMPLEXMATRIX: {
            const vartype_complexmatrix *v = (const vartype_complexmatrix *) value;
            return sizeof(*v) + sizeof(complexmatrix_data) + size_t(v->rows) * v->columns * sizeof(phloat) * 2;
        }
        case TYPE_LIST: {
            const vartype_list *v = (const vartype_list *) value;
            size_t bytes = sizeof(*v) + sizeof(list_data) + size_t(v->size) * sizeof(vartype *);
            for (int4 i = 0; i < v->size && bytes <= history_budget; i++) bytes += value_bytes(v->array->data[i]);
            return bytes;
        }
        default: return history_budget + 1;
    }
}

static bool same_value(const vartype *a, const vartype *b) {
    if (a == NULL || b == NULL) return a == b;
    if (a->type != b->type) return false;
    switch (a->type) {
        case TYPE_REAL:
            return !memcmp(&((const vartype_real *) a)->x, &((const vartype_real *) b)->x, sizeof(phloat));
        case TYPE_COMPLEX:
            return !memcmp(&((const vartype_complex *) a)->re, &((const vartype_complex *) b)->re, sizeof(phloat))
                && !memcmp(&((const vartype_complex *) a)->im, &((const vartype_complex *) b)->im, sizeof(phloat));
        case TYPE_STRING: {
            const vartype_string *x = (const vartype_string *) a, *y = (const vartype_string *) b;
            return x->length == y->length && !memcmp(x->txt(), y->txt(), x->length);
        }
        case TYPE_REALMATRIX: {
            const vartype_realmatrix *x = (const vartype_realmatrix *) a, *y = (const vartype_realmatrix *) b;
            return x->rows == y->rows && x->columns == y->columns && x->array == y->array;
        }
        case TYPE_COMPLEXMATRIX: {
            const vartype_complexmatrix *x = (const vartype_complexmatrix *) a, *y = (const vartype_complexmatrix *) b;
            return x->rows == y->rows && x->columns == y->columns && x->array == y->array;
        }
        case TYPE_LIST: {
            const vartype_list *x = (const vartype_list *) a, *y = (const vartype_list *) b;
            return x->size == y->size && x->array == y->array;
        }
        default: return false;
    }
}

static bool copy_program_data(history_snapshot *s,
        const prgm_struct *source_programs, int source_program_count,
        const label_struct *source_labels, int source_label_count) {
    s->program = true;
    s->bytes += sizeof(prgm_struct) * source_program_count + sizeof(label_struct) * source_label_count;
    for (int i = 0; i < source_program_count; i++) s->bytes += source_programs[i].size;
    if (s->bytes > history_budget) return false;
    s->programs = (prgm_struct *) history_memory::allocate_zeroed(source_program_count, sizeof(prgm_struct));
    if (source_program_count && s->programs == NULL) return false;
    s->program_count = source_program_count;
    for (int i = 0; i < source_program_count; i++) {
        s->programs[i] = source_programs[i];
        s->programs[i].capacity = s->programs[i].size;
        s->programs[i].text = (unsigned char *) history_memory::allocate(s->programs[i].size);
        if (s->programs[i].text == NULL) return false;
        memcpy(s->programs[i].text, source_programs[i].text, s->programs[i].size);
    }
    s->program_labels = (label_struct *) history_memory::allocate(sizeof(label_struct) * source_label_count);
    if (source_label_count && s->program_labels == NULL) return false;
    if (source_label_count)
        memcpy(s->program_labels, source_labels, sizeof(label_struct) * source_label_count);
    s->label_count = source_label_count;
    return true;
}

static history_snapshot *capture_snapshot() {
    if (stack == NULL || lastx == NULL || prgms == NULL || prgms_count == 0) return NULL;
    history_snapshot *s = new_snapshot();
    if (s == NULL) return NULL;
    if (!copy_program_data(s, prgms, prgms_count, labels, labels_count)) {
        free_snapshot(s); return NULL;
    }
    s->selected_program = current_prgm;
    s->position = mode_command_entry ? incomplete_saved_pc : pc;
    s->highlight = mode_command_entry ? incomplete_saved_highlight_row : prgm_highlight_row;
    s->disable_lift = mode_disable_stack_lift;
    s->lift_disabled = flags.f.stack_lift_disable;
    s->number_mode = mode_number_entry;
    s->alpha_mode = mode_alpha_entry;
    s->big_stack = flags.f.big_stack;
    s->numeric_input = flags.f.numeric_data_input;
    s->line_length = cmdline_length;
    s->line_row = cmdline_row;
    memcpy(s->line, cmdline, sizeof(s->line));
    s->number = entered_number;
    s->string_length = entered_string_length;
    memcpy(s->string, entered_string, sizeof(s->string));
    return s;
}

static bool same_program_text(const history_snapshot *s, int i) {
    if (i >= s->program_count || s->programs[i].size != prgms[i].size) return false;
    int4 pos = 0;
    while (pos < prgms[i].size) {
        int length = get_command_length(i, pos);
        const unsigned char *a = s->programs[i].text + pos, *b = prgms[i].text + pos;
        if (memcmp(a, b, 2)) return false;
        int command = b[0] | ((b[1] & 112) << 4), type = b[1] & 15;
        // Local branch caches are derived execution metadata, not edits.
        int skip = (command == CMD_GTO || command == CMD_XEQ)
            && (type == ARGTYPE_NUM || type == ARGTYPE_STK || type == ARGTYPE_LCLBL) ? 6 : 2;
        if (length > skip && memcmp(a + skip, b + skip, length - skip)) return false;
        pos += length;
    }
    return true;
}

static bool same_programs(const history_snapshot *s) {
    if (s->program_count != prgms_count) return false;
    for (int i = 0; i < prgms_count; i++) if (!same_program_text(s, i)) return false;
    return true;
}

static bool same_snapshot(const history_snapshot *s) {
    if (s->program) {
        int4 position = mode_command_entry ? incomplete_saved_pc : pc;
        int highlight = mode_command_entry ? incomplete_saved_highlight_row : prgm_highlight_row;
        return same_programs(s) && s->selected_program == current_prgm && s->position == position
            && s->highlight == highlight && s->number_mode == mode_number_entry
            && s->alpha_mode == mode_alpha_entry
            && (!s->number_mode || (s->line_length == cmdline_length
                && !memcmp(s->line, cmdline, s->line_length)))
            && (!s->alpha_mode || (s->string_length == entered_string_length
                && !memcmp(s->string, entered_string, s->string_length)));
    }
    return true;
}

struct history_lane {
    history_snapshot *undo[history_limit];
    history_snapshot *redo[history_limit];
    int undo_count, redo_count;
    bool unsupported;
};
struct repeat_command_payload {
    int command;
    arg_struct arg;
    char *text;
    char *number;
    repeat_command_payload(int command, const arg_struct &arg, char *text, char *number)
        : command(command), arg(arg), text(text), number(number) {}
    ~repeat_command_payload() { free(text); free(number); }
    repeat_command_payload(const repeat_command_payload &) = delete;
    repeat_command_payload &operator=(const repeat_command_payload &) = delete;
};

enum class repeat_kind { none, command, action };
typedef bool (*repeat_action)(bool *enqueued, int *repeat);

struct repeated_command {
    repeat_kind kind;
    union storage {
        repeat_command_payload command;
        repeat_action action;
        // Construct and destroy command explicitly because it contains a nontrivial phloat object.
        storage() {}
        ~storage() {}
    } data;
    repeated_command() : kind(repeat_kind::none), data() {}
    ~repeated_command();
    repeated_command(const repeated_command &) = delete;
    repeated_command &operator=(const repeated_command &) = delete;
};

struct history_context {
    history_lane lanes[2];
    int scope_depth;
    history_snapshot *before;
    bool suppressed;
    bool scope_program;
    int execute_depth;
    unsigned long generation, barrier_serial;
    bool observed_big_stack;
    repeated_command last_command[2], inflight;
    history_context() : lanes(), scope_depth(0), before(NULL), suppressed(false), scope_program(false),
        execute_depth(0), generation(++snapshot_serial), barrier_serial(0),
        observed_big_stack(flags.f.big_stack),
        last_command(), inflight() {}
};
static history_context *history;

static bool ensure_history() {
    if (history != NULL) return true;
    void *memory = history_memory::allocate(sizeof(history_context));
    if (memory == NULL) return false;
    history = new (memory) history_context();
    return true;
}


static void clear_list(history_snapshot **list, int &count) {
    while (count) free_snapshot(list[--count]);
}

static void clear_lane(history_lane &lane, bool unsupported) {
    clear_list(lane.undo, lane.undo_count);
    clear_list(lane.redo, lane.redo_count);
    lane.unsupported = unsupported;
}

static void append_snapshot(history_snapshot **list, int &count, history_snapshot *s) {
    if (count == history_limit) {
        free_snapshot(list[0]);
        memmove(list, list + 1, sizeof(history_snapshot *) * --count);
    }
    list[count++] = s;
    // Apply one retained-memory budget to both modes and both undo directions.
    for (;;) {
        size_t bytes = 0;
        history_snapshot **oldest = NULL;
        int *oldest_count = NULL;
        for (int mode = 0; mode < 2; mode++) {
            history_snapshot **lists[] = { history->lanes[mode].undo, history->lanes[mode].redo };
            int *counts[] = { &history->lanes[mode].undo_count, &history->lanes[mode].redo_count };
            for (int direction = 0; direction < 2; direction++) {
                for (int i = 0; i < *counts[direction]; i++) bytes += lists[direction][i]->bytes;
                if (*counts[direction] && (oldest == NULL
                        || lists[direction][0]->serial < oldest[0]->serial)) {
                    oldest = lists[direction]; oldest_count = counts[direction];
                }
            }
        }
        if (bytes <= history_budget || oldest == NULL) break;
        free_snapshot(oldest[0]);
        memmove(oldest, oldest + 1, sizeof(history_snapshot *) * --*oldest_count);
    }
}

static bool is_excluded_from_execution_history(int command) {
    switch (command) {
#define EXCLUDED_CASE(id) case id:
        EXECUTION_HISTORY_EXCLUDED_COMMANDS(EXCLUDED_CASE)
#undef EXCLUDED_CASE
            return true;
        default:
            return false;
    }
}

static bool invalidates_program_history(int command) {
    switch (command) {
#define PROGRAM_HISTORY_CASE(id) case id:
        PROGRAM_HISTORY_INVALIDATING_COMMANDS(PROGRAM_HISTORY_CASE)
#undef PROGRAM_HISTORY_CASE
            return true;
        default:
            return false;
    }
}

static void finish_program_history_command(int command, int result, bool recorded_edit) {
    if (result == ERR_NONE && invalidates_program_history(command)
            && (!recorded_edit || !flags.f.prgm_mode))
        clear_lane(history->lanes[1], true);
}

static int consumed_values(int command) {
    switch (command) {
#define UNDO_CASE(id, count) case id: return count;
        UNDO_COMMANDS(UNDO_CASE)
#undef UNDO_CASE
        default:
            return 0;
    }
}

static history_snapshot *capture_command(int consumed) {
    if (stack == NULL || lastx == NULL || sp + 1 < consumed
            || (!flags.f.big_stack && sp != 3)) return NULL;
    history_snapshot *s = new_snapshot();
    if (s == NULL) return NULL;
    s->consumed = consumed; s->depth = sp + 1; s->big_stack = flags.f.big_stack;
    s->disable_lift = mode_disable_stack_lift;
    s->lift_disabled = flags.f.stack_lift_disable;
    s->carry_recorded = baseapp != 0;
    s->carry = mode_carry;
    s->bytes += value_bytes(lastx);
    for (int i = 0; i < consumed; i++) s->bytes += value_bytes(stack[sp + 1 - consumed + i]);
    if (s->bytes > action_budget) { free_snapshot(s); return NULL; }
    s->last = history_memory::duplicate(lastx);
    if (s->last == NULL) { free_snapshot(s); return NULL; }
    for (int i = 0; i < consumed; i++) {
        s->values[i] = history_memory::duplicate(stack[sp + 1 - consumed + i]);
        if (s->values[i] == NULL) { free_snapshot(s); return NULL; }
    }
    return s;
}


static void clear_repeat(repeated_command &r) {
    if (r.kind == repeat_kind::command)
        r.data.command.~repeat_command_payload();
    r.kind = repeat_kind::none;
}

repeated_command::~repeated_command() { clear_repeat(*this); }

static bool save_repeat(repeated_command &r, int command, const arg_struct *arg, const char *number) {
    // Prepare an independent argument before replacing the old union member.
    // CMD_NUMBER retains val_d even when storage changes its argument type to an integer.
    arg_struct normalized = {};
    normalized.type = arg == NULL ? ARGTYPE_NONE : arg->type;
    normalized.target = -1;
    normalized.val_d = 0;
    if (arg != NULL) {
        if (arg->type == ARGTYPE_DOUBLE || command == CMD_NUMBER)
            normalized.val_d = arg->val_d;
        if (arg->type != ARGTYPE_DOUBLE && arg->type != ARGTYPE_NONE)
            normalized.val = arg->val;
        if (arg->type == ARGTYPE_STR || arg->type == ARGTYPE_IND_STR || arg->type == ARGTYPE_XSTR)
            normalized.length = arg->length;
    }
    if (normalized.type == ARGTYPE_LBLINDEX) {
        if (normalized.val.num < 0 || normalized.val.num >= labels_count) return false;
        int label = normalized.val.num;
        normalized.type = ARGTYPE_STR;
        normalized.length = labels[label].length;
        memcpy(normalized.val.text, labels[label].name, normalized.length);
    }
    char *text = NULL, *digits = NULL;
    if (normalized.type == ARGTYPE_XSTR) {
        text = (char *) history_memory::allocate(normalized.length + 1);
        if (text == NULL) return false;
        memcpy(text, normalized.val.xstr, normalized.length);
        text[normalized.length] = 0;
        normalized.val.xstr = text;
    }
    if (number != NULL) {
        digits = (char *) history_memory::allocate(strlen(number) + 1);
        if (digits == NULL) { free(text); return false; }
        strcpy(digits, number);
    }
    clear_repeat(r);
    new (&r.data.command) repeat_command_payload(command, normalized, text, digits);
    r.kind = repeat_kind::command;
    return true;
}

static void history_error(int error) {
    display_error(error);
    redisplay();
}

template<class T> static void exchange(T &a, T &b) {
    T value = a; a = b; b = value;
}

// A binary undo restores only the consumed Y/X pair. In four-level mode, Z
// returns from its shifted position and the duplicated T is held for REDO.
static bool replay_stack(history_snapshot *s) {
    int expected = s->depth - (!s->undone && s->big_stack ? s->consumed - 1 : 0);
    if (s->big_stack != bool(flags.f.big_stack) || sp + 1 != expected) {
        history_error(ERR_INVALID_CONTEXT); return false;
    }
    if (!s->undone && s->big_stack && s->consumed == 2 && stack_capacity < s->depth) {
        int capacity = s->depth < 4 ? 4 : s->depth;
        vartype **expanded = (vartype **) history_memory::resize(stack, sizeof(vartype *) * capacity);
        if (expanded == NULL) { history_error(ERR_INSUFFICIENT_MEMORY); return false; }
        stack = expanded; stack_capacity = capacity;
    }
    if (s->consumed == 1) {
        exchange(s->values[0], stack[sp]);
    } else if (!s->undone) {
        vartype *result = stack[sp];
        if (s->big_stack) {
            stack[sp] = s->values[0];
            stack[++sp] = s->values[1];
        } else {
            s->duplicate_t = stack[REG_Z];
            stack[REG_Z] = stack[REG_Y];
            stack[REG_Y] = s->values[0];
            stack[REG_X] = s->values[1];
        }
        s->values[0] = result; s->values[1] = NULL;
    } else {
        vartype *y = stack[sp - 1], *x = stack[sp];
        if (s->big_stack) {
            stack[--sp] = s->values[0];
        } else {
            stack[REG_X] = s->values[0];
            stack[REG_Y] = stack[REG_Z];
            stack[REG_Z] = s->duplicate_t;
            s->duplicate_t = NULL;
        }
        s->values[0] = y; s->values[1] = x;
    }
    s->undone = !s->undone;
    exchange(s->last, lastx);
    exchange(s->disable_lift, mode_disable_stack_lift);
    bool lift = flags.f.stack_lift_disable;
    flags.f.stack_lift_disable = s->lift_disabled; s->lift_disabled = lift;
    if (s->carry_recorded) exchange(s->carry, mode_carry);
    pending_command = CMD_NONE;
    flags.f.message = flags.f.two_line_message = 0;
    redisplay();
    return true;
}

static bool restore_snapshot(const history_snapshot *s) {
    const bool changed_program = s->program && !same_programs(s);
    if (changed_program) {
        for (int i = 0; i < prgms_count; i++)
            if (prgms[i].locked && (s->program_count != prgms_count || !same_program_text(s, i))) {
                history_error(ERR_PROGRAM_LOCKED); return false;
            }
    }
    history_snapshot *copy = new_snapshot();
    if (copy == NULL || !copy_program_data(copy, s->programs, s->program_count,
            s->program_labels, s->label_count)) {
        free_snapshot(copy);
        history_error(ERR_INSUFFICIENT_MEMORY);
        return false;
    }
    if (s->program) {
        if (changed_program) {
            // Keep current lock settings; locks are not editor history.
            for (int i = 0; i < copy->program_count && i < prgms_count; i++)
                copy->programs[i].locked = prgms[i].locked;
            clear_all_rtns();
            clear_all_prgms();
            prgms = copy->programs;
            prgms_count = prgms_capacity = copy->program_count;
            labels = copy->program_labels;
            labels_count = labels_capacity = copy->label_count;
            copy->programs = NULL; copy->program_count = 0;
            copy->program_labels = NULL; copy->label_count = 0;
            // Cached local branches belong to the restored text; invalidate them
            // without asking the allocator to rebuild metadata during the swap.
            for (int p = 0; p < prgms_count; p++) {
                int4 pos = 0;
                while (pos < prgms[p].size) {
                    int command = prgms[p].text[pos] | ((prgms[p].text[pos + 1] & 112) << 4);
                    int type = prgms[p].text[pos + 1] & 15;
                    if ((command == CMD_GTO || command == CMD_XEQ)
                            && (type == ARGTYPE_NUM || type == ARGTYPE_STK || type == ARGTYPE_LCLBL))
                        memset(prgms[p].text + pos + 2, 255, 4);
                    pos += get_command_length(p, pos);
                }
                prgms[p].lclbl_invalid = true;
            }
        }
        current_prgm = s->selected_program;
        pc = s->position;
        prgm_highlight_row = s->highlight;
    }
    mode_number_entry = s->number_mode;
    mode_alpha_entry = s->alpha_mode;
    mode_command_entry = false;
    pending_command = CMD_NONE;
    cmdline_length = s->line_length;
    cmdline_row = s->line_row;
    memcpy(cmdline, s->line, sizeof(s->line));
    entered_number = s->number;
    entered_string_length = s->string_length;
    memcpy(entered_string, s->string, sizeof(s->string));
    free_snapshot(copy);
    flags.f.message = flags.f.two_line_message = 0;
    redisplay();
    return true;
}

static bool move_history(bool redo, bool *enqueued, int *repeat) {
    *enqueued = false; *repeat = 0;
    if (history == NULL) { history_error(ERR_INSUFFICIENT_MEMORY); return false; }
    history->suppressed = true;
    if (mode_running || mode_interruptible != NULL || mode_command_entry || matedit_mode != 0
            || (!flags.f.prgm_mode && mode_number_entry)) {
        history_error(ERR_INVALID_CONTEXT); return false;
    }
    history_lane &lane = history->lanes[flags.f.prgm_mode ? 1 : 0];
    history_snapshot **source = redo ? lane.redo : lane.undo;
    history_snapshot **destination = redo ? lane.undo : lane.redo;
    int &source_count = redo ? lane.redo_count : lane.undo_count;
    int &destination_count = redo ? lane.undo_count : lane.redo_count;
    if (!source_count) {
        history_error(lane.unsupported ? ERR_NOT_YET_IMPLEMENTED : ERR_NONEXISTENT);
        return false;
    }
    history_snapshot *target = source[source_count - 1];
    if (!flags.f.prgm_mode) {
        if (!replay_stack(target)) return false;
        source_count--;
        append_snapshot(destination, destination_count, target);
        return false;
    }
    history_snapshot *current = capture_snapshot();
    if (current == NULL) { history_error(ERR_INSUFFICIENT_MEMORY); return false; }
    if (!restore_snapshot(target)) { free_snapshot(current); return false; }
    source_count--;
    free_snapshot(target);
    append_snapshot(destination, destination_count, current);
    return false;
}

} // namespace

void custom_history_begin() {
    if (!ensure_history()) return;
    if (history->scope_depth++ != 0) return;
    history->suppressed = false;
    history->scope_program = flags.f.prgm_mode;
    if (mode_running || mode_interruptible != NULL || matedit_mode != 0) custom_history_barrier();
    if (history->observed_big_stack != bool(flags.f.big_stack)) {
        clear_lane(history->lanes[0], true);
        history->observed_big_stack = flags.f.big_stack;
    }
    history->before = flags.f.prgm_mode && !mode_running && mode_interruptible == NULL
        && matedit_mode == 0 ? capture_snapshot() : NULL;
}

void custom_history_end() {
    if (history == NULL) return;
    if (history->scope_depth <= 0 || --history->scope_depth != 0) return;
    if (mode_running || mode_interruptible != NULL || matedit_mode != 0) custom_history_barrier();
    if (history->observed_big_stack != bool(flags.f.big_stack)) {
        clear_lane(history->lanes[0], true);
        history->observed_big_stack = flags.f.big_stack;
    }
    if (!history->suppressed && history->scope_program && flags.f.prgm_mode) {
        history_lane &lane = history->lanes[1];
        if (mode_running || mode_interruptible != NULL || matedit_mode != 0
                || history->before == NULL) clear_lane(lane, true);
        else if (!same_snapshot(history->before)) {
            clear_list(lane.redo, lane.redo_count);
            append_snapshot(lane.undo, lane.undo_count, history->before);
            history->before = NULL;
            lane.unsupported = false;
        }
    }
    free_snapshot(history->before); history->before = NULL;
    history->suppressed = false;
}

void custom_history_reset() {
    if (history == NULL) return;
    clear_lane(history->lanes[0], false); clear_lane(history->lanes[1], false);
    free_snapshot(history->before); history->before = NULL;
    history->scope_depth = 0;
    history->suppressed = false;
    history->~history_context();
    free(history); history = NULL;
}

void custom_history_barrier() {
    if (history == NULL || history->suppressed) return;
    clear_lane(history->lanes[0], true);
    history->barrier_serial++;
}

int custom_history_execute(int command, arg_struct *arg,
        int (*handler)(int command, arg_struct *arg)) {
    if (!ensure_history()) return handler(command, arg);
    if (history->observed_big_stack != bool(flags.f.big_stack)) {
        custom_history_barrier();
        history->observed_big_stack = flags.f.big_stack;
    }
    unsigned long generation = history->generation;
    const bool recorded_edit = history->before != NULL && history->before->program
        && history->scope_program && flags.f.prgm_mode && !mode_running
        && mode_interruptible == NULL && matedit_mode == 0;
    if (history->execute_depth++ != 0) {
        custom_history_barrier();
        int result = handler(command, arg);
        if (history != NULL && history->generation == generation) {
            history->execute_depth--;
            finish_program_history_command(command, result, recorded_edit);
        }
        return result;
    }
    bool eligible = !loading_state && !flags.f.prgm_mode && !mode_running
        && mode_interruptible == NULL && matedit_mode == 0;
    int consumed = consumed_values(command);
    history_snapshot *s = eligible && consumed ? capture_command(consumed) : NULL;
    if (s == NULL && (!eligible || !is_excluded_from_execution_history(command))) custom_history_barrier();
    unsigned long barrier = history->barrier_serial;
    // Borrowed positions validate the fixed-stack shift without copying Z or T.
    vartype *old_t = s != NULL && !s->big_stack && consumed == 2 ? stack[REG_T] : NULL;
    vartype *old_z = s != NULL && !s->big_stack && consumed == 2 ? stack[REG_Z] : NULL;
    int result = handler(command, arg);
    if (history == NULL || history->generation != generation) {
        free_snapshot(s); return result;
    }
    history->execute_depth--;
    finish_program_history_command(command, result, recorded_edit);
    if (s == NULL) return result;
    bool shape = sp + 1 == s->depth - (s->big_stack ? consumed - 1 : 0)
        && s->big_stack == bool(flags.f.big_stack) && stack[sp] != NULL && lastx != NULL;
    if (shape && !s->big_stack && consumed == 2)
        shape = stack[REG_T] == old_t && stack[REG_Y] == old_z
            && same_value(stack[REG_Z], stack[REG_T]);
    if (shape) {
        s->bytes += value_bytes(stack[sp]) + value_bytes(lastx);
        if (!s->big_stack && consumed == 2) s->bytes += value_bytes(stack[REG_Z]);
    }
    if (result != ERR_NONE || !shape || s->bytes > action_budget
            || mode_running || mode_interruptible != NULL || matedit_mode != 0
            || history->barrier_serial != barrier) {
        free_snapshot(s); custom_history_barrier(); return result;
    }
    history_lane &lane = history->lanes[0];
    clear_list(lane.redo, lane.redo_count);
    append_snapshot(lane.undo, lane.undo_count, s);
    lane.unsupported = false;
    return result;
}

void custom_history_command(int command, const arg_struct *arg) {
    if (history == NULL || loading_state) return;
    if (flags.f.prgm_mode || command < 0 || command >= CMD_SENTINEL) return;
    if (!is_excluded_from_execution_history(command) && !save_repeat(history->last_command[0], command, arg, NULL))
        clear_repeat(history->last_command[0]);
}

void custom_history_program_command(int command, const arg_struct *arg, const char *number) {
    if (loading_state || !ensure_history()) return;
    if (flags.f.prgm_mode && command >= 0 && command < CMD_SENTINEL
            && !save_repeat(history->last_command[1], command, arg, number))
        clear_repeat(history->last_command[1]);
}

void custom_history_program_action(bool (*action)(bool *enqueued, int *repeat)) {
    if (loading_state || !flags.f.prgm_mode || !ensure_history()) return;
    repeated_command &record = history->last_command[1];
    clear_repeat(record);
    if (action != NULL) {
        record.data.action = action;
        record.kind = repeat_kind::action;
    }
}

extern "C" bool core_undo(bool *enqueued, int *repeat) {
    custom_history_scope scope;
    return move_history(false, enqueued, repeat);
}

extern "C" bool core_redo(bool *enqueued, int *repeat) {
    custom_history_scope scope;
    return move_history(true, enqueued, repeat);
}

extern "C" bool core_repeat_last_command(bool *enqueued, int *repeat) {
    custom_history_scope scope;
    if (history == NULL) { *enqueued = false; *repeat = 0; history_error(ERR_INSUFFICIENT_MEMORY); return false; }
    *enqueued = false; *repeat = 0;
    if (mode_running || mode_interruptible != NULL || mode_command_entry || mode_alpha_entry) {
        history_error(ERR_INVALID_CONTEXT); return false;
    }
    const bool program = flags.f.prgm_mode;
    repeated_command &record = history->last_command[program ? 1 : 0];
    // Editor navigation can repeat in locked programs and must not commit pending numbers.
    if (record.kind == repeat_kind::action)
        return record.data.action(enqueued, repeat);
    if (program && prgms[current_prgm].locked) { history_error(ERR_PROGRAM_LOCKED); return false; }
    if (program && mode_number_entry) {
        arg_struct number;
        number.type = ARGTYPE_DOUBLE;
        number.val_d = entered_number;
        cmdline[cmdline_length] = 0;
        if (!store_command(pc, CMD_NUMBER, &number, cmdline)) {
            history_error(ERR_INSUFFICIENT_MEMORY); return false;
        }
        mode_number_entry = false;
        prgm_highlight_row = 1;
        pending_command = CMD_NONE;
    }
    if (record.kind != repeat_kind::command) { history_error(ERR_NONEXISTENT); return false; }
    const repeat_command_payload &saved = record.data.command;
    if (!save_repeat(history->inflight, saved.command, &saved.arg, saved.number)) {
        history_error(ERR_INSUFFICIENT_MEMORY); return false;
    }
    repeat_command_payload &inflight = history->inflight.data.command;
    if (program) {
        int4 insertion = pc < 0 ? 0 : pc;
        if (pc >= 0 && !prgms[current_prgm].is_end(pc)) insertion += get_command_length(current_prgm, pc);
        if (!store_command(insertion, inflight.command, &inflight.arg, inflight.number)) {
            history_error(ERR_INSUFFICIENT_MEMORY); return false;
        }
        pc = inflight.command == CMD_END ? 0 : insertion;
        prgm_highlight_row = 1;
        pending_command = CMD_NONE;
        redisplay();
        return false;
    }
    bool running = core_keydown_command_id(inflight.command, enqueued, repeat);
    if (mode_command_entry && !*enqueued) {
        pending_command = inflight.command;
        pending_command_arg = inflight.arg;
        finish_command_entry(true);
    }
    return running;
}
