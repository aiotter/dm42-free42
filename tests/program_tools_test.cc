// Exercise editor actions through the real core and normal key-release lifecycle.
#include "core_test_support.h"
#include "core_program_tools_custom.h"

static void open_jump() {
  bool queued = true;
  int repeat = 77;
  core_program_label_jump(&queued, &repeat);
  assert(!queued && repeat == 0);
  core_keyup();
}

static void test_label_jump() {
  // Editor-only actions reject normal mode before starting command entry.
  hard_reset(0);
  open_jump();
  check(!mode_command_entry && !core_menu(), "normal mode refuses program label jump");

  // Opening and cancelling LBLs leaves program contents and the editor position intact.
  flags.f.prgm_mode = true;
  arg_struct label = {};
  label.type = ARGTYPE_STR; label.length = 4;
  memcpy(label.val.text, "TEST", 4);
  store_command(0, CMD_LBL, &label, NULL);
  pc = get_command_length(current_prgm, 0);
  arg_struct number = {};
  number.type = ARGTYPE_DOUBLE; number.val_d = 42;
  store_command(pc, CMD_NUMBER, &number, NULL);
  int saved_pc = pc, saved_highlight = prgm_highlight_row;
  std::vector<unsigned char> original = program();
  auto original_commands = program_commands();
  open_jump();
  check(mode_command_entry && incomplete_command == CMD_GTODOT
      && get_cat_section() == CATSECT_PGM_ONLY, "editor opens GTO-dot program-only catalog");
  check(program() == original, "opening catalog does not insert program instructions");
  press(KEY_EXIT);
  check(!mode_command_entry && !core_menu() && pc == saved_pc
      && prgm_highlight_row == saved_highlight, "EXIT restores the original editor position");
  check(program() == original, "cancel preserves program bytes");

  // Selection uses the native catalog's current label index rather than fixed label ordering.
  open_jump();
  int item = get_cat_item(0);
  assert(item >= 0);
  int target_prgm = labels[item].prgm, target_pc = labels[item].pc;
  press(KEY_SIGMA);
  check(!mode_command_entry && current_prgm == target_prgm && pc == target_pc,
      "catalog selection jumps to the selected label");
  check(flags.f.prgm_mode && program() == original,
      "jump preserves program mode and program bytes");

  // Label navigation records positions so the history actions can reverse and replay it.
  bool history_queued;
  int history_repeat;
  core_undo(&history_queued, &history_repeat); core_keyup();
  check(pc == saved_pc && program() == original,
      "UNDO restores the editor position before label selection");
  core_redo(&history_queued, &history_repeat); core_keyup();
  check(current_prgm == target_prgm && pc == target_pc && program() == original,
      "REDO restores the selected label without editing the program");

  // Numeric input is committed once before LBLs changes the command-entry context.
  pc = saved_pc;
  press(KEY_1);
  assert(mode_number_entry);
  int edited_pc = pc;
  open_jump();
  check(!mode_number_entry && mode_command_entry && incomplete_saved_pc == edited_pc,
      "number entry is committed before opening label catalog");
  std::vector<unsigned char> after_number = program();
  int4 number_position = edited_pc;
  int committed_command;
  arg_struct committed_argument;
  get_next_command(&number_position, &committed_command, &committed_argument, 0, NULL);
  check(program_commands().size() == original_commands.size() + 1 &&
      program_commands() == std::vector<int>({CMD_LBL, CMD_NUMBER, CMD_NUMBER, CMD_END}) &&
      committed_command == CMD_NUMBER && committed_argument.val_d == 1,
      "pending number is stored once with its entered value");
  press(KEY_EXIT);
  check(pc == edited_pc && program() == after_number,
      "cancel after number entry retains the committed number and position");

  // Navigation must not take over the editor while a program is executing.
  mode_running = true;
  open_jump();
  check(!mode_command_entry, "running program refuses editor jump");
  mode_running = false;

  flags.f.prgm_mode = false;
}

