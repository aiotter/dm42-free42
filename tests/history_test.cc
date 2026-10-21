#include "core_history_custom.h"
#include "core_test_support.h"
#include <algorithm>
#include <cstdlib>

static bool queued;
static int repetition;
static phloat real_at(int i) {
  assert(i >= 0 && i <= sp && stack[i]->type == TYPE_REAL);
  return ((vartype_real *)stack[i])->x;
}
static phloat x() { return real_at(sp); }
static phloat lx() {
  assert(lastx->type == TYPE_REAL);
  return ((vartype_real *)lastx)->x;
}
static void command(int c) {
  core_keydown_command_id(c, &queued, &repetition);
  assert(!queued);
  core_keyup();
}
static void undo() {
  core_undo(&queued, &repetition);
  assert(!queued);
  core_keyup();
}
static void redo() {
  core_redo(&queued, &repetition);
  assert(!queued);
  core_keyup();
}
static void again() {
  core_repeat_last_command(&queued, &repetition);
  assert(!queued);
  core_keyup();
}
static void setx(vartype *v) {
  assert(sp >= 0);
  free_vartype(stack[sp]);
  stack[sp] = v;
}
static void reset(int depth = 4, bool big = false) {
  journal_test_fail_after = 0;
  hard_reset(0);
  core_settings.allow_big_stack = true;
  for (int i = 0; i <= sp; i++)
    free_vartype(stack[i]);
  free(stack);
  stack = (vartype **)calloc(std::max(depth, 4), sizeof(vartype *));
  stack_capacity = std::max(depth, 4);
  sp = depth - 1;
  for (int i = 0; i < depth; i++)
    stack[i] = new_real(i + 1);
  flags.f.big_stack = big;
  flags.f.stack_lift_disable = 0;
  mode_disable_stack_lift = false;
  flags.f.message = flags.f.two_line_message = 0;
  mode_number_entry = 0;
  mode_alpha_entry = false;
  mode_command_entry = false;
  pending_command = CMD_NONE;
  custom_history_reset();
}
static void check_stack(std::initializer_list<int> v, const char *n) {
  bool ok = sp + 1 == (int)v.size();
  int i = 0;
  for (int a : v) {
    if (ok)
      ok = stack[i]->type == TYPE_REAL && real_at(i) == a;
    i++;
  }
  check(ok, n);
}
static arg_struct strarg(const char *s) {
  arg_struct a = {};
  a.type = ARGTYPE_STR;
  a.length = strlen(s);
  memcpy(a.val.text, s, a.length);
  return a;
}
static void arg_command(int c, arg_struct a) {
  core_keydown_command_id(c, &queued, &repetition);
  core_keyup();
  pending_command = c;
  pending_command_arg = a;
  if (mode_command_entry)
    finish_command_entry(true);
  core_keyup();
}
static void setxy(int y, int z) {
  free_vartype(stack[sp - 1]);
  stack[sp - 1] = new_real(y);
  setx(new_real(z));
}
static void begin_measure() {
  journal_test_allocated = journal_test_duplicates = 0;
}
#include "history_test_boundary.h"
static std::vector<phloat> matrix_state() {
  assert(sp == 0 && stack[0]->type == TYPE_REALMATRIX);
  auto *m = (vartype_realmatrix *)stack[0];
  return std::vector<phloat>(m->array->data,
                             m->array->data + m->columns * m->rows);
}
static std::vector<std::vector<phloat>> budget_setup(bool large_program) {
  reset(1, true);
  if (large_program) {
    std::string source(40000, 'Q');
    arg_struct a = {};
    a.type = ARGTYPE_XSTR;
    a.length = source.size();
    a.val.xstr = (char *)source.data();
    check(store_command(0, CMD_XSTR, &a, NULL),
          "budget fixture stores large valid program");
  }
  auto *m = (vartype_realmatrix *)new_realmatrix(1, 90);
  for (int i = 0; i < 90; i++)
    m->array->data[i] = i + 1;
  setx((vartype *)m);
  std::vector<std::vector<phloat>> states;
  states.push_back(matrix_state());
  for (int i = 0; i < 20; i++) {
    command(CMD_TO_METER);
    check(!flags.f.message, "matrix operation fits per-action budget");
    states.push_back(matrix_state());
  }
  return states;
}

