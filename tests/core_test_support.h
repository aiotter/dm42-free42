#ifndef CORE_TEST_SUPPORT_H
#define CORE_TEST_SUPPORT_H

#include "core_aux.h"
#include "core_display.h"
#include "core_globals.h"
#include "core_helpers.h"
#include "core_history_custom.h"
#include "core_keydown.h"
#include "core_main.h"
#include "core_variables.h"
#include "shell.h"
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

const char *shell_platform() { return "test"; }
void shell_blitter(const char *, int, int, int, int, int) {}
static int beep_calls;
void shell_beeper(int) { beep_calls++; }
void shell_annunciators(int, int, int, int, int, int) {}
bool shell_wants_cpu() { return false; }
void shell_delay(int) {}
void shell_request_timeout3(int) {}
uint8 shell_get_mem() { return 100000000; }
bool shell_low_battery() { return false; }
void shell_powerdown() {}
int8 shell_random_seed() { return 123; }
uint4 shell_milliseconds() { return 0; }
const char *shell_number_format() { return ".,33"; }
int shell_date_format() { return 0; }
bool shell_clk24() { return true; }
void shell_print(const char *, int, const char *, int, int, int, int, int) {}
void shell_get_time_date(uint4 *t, uint4 *d, int *w) {
  *t = 12000000;
  *d = 20261021;
  *w = 0;
}
void shell_message(const char *) {}
void shell_log(const char *) {}

static int checks;
static void check(bool ok, const char *name) {
  if (!ok) {
    fprintf(stderr, "FAIL %s\n", name);
    abort();
  }
  printf("PASS %s\n", name);
  checks++;
}
static void press(int key) {
  bool queued;
  int repeat;
  core_keydown(key, &queued, &repeat);
  assert(!queued);
  core_keyup();
}

static std::vector<unsigned char> program() {
  return std::vector<unsigned char>(prgms[current_prgm].text,
                                    prgms[current_prgm].text +
                                        prgms[current_prgm].size);
}

static std::vector<int> program_commands() {
  std::vector<int> commands;
  int4 cursor = 0;
  while (cursor < prgms[current_prgm].size) {
    int command;
    arg_struct argument;
    get_next_command(&cursor, &command, &argument, 0, NULL);
    commands.push_back(command);
    if (command == CMD_END)
      break;
  }
  return commands;
}

// Failure injection affects only allocations requested by the history module.
size_t journal_test_allocated;
size_t journal_test_duplicates;
int journal_test_fail_after;

namespace history_memory {
static bool fail_allocation() {
  return journal_test_fail_after > 0 && --journal_test_fail_after == 0;
}
void *allocate(size_t bytes) {
  journal_test_allocated += bytes;
  return fail_allocation() ? NULL : malloc(bytes);
}
void *allocate_zeroed(size_t count, size_t bytes) {
  journal_test_allocated += count * bytes;
  return fail_allocation() ? NULL : calloc(count, bytes);
}
void *resize(void *memory, size_t bytes) {
  journal_test_allocated += bytes;
  return fail_allocation() ? NULL : realloc(memory, bytes);
}
vartype *duplicate(const vartype *value) {
  journal_test_duplicates++;
  return fail_allocation() ? NULL : dup_vartype(value);
}
}

#endif