struct fixture {
  int main_program, target_program;
  int sto_first, sto_add, hello, sto_second, global_call, local_goto, local_label;
  int letter_call, letter_label, indirect_call, xstr, number;
};
static int append(int command, arg_struct arg, const char *text = NULL) {
  store_command_after(&pc, command, &arg, text);
  return pc;
}
static arg_struct numeric(int n) { arg_struct a = {}; a.type=ARGTYPE_NUM; a.val.num=n; return a; }
static arg_struct named(const char *s) {
  arg_struct a = {}; a.type=ARGTYPE_STR; a.length=strlen(s); memcpy(a.val.text,s,a.length); return a;
}
static fixture setup() {
  hard_reset(0); custom_program_tools_reset(); flags.f.prgm_mode=true; flags.f.message=0; pc=-1;
  fixture f = {}; f.main_program=current_prgm;
  append(CMD_LBL,named("MAIN"));
  f.sto_first=append(CMD_STO,numeric(0));
  f.sto_add=append(CMD_STO_ADD,named("ABC"));
  f.hello=append(CMD_STRING,named("Hello ABC World"));
  f.sto_second=append(CMD_STO,numeric(99));
  f.global_call=append(CMD_XEQ,named("TARGET"));
  f.local_goto=append(CMD_GTO,numeric(1));
  f.local_label=append(CMD_LBL,numeric(1));
  arg_struct letter={}; letter.type=ARGTYPE_LCLBL; letter.val.lclbl='A';
  f.letter_call=append(CMD_XEQ,letter);
  f.letter_label=append(CMD_LBL,letter);
  arg_struct indirect=numeric(3); indirect.type=ARGTYPE_IND_NUM;
  f.indirect_call=append(CMD_GTO,indirect);
  arg_struct longtext={}; longtext.type=ARGTYPE_XSTR;
  longtext.val.xstr="long XYZ literal beyond fifteen characters";
  longtext.length=strlen(longtext.val.xstr);
  f.xstr=append(CMD_XSTR,longtext);
  arg_struct number={}; number.type=ARGTYPE_DOUBLE; number.val_d=phloat("1.23");
  f.number=append(CMD_NUMBER,number,"1.2300");
  goto_dot_dot(true); f.target_program=current_prgm;
  append(CMD_LBL,named("TARGET")); append(CMD_STO,numeric(77));
  current_prgm=f.main_program; pc=f.sto_first; prgm_highlight_row=1;
  return f;
}
static std::vector<std::vector<unsigned char>> programs() {
  std::vector<std::vector<unsigned char>> bytes;
  for(int i=0;i<prgms_count;i++) bytes.emplace_back(prgms[i].text,prgms[i].text+prgms[i].size);
  return bytes;
}
static void follow() { bool queued=true; int repeat=77; core_program_follow(&queued,&repeat); assert(!queued&&repeat==0); core_keyup(); }
static void begin_find(bool reverse = false) { bool queued=true; int repeat=77; core_program_find(reverse,&queued,&repeat); assert(!queued&&repeat==0); core_keyup(); }
static void again() { bool queued=true; int repeat=77; core_repeat_last_command(&queued,&repeat); assert(!queued&&repeat==0); core_keyup(); }
static void query_command(const char *command, bool reverse = false) {
  begin_find(reverse); bool queued; int repeat;
  core_keydown_command(command, false, &queued, &repeat); core_keyup();
}
static void query_text(const char *text, bool reverse = false) {
  begin_find(reverse); press(KEY_SHIFT); press(KEY_ENTER);
  assert(mode_command_entry && get_front_menu()==MENU_ALPHA1);
  for(const char *p=text;*p;p++) {
    const char character[] = {*p, 0};
    bool queued; int repeat;
    core_keydown_command(character, true, &queued, &repeat);
    core_keyup();
  }
  press(KEY_ENTER);
}
static void test_follow_and_find() {
  // FOLLOW resolves direct global and local branch targets without executing or editing.
  fixture f=setup();
  auto bytes=programs();
  pc=f.global_call; follow();
  check(current_prgm==f.target_program&&pc==0,"FOLLOW jumps to the named global label without running it");
  check(programs()==bytes&&flags.f.prgm_mode,"FOLLOW preserves all program bytes and editor mode");
  current_prgm=f.main_program; pc=f.local_goto; follow();
  check(pc==f.local_label,"FOLLOW resolves numeric local labels");
  pc=f.letter_call; follow(); check(pc==f.letter_label,"FOLLOW resolves letter local labels");
  pc=f.indirect_call; follow(); check(pc==f.indirect_call&&flags.f.message,"FOLLOW rejects indirect calls without changing position");
  flags.f.message=false; pc=f.sto_first; follow(); check(pc==f.sto_first&&flags.f.message,"FOLLOW rejects a non-branch instruction");

  // Command FIND ignores operands, distinguishes commands, and wraps only in the current program.
  f=setup(); bytes=programs();
  query_command("STO");
  check(pc==f.sto_second&&!core_menu(),"FIND STO ignores arguments and selects the next STO");
  query_command("STO");
  check(pc==f.sto_first&&current_prgm==f.main_program,"FIND wraps within the current program");
  query_command("STO+");
  check(pc==f.sto_add,"FIND distinguishes STO from STO+");
  check(programs()==bytes,"command queries never insert or modify instructions");
  int before=pc;
  query_command("SIN");
  check(pc==before&&flags.f.message,"unmatched command search leaves position unchanged");

  // Text FIND searches string operands case-sensitively, including long string instructions.
  f=setup(); bytes=programs(); pc=f.sto_add;
  query_text("ABC");
  check(pc==f.hello,"FIND ALPHA finds a case-sensitive substring in a string literal");
  query_text("ABC");
  check(pc==f.sto_add,"text FIND wraps and includes command string arguments");
  query_text("XYZ");
  check(pc==f.xstr,"text FIND includes long XSTR operands");
  before=pc; query_text("xyz");
  check(pc==before&&flags.f.message,"text FIND preserves case distinctions");
  flags.f.message=false; query_text("");
  check(pc==before&&!mode_command_entry&&!core_menu(),"empty text query cancels without inserting XSTR");
  check(programs()==bytes,"text queries preserve all program bytes");

  // FIND owns input until a command is selected or cancelled; physical keys must not insert code.
  f=setup(); bytes=programs();
  begin_find(); check(core_menu()&&get_front_menu()==MENU_BLANK,"FIND waiting owns a blank core menu instead of home bindings");
  before=pc; press(KEY_SIGMA);
  check(pc==before&&get_front_menu()==MENU_BLANK&&programs()==bytes,"blank FIND softkeys cannot insert instructions or enter ALPHA");
  bool queued; int repeat;
  core_keydown_ex(KEY_STO,&queued,&repeat,1); core_keyup();
  check(pc==f.sto_second&&!core_menu(),"physical STO key is accepted as a command query");
  pc=f.sto_first; begin_find(); press(KEY_EXIT);
  check(pc==f.sto_first&&!core_menu()&&programs()==bytes,"EXIT cancels command FIND without editing");
  begin_find(); press(KEY_1);
  check(!mode_number_entry&&programs()==bytes,"numeric input cannot accidentally edit while waiting for a FIND command");
  press(KEY_BSP); check(!core_menu(),"backspace cancels a waiting command query");

  // Cancelling text entry or leaving program mode restores native input ownership.
  begin_find(); press(KEY_SHIFT); press(KEY_ENTER);
  core_keydown_command("X", true, &queued, &repeat); core_keyup(); press(KEY_EXIT);
  check(!mode_command_entry&&!core_menu()&&pc==f.sto_first&&programs()==bytes,"EXIT cancels text FIND and keeps the editor position");
  begin_find(); press(KEY_SHIFT); press(KEY_RUN);
  check(!flags.f.prgm_mode&&!mode_command_entry&&!core_menu(),"leaving program mode cancels FIND cleanly");

  // Read-only navigation remains available when the selected program is locked.
  f=setup(); prgms[f.main_program].locked=true;
  pc=f.global_call; follow(); check(current_prgm==f.target_program,"FOLLOW works in a locked program");
  current_prgm=f.main_program; pc=f.sto_first; query_text("ABC");
  check(pc==f.sto_add,"FIND works in a locked program without asking to edit it");
  prgms[f.main_program].locked=false;

  // An unfinished numeric instruction cannot be replaced by another editor action.
  f=setup(); press(KEY_1); assert(mode_number_entry); before=pc; bytes=programs();
  follow(); check(mode_number_entry&&pc==before&&programs()==bytes,"FOLLOW leaves an unfinished numeric instruction untouched");
  begin_find(); check(mode_number_entry&&pc==before&&programs()==bytes,"FIND leaves an unfinished numeric instruction untouched");

  // Searching must preserve the stored numeric source even when display formatting changed.
  f=setup(); pc=f.sto_first; bytes=programs(); flags.f.decimal_point=!flags.f.decimal_point;
  query_command("SIN");
  check(programs()==bytes,"search does not decode and rewrite unrelated numeric source text");

  // Commands chosen through CUSTOM enter FIND instead of being inserted as XEQ instructions.
  f=setup(); bytes=programs();
  begin_find();
  press(KEY_SHIFT); press(KEY_2);
  check(get_front_menu()==MENU_CUSTOM1,"FIND can open the existing CUSTOM menu to choose a command");
  flags.f.local_label=true;
  press(KEY_SIGMA);
  check(pc==f.global_call&&programs()==bytes&&!core_menu(),"CUSTOM local-label XEQ selection searches without inserting XEQ");

  // A destructive command selected through CUSTOM must be searched, never confirmed or executed.
  f=setup(); assign_custom_key(1,"CLALL",5); bytes=programs();
  begin_find(); press(KEY_SHIFT); press(KEY_2); press(KEY_SIGMA);
  check(!mode_clall&&programs()==bytes&&prgms_count==2,
      "CUSTOM CLALL query cannot open or execute the destructive confirmation");

  // Ordinary function-menu selection is intercepted before the native insertion path.
  f=setup(); pc=f.number; arg_struct noarg={}; noarg.type=ARGTYPE_NONE;
  int conversion=append(CMD_TO_DEG,noarg); pc=f.sto_first; bytes=programs();
  begin_find(); press(KEY_SHIFT); press(KEY_5);
  check(get_front_menu()==MENU_CONVERT1,"FIND can browse the normal CONVERT menu");
  press(KEY_SIGMA);
  check(pc==conversion&&programs()==bytes&&!core_menu(),"menu function selection searches without executing or inserting the function");

  // Catalog lookup follows the actual catalog rows, so upstream ordering changes are harmless.
  f=setup(); bytes=programs(); begin_find();
  press(KEY_SHIFT); press(KEY_ADD);
  assert(get_front_menu()==MENU_CATALOG);
  set_cat_section(CATSECT_FCN); move_cat_row(0); redisplay();
  int selected=-1;
  const int first_row = get_cat_row();
  do {
    for(int slot=0;slot<6;slot++) if(get_cat_item(slot)==CMD_STO) selected=slot;
    if(selected<0) {move_cat_row(1);redisplay();}
  } while(selected < 0 && get_cat_row() != first_row);
  assert(selected>=0);
  press(KEY_SIGMA+selected);
  check(pc==f.sto_second&&programs()==bytes&&!core_menu(),"function-catalog choice is interpreted as a FIND command");

  // Text queries inspect indirect names without dereferencing their variable contents.
  f=setup(); pc=f.number;
  arg_struct indirect_text=named("INDONLY"); indirect_text.type=ARGTYPE_IND_STR;
  int indirect_position=append(CMD_GTO,indirect_text);
  pc=f.sto_first; bytes=programs(); query_text("ONLY");
  check(pc==indirect_position&&programs()==bytes,"text FIND includes indirect string operands without evaluating them");

  // FOLLOW records navigation in program history without changing any instruction bytes.
  f=setup(); pc=f.global_call; bytes=programs(); follow();
  bool history_queued; int history_repeat;
  core_undo(&history_queued,&history_repeat); core_keyup();
  check(current_prgm==f.main_program&&pc==f.global_call&&programs()==bytes,
      "UNDO restores the position before FOLLOW without rewriting program bytes");
  core_redo(&history_queued,&history_repeat); core_keyup();
  check(current_prgm==f.target_program&&pc==0,"REDO restores a followed global-label location");

  // Command FIND positions are undoable and redoable through the public history API.
  f=setup(); pc=f.sto_first; query_command("STO");
  core_undo(&history_queued,&history_repeat); core_keyup();
  check(pc==f.sto_first,"UNDO restores position before a command FIND");
  core_redo(&history_queued,&history_repeat); core_keyup();
  check(pc==f.sto_second,"REDO restores command FIND match");

  // Text FIND uses the same position history as command FIND.
  f=setup(); pc=f.sto_first; query_text("XYZ");
  core_undo(&history_queued,&history_repeat); core_keyup();
  check(pc==f.sto_first,"UNDO restores position before text FIND");
  core_redo(&history_queued,&history_repeat); core_keyup();
  check(pc==f.xstr,"REDO restores text FIND match");

}