static void program_edge_cases() {
  // REPEAT commits an unfinished numeric instruction before inserting its copy.
  reset();
  flags.f.prgm_mode = true;
  pc = -1;
  press(KEY_1);
  again();
  int4 position = 0;
  int cmd;
  arg_struct argument;
  get_next_command(&position, &cmd, &argument, 0, NULL);
  check(cmd == CMD_NUMBER && argument.val_d == 1,
        "program REPEAT commits numeric entry");
  get_next_command(&position, &cmd, &argument, 0, NULL);
  check(cmd == CMD_NUMBER && argument.val_d == 1,
        "program REPEAT duplicates numeric value");

  // A locked program rejects restoration without consuming the history entry.
  reset();
  flags.f.prgm_mode = true;
  pc = -1;
  command(CMD_ADD);
  prgms[current_prgm].locked = true;
  auto locked = program();
  undo();
  check(program() == locked && flags.f.message,
        "locked edited program rejects UNDO");
  prgms[current_prgm].locked = false;
  undo();
  check(program_commands() == std::vector<int>({CMD_END}),
        "failed locked UNDO preserves its history entry");

  // A lock on an unrelated program does not prevent undoing the selected program.
  reset();
  flags.f.prgm_mode = true;
  pc = -1;
  arg_struct end = {};
  end.type = ARGTYPE_NONE;
  check(store_command(0, CMD_END, &end, NULL), "create second program");
  current_prgm = 0;
  pc = -1;
  custom_history_reset();
  prgms[1].locked = true;
  auto empty = program();
  command(CMD_ADD);
  undo();
  check(program() == empty, "unrelated locked program permits editing UNDO");

  // Command argument entry forms one edit even though it spans several keys.
  reset();
  flags.f.prgm_mode = true;
  pc = -1;
  empty = program();
  command(CMD_STO);
  press(KEY_0);
  press(KEY_1);
  check(program() != empty, "program STO 01 completes argument entry");
  undo();
  check(program() == empty, "one UNDO removes entire STO 01 instruction");

  // Cancelling argument entry does not create an additional undo step.
  reset();
  flags.f.prgm_mode = true;
  pc = -1;
  empty = program();
  command(CMD_ADD);
  command(CMD_STO);
  press(KEY_EXIT);
  undo();
  check(program() == empty,
        "cancelled program argument entry adds no history step");

  // REPEAT owns long string operands independently of the caller's storage.
  reset();
  flags.f.prgm_mode = true;
  pc = -1;
  char long_text[40];
  memset(long_text, 'A', sizeof(long_text));
  arg_struct text_arg = {};
  text_arg.type = ARGTYPE_XSTR;
  text_arg.length = sizeof(long_text);
  text_arg.val.xstr = long_text;
  {
    custom_history_scope scope;
    check(store_command(0, CMD_XSTR, &text_arg, NULL), "insert long string");
    pc = 0;
  }
  memset(long_text, 'B', sizeof(long_text));
  again();
  position = 0;
  get_next_command(&position, &cmd, &argument, 0, NULL);
  get_next_command(&position, &cmd, &argument, 0, NULL);
  check(cmd == CMD_XSTR && argument.length == 40 &&
            argument.val.xstr[0] == 'A' && argument.val.xstr[39] == 'A',
        "program REPEAT owns long string argument independently");
  undo();
  redo();
  position = 0;
  get_next_command(&position, &cmd, &argument, 0, NULL);
  get_next_command(&position, &cmd, &argument, 0, NULL);
  check(cmd == CMD_XSTR && argument.length == 40 && argument.val.xstr[39] == 'A',
        "long string REPEAT survives UNDO and REDO");

  // Restoring program edits does not restore ordinary ALPHA register contents.
  reg_alpha_length = 3;
  memcpy(reg_alpha, "NEW", 3);
  undo();
  check(reg_alpha_length == 3 && !memcmp(reg_alpha, "NEW", 3),
        "program UNDO leaves normal ALPHA data unchanged");
}

