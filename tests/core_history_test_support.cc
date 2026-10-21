// Adapt the history implementation for host tests and expose its registration table.
#include "../common/core_history_custom.cc"
#include "history_test_boundary.h"

registration_range history_test_commands() {
#define HISTORY_REGISTRATION(command, arity) {command, arity, #command},
  static const registration_case cases[] = {
      UNDO_COMMANDS(HISTORY_REGISTRATION)
  };
#undef HISTORY_REGISTRATION
  return {cases, cases + sizeof(cases) / sizeof(cases[0])};
}