static void test_reverse_find() {
  // Reverse queries select the closest earlier match before wrapping within the same program.
  fixture f=setup(); auto bytes=programs(); pc=f.sto_second;
  query_command("STO",true);
  check(pc==f.sto_first,"reverse command FIND selects the preceding STO, ignoring STO+");
  query_command("STO",true);
  check(pc==f.sto_second&&current_prgm==f.main_program,"reverse FIND wraps without visiting another program");
  for(int start : {-1, 0, prgms[f.main_program].size-2}) {
    pc=start; query_command("STO",true);
    check(pc==f.sto_second,"reverse FIND handles the program header, first instruction, and END");
  }
  query_command("STO+",true); query_command("STO+",true);
  check(pc==f.sto_add,"reverse FIND selects the current line only after one full cycle");
  int before=pc, highlight=prgm_highlight_row;
  query_command("SIN",true);
  check(pc==before&&prgm_highlight_row==highlight&&flags.f.message,
      "unmatched reverse command FIND preserves the editor position");
  check(programs()==bytes,"reverse command FIND preserves all program bytes");

  // Text search retains its direction across ALPHA entry and variable-length operands.
  f=setup(); bytes=programs(); pc=f.hello;
  query_text("ABC",true);
  check(pc==f.sto_add,"reverse text FIND selects the preceding matching string operand");
  query_text("ABC",true);
  check(pc==f.hello,"reverse text FIND wraps to the last matching string operand");
  query_text("XYZ",true);
  check(pc==f.xstr,"reverse text FIND finds long XSTR operands after wrapping");
  query_text("XYZ",true);
  check(pc==f.xstr,"a sole text match remains selected after a reverse cycle");
  before=pc; query_text("xyz",true);
  check(pc==before&&flags.f.message,"reverse text FIND remains case-sensitive");
  query_text("",true);
  check(pc==before&&!mode_command_entry&&!core_menu(),"empty reverse text FIND cancels cleanly");
  check(programs()==bytes,"reverse text FIND never changes instructions or numeric source text");

  // Cancelling or leaving program mode must not leak reverse direction into the next FIND.
  // Three STO matches make the forward and reverse destinations different.
  pc=f.number; append(CMD_STO,numeric(7));
  pc=f.sto_first; begin_find(true); press(KEY_EXIT); query_command("STO");
  check(pc==f.sto_second,"forward FIND remains forward after cancelling reverse command FIND");
  pc=f.sto_first; begin_find(true); press(KEY_SHIFT); press(KEY_ENTER); press(KEY_EXIT);
  query_text("ABC");
  check(pc==f.sto_add,"forward text FIND remains forward after cancelling reverse text FIND");
  begin_find(true); press(KEY_SHIFT); press(KEY_RUN);
  check(!flags.f.prgm_mode&&!core_menu(),"leaving program mode cancels reverse FIND");
  flags.f.prgm_mode=true; pc=f.sto_first; query_command("STO");
  check(pc==f.sto_second,"re-entering program mode restores ordinary forward FIND");

  // Reverse navigation uses the existing history lifecycle, including locked programs.
  f=setup(); bytes=programs(); pc=f.sto_second; prgms[f.main_program].locked=true;
  query_command("STO",true);
  check(pc==f.sto_first&&programs()==bytes,"reverse FIND supports locked programs without editing");
  bool queued; int repeat;
  core_undo(&queued,&repeat); core_keyup();
  check(pc==f.sto_second&&programs()==bytes,"UNDO restores the position before reverse command FIND");
  core_redo(&queued,&repeat); core_keyup();
  check(pc==f.sto_first&&programs()==bytes,"REDO restores the reverse command FIND match");
  query_text("ABC",true);
  check(pc==f.hello,"reverse text FIND supports locked programs");
  core_undo(&queued,&repeat); core_keyup();
  check(pc==f.sto_first,"UNDO restores the position before reverse text FIND");
  core_redo(&queued,&repeat); core_keyup();
  check(pc==f.hello&&programs()==bytes,"REDO restores the reverse text FIND match without editing");
  prgms[f.main_program].locked=false;

  // Empty programs and incomplete number entry retain the existing FIND boundaries.
  hard_reset(0); custom_program_tools_reset(); flags.f.prgm_mode=true; pc=-1;
  query_command("END",true);
  check(pc==0,"reverse FIND searches END in an otherwise empty program");
  f=setup(); press(KEY_1); before=pc; bytes=programs(); begin_find(true);
  check(mode_number_entry&&pc==before&&programs()==bytes,
      "reverse FIND cannot take over unfinished number entry");
}