int main() {
  setbuf(stdout, NULL);
  core_init(0, 0, NULL, 0);

  // The public string-command API reaches the same undoable execution path.
  reset();
  core_keydown_command("+", false, &queued, &repetition);
  check(!queued && repetition == 0, "string API accepts arithmetic command");
  core_keyup();
  check_stack({1, 1, 2, 7}, "string API executes ADD");
  undo();
  check_stack({1, 2, 3, 4}, "string API arithmetic is undoable");

  // Fixed-stack binary operations replicate T, unlike a large-stack pop.
  reset();
  press(KEY_ADD);
  check_stack({1, 1, 2, 7}, "4-level ADD replicates T");
  check(lx() == 4, "ADD updates LAST X");
  for (int i = 0; i < 2; i++) {
    undo();
    check_stack({1, 2, 3, 4}, "4-level binary UNDO restores original Z and T");
    check(lx() == 0, "binary UNDO restores LAST X");
    redo();
    check_stack({1, 1, 2, 7}, "4-level binary REDO restores T duplication");
    check(lx() == 4, "binary REDO restores LAST X");
  }

  // A large-stack binary operation restores consumed operands and leaves lower levels intact.
  reset(10000, true);
  command(CMD_ADD);
  check(sp == 9998 && x() == 19999 && real_at(0) == 1,
        "deep ADD produces one result");
  undo();
  check(sp == 9999 && x() == 10000 && real_at(9998) == 9999 && real_at(0) == 1,
        "deep ADD UNDO restores only consumed operands");
  redo();
  check(sp == 9998 && x() == 19999, "deep ADD REDO");
  undo();
  command(CMD_MUL);
  redo();
  check(sp == 9998 && x() == 99990000 && flags.f.message,
        "new calculation invalidates REDO");

  // Representative binary handlers preserve the same large-stack history contract.
  for (int c : {CMD_SUB, CMD_MUL, CMD_DIV}) {
    reset(10000, true);
    command(c);
    phloat result = x();
    undo();
    check(sp == 9999 && x() == 10000 && real_at(9998) == 9999,
          "binary pure calculation UNDO");
    redo();
    check(sp == 9998 && x() == result, "binary pure calculation REDO");
  }

  // Unary handlers restore X without changing stack depth.
  for (int c : {CMD_SIN, CMD_COS, CMD_TAN, CMD_SQRT, CMD_SQUARE, CMD_INV,
                CMD_LOG, CMD_LN, CMD_ABS, CMD_IP, CMD_FP}) {
    reset(10000, true);
    command(c);
    phloat result = x();
    undo();
    check(sp == 9999 && x() == 10000, "unary pure calculation UNDO");
    redo();
    check(sp == 9999 && x() == result, "unary pure calculation REDO");
  }

  // History preserves complex operands and their calculated result.
  reset(2, true);
  setx(new_complex(2, 3));
  command(CMD_SQUARE);
  undo();
  check(stack[sp]->type == TYPE_COMPLEX &&
            ((vartype_complex *)stack[sp])->re == 2 &&
            ((vartype_complex *)stack[sp])->im == 3,
        "pure unary records complex operand");
  redo();
  check(((vartype_complex *)stack[sp])->re == -5 &&
            ((vartype_complex *)stack[sp])->im == 12,
        "pure unary REDO restores complex result");

  // Percent reads Y but replaces only X, including when the command is repeated.
  reset(2, true);
  setxy(200, 5);
  command(CMD_PERCENT);
  check(sp == 1 && real_at(0) == 200 && x() == 10,
        "PERCENT changes X while keeping read-only Y");
  undo();
  check(sp == 1 && real_at(0) == 200 && x() == 5,
        "PERCENT UNDO restores X without changing Y");
  redo();
  check(sp == 1 && real_at(0) == 200 && x() == 10,
        "PERCENT REDO restores X without changing Y");
  again();
  check(sp == 1 && real_at(0) == 200 && x() == 20,
        "PERCENT REPEAT keeps Y and recomputes X");
  undo();
  check(sp == 1 && real_at(0) == 200 && x() == 10,
        "repeated PERCENT is undoable");

  // Matrix history owns values correctly when arithmetic uses copy-on-write storage.
  reset(2, true);
  auto *m = (vartype_realmatrix *)new_realmatrix(1, 2);
  m->array->data[0] = 2;
  m->array->data[1] = 3;
  setx((vartype *)m);
  command(CMD_CHS);
  undo();
  check(stack[sp]->type == TYPE_REALMATRIX &&
            ((vartype_realmatrix *)stack[sp])->array->data[0] == 2,
        "pure CHS UNDO restores COW matrix operand");
  redo();
  check(((vartype_realmatrix *)stack[sp])->array->data[0] == -2,
        "pure CHS REDO restores matrix result");

  // BASE history restores carry in both hexadecimal and decimal BASE displays.
  for (int radix : {16, 10}) {
    reset(2, true);
    baseapp = 1;
    set_base(radix);
    mode_wsize = 8;
    flags.f.base_signed = 0;
    flags.f.base_wrap = 1;
    setxy(255, 2);
    mode_carry = false;
    command(CMD_BASEADD);
    check(x() == 1 && mode_carry, "BASE+ sets carry in HEX or DEC BASE app");
    undo();
    check(sp == 1 && real_at(0) == 255 && x() == 2 && !mode_carry,
          "BASE app UNDO restores operands and carry");
    redo();
    check(sp == 0 && x() == 1 && mode_carry,
          "BASE app REDO restores result and carry");
  }

  // Explicit BASE commands outside the BASE application do not make carry undoable.
  reset(2, true);
  set_base(16);
  baseapp = 0;
  mode_wsize = 8;
  flags.f.base_signed = 0;
  flags.f.base_wrap = 1;
  setxy(255, 2);
  mode_carry = false;
  command(CMD_BASEADD);
  check(x() == 1 && mode_carry,
        "explicit BASE+ outside BASE app changes carry");
  undo();
  check(sp == 1 && x() == 2 && mode_carry,
        "outside BASE app UNDO leaves carry unchanged");
  mode_carry = false;
  redo();
  check(sp == 0 && x() == 1 && !mode_carry,
        "outside BASE app REDO leaves carry unchanged");

  // Other BASE arithmetic handlers share the operand-and-carry restoration contract.
  for (int c : {CMD_BASESUB, CMD_BASEMUL, CMD_BASEDIV, CMD_BASECHS}) {
    reset(4, true);
    baseapp = 1;
    set_base(16);
    mode_wsize = 8;
    flags.f.base_signed = 0;
    flags.f.base_wrap = 1;
    command(c);
    phloat result = x();
    bool carry = mode_carry;
    undo();
    check(sp == 3 && x() == 4, "BASE pure operation UNDO");
    redo();
    check(x() == result && mode_carry == carry,
          "BASE pure operation REDO restores carry");
  }

  // Unsupported stack mutations prevent history from crossing an unrecorded change.
  for (int c :
       {CMD_RUP, CMD_RDN, CMD_SWAP, CMD_DUP, CMD_DROP, CMD_ENTER, CMD_PI}) {
    reset(4, true);
    command(CMD_ADD);
    command(c);
    int depth = sp;
    std::vector<phloat> state;
    for (int i = 0; i <= sp; i++)
      state.push_back(real_at(i));
    undo();
    bool same = sp == depth;
    for (int i = 0; same && i <= sp; i++)
      same = real_at(i) == state[i];
    check(same && flags.f.message,
          "unregistered stack operation blocks earlier UNDO");
  }

  // STO remains outside history and its variable assignment survives an UNDO attempt.
  reset(4, true);
  command(CMD_ADD);
  arg_command(CMD_STO, strarg("A"));
  undo();
  check(sp == 2 && x() == 7 && ((vartype_real *)recall_var("A", 1))->x == 7 &&
            flags.f.message,
        "unregistered STO keeps assignment and blocks earlier UNDO");

  // Solver and integration attempts must not leave an earlier calculation undoable.
  for (int c : {CMD_SOLVE, CMD_INTEG}) {
    reset(4, true);
    command(CMD_ADD);
    pending_command = c;
    pending_command_arg = strarg("?");
    core_keyup();
    undo();
    check(sp == 2 && x() == 7 && flags.f.message,
          "solver execution attempt blocks earlier UNDO");
  }

  // Excluded settings neither discard arithmetic history nor get restored by it.
  reset(4, true);
  command(CMD_ADD);
  command(CMD_RAD);
  undo();
  check_stack({1, 2, 3, 4},
              "display setting preserves prior calculation history");
  check(flags.f.rad, "UNDO preserves changed angle setting");

  // Ordinary ALPHA register edits stay outside arithmetic history.
  reset(4, true);
  command(CMD_ADD);
  reg_alpha_length = 3;
  memcpy(reg_alpha, "NEW", 3);
  undo();
  check(reg_alpha_length == 3 && !memcmp(reg_alpha, "NEW", 3),
        "pure calculation UNDO leaves ALPHA contents unchanged");

  // Typed numeric entry prevents UNDO from crossing the newly entered value.
  reset(4, true);
  command(CMD_ADD);
  press(KEY_1);
  press(KEY_2);
  // Finish entry without clearing history, so UNDO cannot fail merely because
  // numeric input is still active.
  command(CMD_RAD);
  undo();
  check(!mode_number_entry && sp == 3 && x() == 12 && flags.f.message,
        "typed numeric input blocks UNDO across input");

  // Pasted numeric values also invalidate earlier arithmetic history.
  reset(4, true);
  command(CMD_ADD);
  core_paste("9");
  int pasted_depth = sp;
  undo();
  check(sp == pasted_depth && x() == 9 && flags.f.message,
        "pasted numeric input blocks earlier UNDO");

  // Power-on processing ends arithmetic history before it can alter the stack.
  reset(4, true);
  command(CMD_ADD);
  core_powercycle();
  undo();
  check(sp == 2 && x() == 7 && flags.f.message,
        "power cycle invalidates previous calculation history");

  // Starting native program execution discards history before the engine can change the stack.
  reset(4, true);
  press(KEY_ADD);
  set_running(true);
  set_running(false);
  undo();
  check(sp == 2 && x() == 7 && flags.f.message,
        "execution start prevents crossing into earlier arithmetic history");

  // Entering interruptible work clears history without invoking the asynchronous callback.
  reset(4, true);
  press(KEY_ADD);
  mode_interruptible = [](bool) { return ERR_NONE; };
  {
    custom_history_scope input;
  }
  mode_interruptible = NULL;
  undo();
  check(sp == 2 && x() == 7 && flags.f.message,
        "interruptible work prevents crossing into earlier arithmetic history");

  // Matrix-editor input also forms a boundary before its edits can bypass the dispatcher.
  reset(4, true);
  press(KEY_ADD);
  matedit_mode = 1;
  {
    custom_history_scope input;
  }
  matedit_mode = 0;
  undo();
  check(sp == 2 && x() == 7 && flags.f.message,
        "matrix editing prevents crossing into earlier arithmetic history");

  // REPEAT executes after numeric entry and records exactly one calculation.
  reset(2, true);
  setxy(10, 2);
  command(CMD_ADD);
  press(KEY_3);
  again();
  check(sp == 0 && x() == 15,
        "REPEAT executes last pure command after numeric entry");
  undo();
  check(sp == 1 && real_at(0) == 12 && x() == 3,
        "REPEAT command is a single undoable calculation");
  redo();
  check(sp == 0 && x() == 15, "REPEAT command REDO restores result");

  // Operand validation failures do not expose stale arithmetic history.
  reset(2, true);
  command(CMD_ADD);
  command(CMD_ADD);
  undo();
  check(sp == 0 && x() == 3 && flags.f.message,
        "insufficient operands blocks earlier UNDO");

  // Type validation failures also block earlier history.
  reset(4, true);
  command(CMD_ADD);
  setx(new_string("bad", 3));
  command(CMD_SIN);
  undo();
  check(stack[sp]->type == TYPE_STRING && flags.f.message,
        "type-check failure blocks earlier UNDO");

  // A handler error invalidates the record instead of restoring pre-error operands.
  reset(4, true);
  command(CMD_ADD);
  setx(new_real(0));
  command(CMD_DIV);
  undo();
  check(sp == 2 && x() == 0 && flags.f.message,
        "handler error blocks earlier UNDO");

  // History allocation must depend on consumed operands rather than total stack depth.
  size_t small_bytes, small_dups, large_bytes, large_dups;
  for (int c : {CMD_ADD, CMD_BASEADD, CMD_SIN}) {
    reset(4, true);
    begin_measure();
    command(c);
    small_bytes = journal_test_allocated;
    small_dups = journal_test_duplicates;
    reset(10000, true);
    begin_measure();
    command(c);
    large_bytes = journal_test_allocated;
    large_dups = journal_test_duplicates;
    printf("MEASURE command%d depth4=%zu bytes/%zu dups depth10000=%zu "
           "bytes/%zu dups\n",
           c, small_bytes, small_dups, large_bytes, large_dups);
    check(small_bytes == large_bytes && small_dups == large_dups,
          "pure history allocation independent of stack depth");
  }

  // Recording failures allow arithmetic to finish and retain either a whole record or a barrier.
  int recording_failures = 0;
  for (int fail = 1;; fail++) {
    reset(4, true);
    command(CMD_ADD);
    journal_test_fail_after = fail;
    command(CMD_MUL);
    bool injected = journal_test_fail_after == 0;
    journal_test_fail_after = 0;
    if (!injected)
      break;
    recording_failures++;
    check(sp == 1 && x() == 14,
          "history allocation failure leaves arithmetic executing");
    undo();
    check((sp == 2 && x() == 7) || (sp == 1 && x() == 14 && flags.f.message),
          "history allocation failure keeps whole record or barrier");
  }
  check(recording_failures > 0, "recording fixture reaches history allocation failures");

  // Restoration failures leave stack values and the retained history entry atomic.
  int restoration_failures = 0;
  for (int fail = 1;; fail++) {
    reset(10000, true);
    command(CMD_ADD);
    shrink_stack();
    journal_test_fail_after = fail;
    undo();
    bool injected = journal_test_fail_after == 0;
    journal_test_fail_after = 0;
    if (!injected)
      break;
    restoration_failures++;
    bool unchanged = sp == 9998 && x() == 19999,
         restored = sp == 9999 && x() == 10000;
    check(unchanged || restored, "UNDO allocation failure leaves atomic state");
    if (unchanged) {
      check(flags.f.message, "UNDO allocation failure reports error");
      undo();
      check(sp == 9999 && x() == 10000, "failed UNDO preserves history");
    }
  }
  check(restoration_failures > 0, "restoration fixture reaches history allocation failures");

  // Count-limit eviction preserves a consecutive undo/redo chain for the retained tail.
  reset(10000, true);
  for (int i = 0; i < 80; i++)
    command(CMD_ADD);
  for (int i = 0; i < 16; i++)
    undo();
  check(sp == 9935, "pure history retains latest16 calculations");
  undo();
  check(sp == 9935 && flags.f.message, "pure history bounded eviction");
  for (int i = 0; i < 16; i++)
    redo();
  check(sp == 9919, "pure history REDO after eviction");

  // Program edits use a separate history and REPEAT inserts another decoded instruction.
  reset();
  flags.f.prgm_mode = true;
  pc = -1;
  auto empty = program();
  command(CMD_ADD);
  auto added = program();
  undo();
  check(program() == empty,
        "program edit UNDO survives execution-entry refactor");
  redo();
  check(program() == added,
        "program edit REDO survives execution-entry refactor");
  again();
  check(program_commands() == std::vector<int>({CMD_ADD, CMD_ADD, CMD_END}),
        "program REPEAT remains supported");

  // Real PRGM toggle keys preserve program history across intervening normal calculations.
  reset();
  flags.f.prgm_mode = true;
  pc = -1;
  empty = program();
  command(CMD_ADD);
  press(KEY_SHIFT);
  press(KEY_RUN);
  check(!flags.f.prgm_mode, "physical PRGM toggle leaves program mode");
  command(CMD_ADD);
  press(KEY_SHIFT);
  press(KEY_RUN);
  check(flags.f.prgm_mode, "physical PRGM toggle reenters program mode");
  undo();
  check(program() == empty,
        "program history survives normal-to-program reentry");

  // Direct execution observes stack-model changes and cannot revive incompatible records.
  reset();
  command(CMD_ADD);
  flags.f.big_stack = true;
  arg_struct none = {};
  none.type = ARGTYPE_NONE;
  check(handle(CMD_MUL, &none) == ERR_NONE,
        "direct execution handles observed big-stack mode change");
  undo();
  check_stack(
      {1, 1, 2, 7},
      "direct execution record survives subsequent UNDO mode observation");
  undo();
  check_stack({1, 1, 2, 7},
              "old fixed-stack record is discarded across mode change");
  check(flags.f.message,
        "mode change prevents crossing into previous stack model");

  // Every opt-in handler must satisfy the same fixed-stack restoration contract.
  for (auto entry : history_test_commands()) {
    reset();
    free_vartype(stack[0]);
    stack[0] = new_real(11);
    free_vartype(stack[1]);
    stack[1] = new_real(13);
    setxy(3, entry.arity == 1 ? 1 : 2);
    flags.f.base_wrap = true;
    command(entry.command);
    printf("REGISTERED %s arity%d\n", entry.name, entry.arity);
    check(!flags.f.message && sp == 3 && stack[3]->type == TYPE_REAL,
          "registered handler succeeds with expected fixed-stack shape");
    std::vector<phloat> after;
    for (int i = 0; i < 4; i++)
      after.push_back(real_at(i));
    phloat after_last = lx();
    {
      undo();
      bool restored = !flags.f.message && sp == 3 && real_at(0) == 11 &&
                      real_at(1) == 13 && real_at(2) == 3 &&
                      real_at(3) == (entry.arity == 1 ? 1 : 2) && lx() == 0;
      check(restored, "registered handler UNDO restores X/Y/Z/T and LAST X");
      redo();
      restored = !flags.f.message && sp == 3 && lx() == after_last;
      for (int i = 0; restored && i < 4; i++)
        restored = real_at(i) == after[i];
      check(restored,
            "registered handler REDO restores complete fixed-stack result");
    }
  }

  // Byte-limit eviction keeps consecutive matrix states in both history directions.
  auto states = budget_setup(false);
  int cursor = 20, undone = 0;
  for (int i = 0; i < 17; i++) {
    undo();
    if (flags.f.message)
      break;
    cursor--;
    undone++;
    check(matrix_state() == states[cursor],
          "64KiB eviction retains consecutive matrix UNDO states");
  }
  check(undone > 0 && undone < 16,
        "matrix history actually reaches byte budget before count limit");
  for (int i = 0; i < undone; i++) {
    redo();
    cursor++;
    check(!flags.f.message && matrix_state() == states[cursor],
          "64KiB eviction retains consecutive matrix REDO states");
  }
  check(cursor == 20, "budget-limited REDO reaches original latest state");

  // Program snapshots and stack records share a budget without breaking pending REDO order.
  states = budget_setup(true);
  cursor = 20;
  for (int i = 0; i < 4; i++) {
    undo();
    cursor--;
    check(!flags.f.message && matrix_state() == states[cursor],
          "mixed-budget fixture creates consecutive pending REDO states");
  }
  flags.f.prgm_mode = true;
  pc = -1;
  {
    custom_history_scope scope;
    pc = 0;
  }
  flags.f.prgm_mode = false;
  int redone = 0;
  for (int i = 0; i < 5; i++) {
    redo();
    if (flags.f.message)
      break;
    cursor++;
    redone++;
    check(matrix_state() == states[cursor],
          "PGM budget pressure removes only far-future REDO states");
  }
  printf("BUDGET mixed pressure retained %d of 4 REDO states; final matrix "
         "index%d\n",
         redone, cursor);
  check(redone > 0 && redone < 4,
        "large PGM snapshot actually evicts part of pending REDO chain");
  program_edge_cases();

  // Repeated initialization and cleanup safely release all session-only history.
  for (int i = 0; i < 5; i++) {
    core_cleanup();
    core_cleanup();
    core_init(0, 0, NULL, 0);
  }
  core_cleanup();
  printf("%d pure history checks passed\n", checks);
}
