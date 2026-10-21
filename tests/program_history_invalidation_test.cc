#include "core_history_custom.h"
#include "core_test_support.h"
static bool queued;
static int repeat;
static void xy(int a, int b) {
  free_vartype(stack[sp - 1]);
  stack[sp - 1] = new_real(a);
  free_vartype(stack[sp]);
  stack[sp] = new_real(b);
}
static void command(int cmd) {
  core_keydown_command_id(cmd, &queued, &repeat);
  core_keyup();
}
static void undo() {
  core_undo(&queued, &repeat);
  core_keyup();
}
static void redo() {
  core_redo(&queued, &repeat);
  core_keyup();
}
static void reset() {
  hard_reset(0);
  custom_history_reset();
}

#include "core_commands1.h"
static arg_struct none() {
  arg_struct a = {};
  a.type = ARGTYPE_NONE;
  return a;
}
static arg_struct named(const char *s) {
  arg_struct a = {};
  a.type = ARGTYPE_STR;
  a.length = strlen(s);
  memcpy(a.val.text, s, a.length);
  return a;
}
static void native(int cmd, arg_struct a) {
  pending_command = cmd;
  pending_command_arg = a;
  core_keyup();
}
static void setup() {
  reset();
  flags.f.prgm_mode = true;
  pc = -1;
  command(CMD_ADD);
  command(CMD_MUL);
}
static std::vector<std::vector<unsigned char>> allprograms() {
  std::vector<std::vector<unsigned char>> v;
  for (int i = 0; i < prgms_count; i++)
    v.emplace_back(prgms[i].text, prgms[i].text + prgms[i].size);
  return v;
}
int main() {
  core_init(0, 0, NULL, 0);

  // Normal-mode CLALL prevents UNDO from restoring the deleted programs.
  setup();
  flags.f.prgm_mode = false;
  native(CMD_CLALLb, none());
  auto cleared = allprograms();
  flags.f.prgm_mode = true;
  undo();
  check(allprograms() == cleared && flags.f.message,
        "normal CLALL cannot resurrect program with UNDO");

  // CLALL also invalidates editing history when it leaves program mode itself.
  setup();
  native(CMD_CLALLb, none());
  check(!flags.f.prgm_mode, "CLALL leaves program editing");
  cleared = allprograms();
  flags.f.prgm_mode = true;
  undo();
  check(allprograms() == cleared,
        "CLALL from program mode invalidates program history");

  // Deleting a named program outside the editor cannot be undone with an old snapshot.
  reset();
  flags.f.prgm_mode = true;
  arg_struct label = named("VICTIM");
  store_command(0, CMD_LBL, &label, NULL);
  pc = 0;
  custom_history_reset();
  command(CMD_ADD);
  command(CMD_MUL);
  flags.f.prgm_mode = false;
  native(CMD_CLP, label);
  cleared = allprograms();
  flags.f.prgm_mode = true;
  undo();
  check(allprograms() == cleared && flags.f.message,
        "normal CLP cannot resurrect deleted label or program");

  // Creating a program outside the editor invalidates the previous program snapshots.
  setup();
  flags.f.prgm_mode = false;
  command(CMD_GTODOTDOT);
  auto moved = allprograms();
  flags.f.prgm_mode = true;
  undo();
  check(allprograms() == moved && flags.f.message,
        "normal GTO.. invalidates program history");

  // The same program-creation command remains undoable when captured as an editor operation.
  setup();
  auto before = allprograms();
  command(CMD_GTODOTDOT);
  auto after = allprograms();
  int chosen = current_prgm;
  undo();
  check(allprograms() == before, "program GTO.. remains undoable");
  redo();
  check(allprograms() == after && current_prgm == chosen,
        "program GTO.. remains redoable");

  // DEL inside program editing restores the exact instructions before and after deletion.
  setup();
  before = allprograms();
  arg_struct n = {};
  n.type = ARGTYPE_NUM;
  n.val.num = 1;
  native(CMD_DEL, n);
  after = allprograms();
  check(after != before, "program DEL modifies instruction bytes");
  undo();
  check(allprograms() == before, "program DEL remains undoable");
  redo();
  check(allprograms() == after, "program DEL remains redoable");

  // CLP inside program editing similarly retains both valid program states.
  reset();
  flags.f.prgm_mode = true;
  store_command(0, CMD_LBL, &label, NULL);
  pc = 0;
  command(CMD_ADD);
  custom_history_reset();
  before = allprograms();
  native(CMD_CLP, label);
  after = allprograms();
  check(after != before, "program CLP modifies instruction bytes");
  undo();
  check(allprograms() == before, "program CLP remains undoable");
  redo();
  check(allprograms() == after, "program CLP remains redoable");

  // Cancelling the native CLALL confirmation preserves the preceding edit record.
  setup();
  before = allprograms();
  command(CMD_CLALLa);
  check(mode_clall, "CLALL confirmation opens in program mode");
  press(KEY_EXIT);
  check(!mode_clall, "CLALL confirmation cancels");
  undo();
  check(program_commands() == std::vector<int>({CMD_ADD, CMD_END}),
        "cancelled CLALL preserves program UNDO");
  redo();
  check(allprograms() == before, "cancelled CLALL preserves program REDO");

  // Normal arithmetic and excluded settings leave the separate editor lane intact.
  setup();
  before = allprograms();
  flags.f.prgm_mode = false;
  xy(10, 2);
  command(CMD_ADD);
  command(CMD_DEG);
  flags.f.prgm_mode = true;
  undo();
  check(program_commands() == std::vector<int>({CMD_ADD, CMD_END}),
        "normal calculation and setting preserve program UNDO");
  redo();
  check(allprograms() == before,
        "normal calculation and setting preserve program REDO");

  // A failed deletion must not discard the last successful edit record.
  reset();
  flags.f.prgm_mode = true;
  store_command(0, CMD_LBL, &label, NULL);
  pc = 0;
  custom_history_reset();
  before = allprograms();
  command(CMD_ADD);
  flags.f.prgm_mode = false;
  native(CMD_CLP, named("MISSING"));
  flags.f.prgm_mode = true;
  undo();
  check(allprograms() == before, "failed nonexistent CLP preserves program UNDO");

  // DEL outside the editor is an uncaptured program mutation and ends that lane.
  setup();
  flags.f.prgm_mode = false;
  native(CMD_DEL, n);
  cleared = allprograms();
  flags.f.prgm_mode = true;
  undo();
  check(allprograms() == cleared && flags.f.message,
        "normal DEL invalidates program history");

  core_cleanup();
  printf("%d program invalidation checks passed\n", checks);
}