static void test_find_again() {
  // Three matches distinguish the two directions when AGAIN advances and wraps.
  for(bool reverse : {false,true}) {
    fixture f=setup(); pc=f.number;
    int third=append(CMD_STO,numeric(7)); pc=f.sto_first;
    auto bytes=programs();
    query_command("STO",reverse);
    check(pc==(reverse?third:f.sto_second),"command FIND records its selected direction");
    again();
    check(pc==(reverse?f.sto_second:third)&&!core_menu(),
        "AGAIN finds the next command match without reopening the prompt");
    again();
    check(pc==f.sto_first&&programs()==bytes,"AGAIN wraps in the saved command-search direction without editing");
    bool queued; int repeat;
    core_undo(&queued,&repeat); core_keyup();
    check(pc==(reverse?f.sto_second:third),"UNDO reverses one repeated FIND movement");
    core_redo(&queued,&repeat); core_keyup();
    check(pc==f.sto_first,"REDO restores one repeated FIND movement");
    again();
    check(pc==(reverse?third:f.sto_second)&&programs()==bytes,
        "AGAIN retains the command query and direction across UNDO and REDO");

    f=setup(); pc=f.number;
    third=append(CMD_STRING,named("ABC LAST")); pc=f.sto_add; bytes=programs();
    query_text("ABC",reverse);
    check(pc==(reverse?third:f.hello),"text FIND records its selected direction");
    again();
    check(pc==(reverse?f.hello:third),"AGAIN reuses the text substring and direction");
    again();
    check(pc==f.sto_add&&programs()==bytes,"repeated text FIND wraps without changing operands");
  }

  // New queries replace previous queries; cancelled or empty queries do not.
  fixture f=setup(); pc=f.number;
  int third=append(CMD_STO,numeric(7)); pc=f.sto_first;
  query_command("STO",true);
  auto bytes=programs();
  begin_find(false); press(KEY_EXIT); again();
  check(pc==f.sto_second,"cancelling a new command query preserves the last FIND direction");
  begin_find(false); press(KEY_SHIFT); press(KEY_ENTER);
  bool queued; int repeat;
  core_keydown_command("Z",true,&queued,&repeat); core_keyup(); press(KEY_EXIT); again();
  check(pc==f.sto_first,"cancelled text input does not replace the saved command query");
  query_text(""); again();
  check(pc==third,"an empty text query preserves the last completed FIND");
  query_text("ABC"); again();
  check(pc==f.hello,"a completed text query replaces a saved command query");
  query_command("STO"); again();
  check(pc==third&&programs()==bytes,"a completed command query replaces a saved text query");

  // Query text is owned by FIND, not by the temporary ALPHA entry buffer.
  f=setup(); query_text("ABC"); bytes=programs();
  begin_find(true); press(KEY_SHIFT); press(KEY_ENTER);
  core_keydown_command("Z",true,&queued,&repeat); core_keyup(); press(KEY_EXIT); again();
  check(pc==f.hello&&programs()==bytes,"later cancelled text entry cannot overwrite the saved search text");
  query_command("SIN"); int before=pc; again();
  check(pc==before&&flags.f.message&&programs()==bytes,
      "AGAIN repeats an unmatched FIND instead of inserting the queried instruction");

  // Read-only FIND can repeat in locked programs, but it cannot consume pending input.
  f=setup(); query_command("STO"); prgms[f.main_program].locked=true; bytes=programs(); again();
  check(pc==f.sto_first&&programs()==bytes,"AGAIN repeats FIND in a locked program without editing");
  prgms[f.main_program].locked=false;
  begin_find(true); before=pc; again();
  check(pc==before&&get_front_menu()==MENU_BLANK&&programs()==bytes,
      "AGAIN leaves an unconfirmed command query open");
  press(KEY_EXIT); again();
  check(pc==f.sto_second,"the saved FIND remains available after cancelling unconfirmed input");
  begin_find(true); press(KEY_SHIFT); press(KEY_ENTER); before=pc; again();
  check(pc==before&&mode_command_entry&&programs()==bytes,
      "AGAIN leaves an unconfirmed text query open");
  press(KEY_EXIT);
  press(KEY_1); before=pc; bytes=programs(); again();
  check(pc==before&&mode_number_entry&&programs()==bytes,
      "repeating FIND cannot commit unfinished number entry");

  // Ordinary instruction input replaces FIND as the last program action.
  f=setup(); query_command("STO");
  core_keydown_command("SIN",false,&queued,&repeat); core_keyup();
  size_t count=program_commands().size(); again();
  int command; arg_struct arg={}; int4 position=pc;
  get_next_command(&position,&command,&arg,0,NULL);
  check(command==CMD_SIN&&program_commands().size()==count+1,
      "AGAIN inserts the last instruction after an edit replaces FIND");
  prgms[f.main_program].locked=true; bytes=programs(); before=pc; again();
  check(pc==before&&flags.f.message&&programs()==bytes,
      "ordinary instruction repetition still rejects locked programs");
  prgms[f.main_program].locked=false;

  // Normal calculations and editor searches retain independent repeat records.
  f=setup(); pc=f.number; third=append(CMD_STO,numeric(7)); pc=f.sto_first;
  query_command("STO"); flags.f.prgm_mode=false;
  ((vartype_real *)stack[sp])->x=2;
  core_keydown_command("CHS",false,&queued,&repeat); core_keyup(); again();
  check(((vartype_real *)stack[sp])->x==2,"normal-mode AGAIN repeats a calculation instead of the saved FIND");
  flags.f.prgm_mode=true; bytes=programs(); again();
  check(pc==third&&programs()==bytes,"normal-mode calculations preserve program-mode FIND repetition");

  // Resetting from normal mode clears both lanes, including the saved editor search.
  flags.f.prgm_mode=false; hard_reset(0); flags.f.prgm_mode=true; pc=-1; bytes=programs(); again();
  check(pc==-1&&flags.f.message&&programs()==bytes,"reset clears FIND from AGAIN history");
}

