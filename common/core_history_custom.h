/* Session-only command and editor history for the calculator frontend. */
#ifndef CORE_HISTORY_CUSTOM_H
#define CORE_HISTORY_CUSTOM_H

#include "core_tables.h"

void custom_history_begin();
void custom_history_end();
void custom_history_reset();
void custom_history_barrier();
int custom_history_execute(int command, arg_struct *arg,
        int (*handler)(int command, arg_struct *arg));
void custom_history_command(int command, const arg_struct *arg);
void custom_history_program_command(int command, const arg_struct *arg, const char *number);

class custom_history_scope {
public:
    custom_history_scope() { custom_history_begin(); }
    ~custom_history_scope() { custom_history_end(); }
private:
    custom_history_scope(const custom_history_scope &) = delete;
    custom_history_scope &operator=(const custom_history_scope &) = delete;
};

extern "C" {
bool core_repeat_last_command(bool *enqueued, int *repeat);
bool core_undo(bool *enqueued, int *repeat);
bool core_redo(bool *enqueued, int *repeat);
}

#endif
