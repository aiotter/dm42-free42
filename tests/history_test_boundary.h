#ifndef HISTORY_TEST_BOUNDARY_H
#define HISTORY_TEST_BOUNDARY_H

struct registration_case {
  int command, arity;
  const char *name;
};

struct registration_range {
  const registration_case *first, *last;
  const registration_case *begin() const { return first; }
  const registration_case *end() const { return last; }
};

registration_range history_test_commands();

#endif