static void test_again_record_transitions() {
  auto instruction = [](int position) -> std::vector<unsigned char> {
    const unsigned char *start=prgms[current_prgm].text+position;
    return std::vector<unsigned char>(start,start+get_command_length(current_prgm,position));
  };

  // A fresh repeat record can become a FIND action and can be cleared without reviving an old command.
  fixture f=setup(); custom_history_reset(); query_command("STO");
  auto bytes=programs(); again();
  check(pc==f.sto_first&&programs()==bytes,"an empty AGAIN record can become a FIND action");
  custom_history_program_action(NULL); int before=pc; again();
  check(pc==before&&flags.f.message&&programs()==bytes,"clearing an AGAIN action leaves an empty record");

  // Owned strings and numeric source survive command/action transitions and inflight replacement.
  f=setup(); pc=f.number;
  char source[40]; memset(source,'A',sizeof(source));
  arg_struct text={}; text.type=ARGTYPE_XSTR; text.length=sizeof(source); text.val.xstr=source;
  int original=append(CMD_XSTR,text); auto expected=instruction(original);
  memset(source,'B',sizeof(source)); again();
  check(instruction(pc)==expected,"an owning command payload repeats its own long-string bytes");
  query_text("ABC"); bytes=programs(); again();
  check(pc==f.hello&&programs()==bytes,"an owning command payload can be replaced by a FIND action");
  pc=f.number;
  char digits[]="1.2300";
  arg_struct number={}; number.type=ARGTYPE_DOUBLE; number.val_d=phloat("1.23");
  original=append(CMD_NUMBER,number,digits); expected=instruction(original);
  memset(digits,'9',strlen(digits)); again();
  check(instruction(pc)==expected,"a FIND action can become a command that owns its original numeric source");
  int command; arg_struct argument={}; int4 position=pc;
  get_next_command(&position,&command,&argument,0,NULL);
  check(command==CMD_NUMBER&&argument.val_d==phloat("1.23"),
      "the constructed numeric command payload preserves its phloat value");
  custom_history_reset(); before=pc; bytes=programs(); again();
  check(pc==before&&flags.f.message&&programs()==bytes,
      "reset destroys owning last-command and inflight payloads without reviving them");

  // Saving a new edit may fail, but it must clear the old action/command rather than leave a partial record.
  for(bool was_action : {false,true}) for(bool numeric_input : {false,true}) {
    f=setup(); pc=f.number;
    append(CMD_XSTR,text); again();
    if(was_action) query_command("STO");
    number.val_d=phloat("7.5");
    int insertion=pc+get_command_length(current_prgm,pc);
    journal_test_fail_after=1;
    bool stored=store_command(insertion,numeric_input?CMD_NUMBER:CMD_XSTR,
        numeric_input?&number:&text,numeric_input?"7.500":NULL);
    bool injected=journal_test_fail_after==0; journal_test_fail_after=0;
    check(stored&&injected,"the program edit succeeds when saving its AGAIN payload runs out of memory");
    pc=insertion; bytes=programs(); again();
    check(pc==insertion&&flags.f.message&&programs()==bytes,
        "a failed command save leaves neither a stale FIND action nor an older owning command repeatable");
    custom_history_reset();
  }

  // Failure while copying to inflight preserves the saved command and permits a later retry.
  for(bool numeric_input : {false,true}) {
    f=setup(); pc=f.number;
    number.val_d=phloat("7.5");
    append(numeric_input?CMD_XSTR:CMD_NUMBER,numeric_input?text:number,numeric_input?NULL:"7.500");
    again();
    original=append(numeric_input?CMD_NUMBER:CMD_XSTR,numeric_input?number:text,numeric_input?"7.500":NULL);
    expected=instruction(original); bytes=programs(); before=pc;
    // Establish the outer history scope before targeting the payload-copy allocation.
    custom_history_begin(); journal_test_fail_after=1; again();
    bool injected=journal_test_fail_after==0; journal_test_fail_after=0; custom_history_end();
    check(injected&&pc==before&&flags.f.message&&programs()==bytes,
        "failed inflight allocation leaves the program and saved command intact");
    again();
    check(instruction(pc)==expected,"AGAIN retries the intact saved command after inflight allocation failure");
    custom_history_reset();
  }
}

int main() {
  setbuf(stdout, NULL);
  core_init(0, 0, NULL, 0);
  test_label_jump();
  test_follow_and_find();
  test_reverse_find();
  test_find_again();
  test_again_record_transitions();
  core_cleanup();
  printf("%d program-tool checks passed\n", checks);
}
