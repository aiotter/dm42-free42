// Exercise time formatting through the normal key lifecycle and both stack displays.
#define shell_blitter unused_time_test_blitter
#include "core_test_support.h"
#undef shell_blitter
#include "core_time_custom.h"
#include <unistd.h>

static char screen[17 * 16];
void shell_blitter(const char *bits, int bytes_per_line, int, int, int, int) {
  assert(bytes_per_line == 17);
  memcpy(screen, bits, sizeof(screen));
}

static void reset_time() {
  hard_reset(0);
  flags.f.message = flags.f.two_line_message = 0;
  flags.f.thousands_separators = 0;
  redisplay();
}

static void open_time() {
  bool queued = true;
  int repeat = -1;
  bool running = core_keydown_command("T.CALC", false, &queued, &repeat);
  assert(!running && !queued && repeat == 0);
  core_keyup();
  assert(custom_time_active() && mode_appmenu == MENU_TIME);
}

static void type_number(const char *text) {
  for (const char *p = text; *p; p++) {
    static const int digits[] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
        KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};
    if (*p >= '0' && *p <= '9')
      press(digits[*p - '0']);
    else if (*p == '.' || *p == ',')
      press(KEY_DOT);
    else {
      assert(*p == '-');
      press(KEY_CHS);
    }
  }
}

static phloat xvalue() {
  assert(sp >= 0 && stack[sp]->type == TYPE_REAL);
  return ((vartype_real *) stack[sp])->x;
}

static phloat parsed_number(const char *text) {
  std::string input(text);
  for (char &c : input)
    if (c == 'e' || c == 'E') c = 24;
  int decimal_point = flags.f.decimal_point;
  flags.f.decimal_point = 1;
  phloat number;
  int error = string2phloat(input.data(), input.size(), &number);
  flags.f.decimal_point = decimal_point;
  assert(error == 0);
  return number;
}

static void expect_hours(phloat hours, const char *expected, const char *name) {
  vartype *value = new_real(hours);
  phloat original = ((vartype_real *) value)->x;
  char text[100];
  int length = custom_time_format(value, text, sizeof(text));
  check(length == (int) strlen(expected)
      && memcmp(text, expected, strlen(expected)) == 0, name);
  check(((vartype_real *) value)->x == original,
      "formatting does not normalize or round the stored number");
  free_vartype(value);
}

static void expect_value(const char *number, const char *expected, const char *name) {
  expect_hours(parsed_number(number), expected, name);
}

static bool same_hours(phloat actual, phloat expected) {
#ifdef BCD_MATH
  const phloat tolerance = parsed_number("1e-31");
#else
  const phloat tolerance = parsed_number("1e-14");
#endif
  phloat difference = actual - expected;
  if (difference < 0) difference = -difference;
  if (expected < 0) expected = -expected;
  return difference <= tolerance * (expected + 1);
}

static void expect_entry(const char *expected, const char *name) {
  std::string original(cmdline, cmdline_length);
  char text[100];
  int length = custom_time_entry(cmdline, cmdline_length, text, sizeof(text));
  check(length == (int) strlen(expected)
      && memcmp(text, expected, strlen(expected)) == 0, name);
  check(std::string(cmdline, cmdline_length) == original,
      "entry formatting preserves the packed HMS input string");
}

static void expect_row(int row, const char *expected, const char *name) {
  // Compare actual LCD pixels with independently specified display text.
  const int row_bytes = 17 * 8;
  char original[17 * 16];
  repaint_display();
  memcpy(original, screen, sizeof(original));
  clear_row(row);
  draw_string(0, row, expected, strlen(expected));
  flush_display();
  bool same = memcmp(original + row * row_bytes,
      screen + row * row_bytes, row_bytes) == 0;
  redisplay();
  check(same, name);
}

static void expect_menu_label(int key, const char *expected, const char *name) {
  // Compare an unselected TIME button with an independently labelled CUSTOM key.
  repaint_display();
  char actual[sizeof(screen)], saved_label[100];
  memcpy(actual, screen, sizeof(actual));
  int saved_length;
  get_custom_key(key, saved_label, &saved_length);
  bool saved_local_labels = flags.f.local_label;
  flags.f.local_label = 0;
  assign_custom_key(key, expected, strlen(expected));
  set_menu(MENULEVEL_PLAIN, MENU_CUSTOM1);
  redisplay();
  repaint_display();
  bool same = true;
  for (int y = 8; y < 16; y++)
    for (int x = (key - 1) * 22; x < key * 22 - 1; x++) {
      int offset = y * 17 + x / 8;
      if ((actual[offset] ^ screen[offset]) & (1 << (x % 8))) same = false;
    }
  assign_custom_key(key, saved_label, saved_length);
  flags.f.local_label = saved_local_labels;
  set_menu(MENULEVEL_PLAIN, MENU_NONE);
  redisplay();
  check(same, name);
}

static void test_open_command() {
  reset_time();
  type_number("12.5");
  bool queued = true;
  int repeat = -1;
  bool running = core_keydown_command("T.CALC", false, &queued, &repeat);
  check(!running && !queued && repeat == 0 && !mode_number_entry
      && !custom_time_active() && xvalue() == parsed_number("12.5"),
      "pressing T.CALC commits ordinary input before the menu opens on key release");
  core_keytimeout1();
  expect_row(0, "T.CALC", "holding T.CALC shows its command name instead of a pending NULL command");
  core_keyup();
  check(custom_time_active() && xvalue() == parsed_number("12.5"),
      "releasing T.CALC opens TIME without changing the confirmed hours");
  expect_row(0, "x\20012:30:00", "T.CALC displays the existing value as hours after key release");

  reset_time();
  type_number("12.5");
  core_keydown_command("T.CALC", false, &queued, &repeat);
  core_keytimeout2();
  core_keyup();
  check(!custom_time_active() && xvalue() == parsed_number("12.5"),
      "a long hold cancels T.CALC through the ordinary command lifecycle");

  reset_time();
  type_number("7");
  press(KEY_ENTER);
  open_time();
  type_number("2");
  check(xvalue() == 2 && ((vartype_real *) stack[sp - 2])->x == 0,
      "T.CALC preserves ENTER's disabled stack lift for the next number");

  reset_time();
  type_number("7");
  press(KEY_ENTER);
  type_number("2");
  press(KEY_ENTER);
  type_number("3");
  press(KEY_ADD);
  open_time();
  core_undo(&queued, &repeat);
  assert(!queued);
  core_keyup();
  check(xvalue() == 3 && ((vartype_real *) stack[sp - 1])->x == 2,
      "opening TIME preserves the preceding calculation's undo record");
  core_redo(&queued, &repeat);
  assert(!queued);
  core_keyup();
  check(xvalue() == 5 && ((vartype_real *) stack[sp - 1])->x == 7,
      "the preceding calculation can be redone after T.CALC");
  core_repeat_last_command(&queued, &repeat);
  assert(!queued);
  core_keyup();
  check(xvalue() == 12,
      "AGAIN repeats the preceding calculation rather than T.CALC");

  reset_time();
  press(KEY_SHIFT);
  press(KEY_RUN);
  std::vector<unsigned char> before = program();
  core_keydown_command("T.CALC", false, &queued, &repeat);
  assert(!queued);
  core_keyup();
  check(flags.f.prgm_mode && flags.f.message && program() == before
      && mode_appmenu == MENU_NONE,
      "T.CALC is restricted in program editing and does not insert an instruction");
}

static void test_input_and_stack() {
  reset_time();
  open_time();
  type_number("123.4567890123");
  const phloat expected_hours = 123 + phloat(45) / 60 + parsed_number("67.890123") / 3600;
  check(mode_number_entry && same_hours(xvalue(), expected_hours)
      && std::string(cmdline, cmdline_length) == "123.4567890123",
      "real time input stores hours while preserving its unnormalized input text");
  expect_entry("123:45:67.890123", "entry allows overflowing hours and seconds without rounding");
  expect_row(cmdline_row, "x\200123:45:67.890123_",
      "the normal input LCD actually renders time separators");
  press(KEY_SIGMA);
  check(mode_number_entry && same_hours(xvalue(), expected_hours),
      "selecting precision during input does not commit or replace its value");
  expect_row(cmdline_row, "x\200123:45:67.890123_",
      "minute precision does not hide unfinished seconds and fractional digits");
  press(KEY_INV);
  press(KEY_BSP);
  expect_entry("123:45:67.89012", "backspace removes a numeric digit rather than a generated separator");
  press(KEY_3);
  press(KEY_ENTER);
  check(!mode_number_entry && same_hours(xvalue(), expected_hours),
      "ENTER leaves the entered duration stored as hours");
  expect_hours(xvalue(), "5d 03:46:08", "default HMS display normalizes and rounds only after entry");
  expect_row(0, "x\2005d 03:46:08", "the normal X register LCD uses time formatting");
  press(KEY_SQRT);
  expect_row(0, "x\2005d 03:46:07.89", "the precision menu updates the actual stack LCD");
  char text[100];
  int length = reg2str(text, sizeof(text), AUX_REG_Y);
  check(length == (int) strlen("5d 03:46:07.89")
      && memcmp(text, "5d 03:46:07.89", strlen("5d 03:46:07.89")) == 0,
      "DM42 register formatting includes the duplicated Y value");

  reset_time();
  open_time();
  type_number("1.");
  expect_entry("1:", "an entered radix point starts minutes without adding zeroes");
  press(KEY_2);
  expect_entry("1:2", "partial minutes remain unpadded while entering digits");
  press(KEY_3);
  press(KEY_4);
  expect_entry("1:23:4", "the first seconds digit adds the seconds separator");
  press(KEY_BSP);
  expect_entry("1:23", "backspacing over a generated separator restores partial minutes");
  press(KEY_CHS);
  expect_entry("-1:23", "sign changes preserve the unfinished time input");

  reset_time();
  open_time();
  std::string maximum = "1234567890123456789012345678901234";
  maximum.resize(MAX_MANT_DIGITS);
  type_number(maximum.c_str());
  check(std::string(cmdline, cmdline_length) == maximum,
      "time entry accepts the existing full significant-digit limit for hours");
  expect_entry(maximum.c_str(), "many-digit hours remain unreduced during entry");
  press(KEY_9);
  check(std::string(cmdline, cmdline_length) == maximum,
      "time entry retains the existing significant-digit limit instead of extending numeric precision");
}

static void test_precision_and_boundaries() {
  reset_time();
  open_time();
  const phloat seconds = parsed_number("121.32") / 3600;
  type_number("8760");
  press(KEY_ENTER);
  check(xvalue() == 8760, "displaying a year keeps the stack value in hours");
  expect_row(0, "x\2001y   0d 00:00:00", "the actual X display carries 365 days into one year with padded zero days");
  expect_hours(23 + seconds, "23:02:01", "HMS precision omits fractional seconds");
  expect_value("12.3", "12:18:00", "stack values are decimal hours rather than packed minute digits");
  expect_value("1.25", "01:15:00", "a quarter hour is displayed as fifteen minutes");
  expect_hours(24 + seconds, "1d 00:02:01", "24 hours carries into one day");
  expect_value("48", "2d 00:00:00", "exact day multiples retain zero time fields");
  expect_value("0.5", "00:30:00", "half an hour retains the correct minute at numeric representation boundaries");
  expect_value("23.9999", "1d 00:00:00", "whole-second rounding carries across midnight");
  expect_hours(-24 - seconds, "-1d 00:02:01", "negative durations have one sign before the full display");
  expect_value("8759.5", "364d 23:30:00", "a duration below 365 days retains its day count");
  expect_value("8760", "1y   0d 00:00:00", "365 days are displayed as one fixed-length year with padded zero days");
  expect_value("8784.5", "1y   1d 00:30:00", "years retain the remaining days and time of day");
  expect_value("17520", "2y   0d 00:00:00", "exact year multiples keep the padded zero-day field");
  expect_value("-8784.5", "-1y   1d 00:30:00", "negative years and days share one leading sign");
  expect_value("8759.9999", "1y   0d 00:00:00", "whole-second rounding carries across the year boundary");
  expect_value("87599.9999", "10y   0d 00:00:00", "rounding carries through multiple year digits");
  expect_value("1e15", "114155251141y 201d 16:00:00",
      "large integer durations pass only the remaining clock hours to HMS conversion");
  expect_value("1e18", "114155251141552y 186d 16:00:00",
      "large year counts do not overflow native integer storage");
  press(KEY_SIGMA);
  expect_value("8760", "1y   0d 00:00", "minute precision keeps the padded zero-day field for an exact year");
  expect_value("8760.5", "1y   0d 00:30", "minute precision also displays the year count");
  expect_value("1.3999", "01:24", "minute precision rounds seconds into the next minute");
  expect_value("-0.00001", "00:00", "a negative duration rounded to zero has no negative sign");
  press(KEY_SQRT);
  expect_hours(8760 + seconds, "1y   0d 00:02:01.32", "fractional-second precision preserves the year count");
  expect_value("0.5", "00:30:00.00", "hundredth precision does not turn an exact minute into the previous second");
  expect_hours(23 + seconds, "23:02:01.32", "hundredth precision renders fractional seconds");
  expect_value("23.9999999", "1d 00:00:00.00", "hundredth rounding carries through seconds minutes and days");
  expect_value("-0.0000001", "00:00:00.00", "hundredth precision also suppresses a rounded negative zero");

  char text[100];
  vartype *complex = new_complex(1, 2);
  vartype *string = new_string("time", 4);
  vartype *matrix = new_realmatrix(1, 1);
  check(custom_time_format(complex, text, sizeof(text)) == -1
      && custom_time_format(string, text, sizeof(text)) == -1
      && custom_time_format(matrix, text, sizeof(text)) == -1,
      "complex values strings and matrices retain their ordinary display path");
  free_vartype(complex);
  free_vartype(string);
  free_vartype(matrix);
  vartype *huge = new_real(parsed_number("1e100"));
  check(custom_time_format(huge, text, sizeof(text)) == -1,
      "a duration too large to fit falls back to ordinary numeric formatting");
  free_vartype(huge);
  vartype *one = new_real(1);
  char tiny[4] = {'a', 'b', 'c', 'd'};
  check(custom_time_format(one, tiny + 1, 2) == -1
      && tiny[0] == 'a' && tiny[3] == 'd',
      "an insufficient display buffer rejects formatting without overwriting its bounds");
  free_vartype(one);
}

static void test_mode_lifecycle() {
  reset_time();
  flags.f.digits_bit3 = 0;
  flags.f.digits_bit2 = 0;
  flags.f.digits_bit1 = 1;
  flags.f.digits_bit0 = 0;
  flags.f.fix_or_all = 1;
  flags.f.eng_or_all = 0;
  open_time();
  press(KEY_SIGMA);
  expect_value("1.3999", "01:24", "time precision is selected by an actual menu key");
  press(KEY_SHIFT);
  press(KEY_E);
  check(custom_time_active() && get_front_menu() == MENU_DISP,
      "a plain display-menu overlay leaves the underlying time application active");
  expect_value("1.3999", "01:24", "time formatting remains active under a plain-menu overlay");
  press(KEY_EXIT);
  check(custom_time_active() && get_front_menu() == MENU_TIME,
      "closing the overlay returns to the time precision menu");
  press(KEY_EXIT);
  char text[100];
  vartype *value = new_real(parsed_number("1.3999"));
  check(!custom_time_active() && custom_time_format(value, text, sizeof(text)) == -1,
      "leaving the application restores the ordinary numeric display path");
  free_vartype(value);
  check(flags.f.digits_bit3 == 0 && flags.f.digits_bit2 == 0
      && flags.f.digits_bit1 == 1 && flags.f.digits_bit0 == 0
      && flags.f.fix_or_all && !flags.f.eng_or_all,
      "time mode preserves the original FIX precision and display mode");
  open_time();
  expect_value("1.3999", "01:24", "reentering time mode restores the selected minute precision");
  press(KEY_EXIT);
  press(KEY_SHIFT);
  press(KEY_CHS);
  press(KEY_UP);
  check(get_front_menu() == MENU_MODES4, "real keys reach the MODES page that previously contained time mode");
  press(KEY_SQRT);
  check(!custom_time_active() && mode_appmenu != MENU_TIME,
      "the former MODES position no longer opens time mode");
  press(KEY_EXIT);
  open_time();
  expect_value("1.3999", "01:24", "the HOME entrance restores the remembered precision");
  press(KEY_SHIFT);
  press(KEY_4);
  check(baseapp && !custom_time_active(), "entering BASE replaces the time application");
  open_time();
  check(!baseapp && custom_time_active(), "entering time mode exits BASE instead of combining their displays");
  expect_value("1.3999", "01:24", "BASE roundtrip retains the remembered time precision");
  flags.f.prgm_mode = 1;
  check(!custom_time_active(), "program editing does not use stack time formatting");
  flags.f.prgm_mode = 0;
}

static void test_rounding_selection() {
  reset_time();
  open_time();
  expect_menu_label(5, "TRNC", "F5 renders the TRNC label rather than the placeholder command");
  press(KEY_SIGMA);
  type_number("1.5950");
  press(KEY_ENTER);
  phloat original = xvalue();
  expect_row(0, "x\20002:00", "RND is the default and rounds to the nearest displayed minute");
  char rounded[sizeof(screen)];
  repaint_display();
  memcpy(rounded, screen, sizeof(rounded));
  press(KEY_LN);
  expect_menu_label(4, "RND", "F4 renders the RND label rather than the placeholder command");
  expect_row(0, "x\20001:59", "F5 selects TRNC and discards the undisplayed seconds");
  repaint_display();
  bool changed_round = false, changed_trunc = false;
  for (int y = 8; y < 16; y++)
    for (int x = 66; x < 109; x++) {
      int offset = y * 17 + x / 8;
      if ((rounded[offset] ^ screen[offset]) & (1 << (x % 8))) {
        if (x < 87) changed_round = true;
        else if (x >= 88) changed_trunc = true;
      }
    }
  check(changed_round && changed_trunc,
      "selecting TRNC moves the visible selection mark from RND to TRNC");
  check(xvalue() == original && !mode_number_entry,
      "changing display rounding leaves the calculated hours intact");
  press(KEY_EXIT);
  open_time();
  expect_row(0, "x\20001:59", "reentering TIME remembers truncation and minute precision");
  press(KEY_LOG);
  repaint_display();
  check(memcmp(rounded, screen, sizeof(rounded)) == 0 && xvalue() == original,
      "F4 restores RND and its selection mark without changing the duration");

  press(KEY_LN);
  expect_value("-1.999", "-01:59", "TRNC discards the magnitude's remainder for negative durations");
  expect_value("-0.00001", "00:00", "truncating a negative duration to zero suppresses its sign");
  type_number("23.59596789");
  std::string entry(cmdline, cmdline_length);
  original = xvalue();
  press(KEY_LOG);
  press(KEY_LN);
  check(mode_number_entry && std::string(cmdline, cmdline_length) == entry
      && xvalue() == original,
      "both rounding keys preserve unfinished HMS input and its stored hours");
  expect_entry("23:59:59.6789", "rounding selection does not truncate or round editable input");
  press(KEY_ENTER);
  expect_hours(xvalue(), "23:59", "minute truncation does not carry an unfinished day");
  press(KEY_INV);
  expect_hours(xvalue(), "23:59:59", "second truncation preserves the seconds before midnight");
  press(KEY_LOG);
  expect_hours(xvalue(), "1d 00:00:00", "second rounding carries into the next day");
  press(KEY_SQRT);
  expect_hours(xvalue(), "23:59:59.68", "hundredth rounding retains the nearest fractional second");
  press(KEY_LN);
  expect_hours(xvalue(), "23:59:59.67", "hundredth truncation discards the fractional remainder");
  expect_value("8759.9999", "364d 23:59:59.64", "truncation does not round a duration into the next year");
  expect_value("1e18", "114155251141552y 186d 16:00:00.00",
      "HMS formatting adds no fractional unit to huge integer hours");
  expect_value("1000000000000.25", "114155251y  51d 16:15:00.00",
      "TRNC preserves the fractional hour when huge counts cannot resolve hundredths");
  const phloat coarse_hours = parsed_number("8589934592") + phloat(1) / 524288;
  expect_hours(coarse_hours, "980586y  51d 08:00:00.00",
      "TRNC does not snap coarse hours from 0.006866 seconds to 0.01 seconds");
  press(KEY_LOG);
  expect_hours(coarse_hours, "980586y  51d 08:00:00.01",
      "RND still rounds coarse hours to the nearest hundredth");
  expect_value("1000000000000.25", "114155251y  51d 16:15:00.00",
      "RND also preserves exact minutes in huge fractional hour counts");
  press(KEY_LN);
  expect_value("0.9999999", "00:59:59.99",
      "TRNC preserves visible hundredths below an exact hour");
  press(KEY_INV);
  expect_value("0.9999999", "00:59:59",
      "TRNC preserves visible seconds below an exact hour");
  press(KEY_LOG);
  expect_value("0.9999999", "01:00:00",
      "RND carries a fractional second across an hour");

  reset_time();
  open_time();
  press(KEY_LN);
  for (const char *input : {"0.01", "1.01", "23.59"}) {
    press(KEY_SIGMA);
    type_number(input);
    press(KEY_ENTER);
    const char *expected = input[0] == '0' ? "00:01" : input[0] == '1' ? "01:01" : "23:59";
    expect_hours(xvalue(), expected, "TRNC preserves an exact minute entered through real keys");
  }
  press(KEY_INV);
  type_number("23.5959");
  press(KEY_ENTER);
  expect_hours(xvalue(), "23:59:59", "TRNC preserves exact entered seconds close to midnight");
  press(KEY_SQRT);
  type_number("123.456789");
  press(KEY_ENTER);
  expect_hours(xvalue(), "5d 03:46:07.89", "TRNC preserves exact entered hundredths after normalization");

  reset_time();
  open_time();
  type_number("23.59595");
  press(KEY_ENTER);
  expect_hours(xvalue(), "1d 00:00:00", "RND carries a half-second boundary in either numeric representation");
  press(KEY_SIGMA);
  type_number("1.0030");
  press(KEY_ENTER);
  expect_hours(xvalue(), "01:01", "RND carries an entered half-minute boundary");
  press(KEY_CHS);
  expect_hours(xvalue(), "-01:01", "RND treats negative half-minute boundaries symmetrically");
  press(KEY_SQRT);
  type_number("1.0000005");
  press(KEY_ENTER);
  expect_hours(xvalue(), "01:00:00.01", "RND carries an entered half-hundredth boundary");

  // Values measurably below a boundary stay below it after HMS conversion.
  press(KEY_SIGMA);
  type_number("1.00299999");
  press(KEY_ENTER);
  expect_hours(xvalue(), "01:00", "RND keeps values below a half-minute boundary below it");
  press(KEY_LN);
  type_number("1.00599999");
  press(KEY_ENTER);
  expect_hours(xvalue(), "01:00", "TRNC keeps values below a whole-minute boundary below it");
}

static void test_settings_command_lifecycle() {
  const int settings_keys[] = {KEY_SIGMA, KEY_INV, KEY_SQRT, KEY_LOG, KEY_LN};
  reset_time();
  open_time();
  type_number("5");
  press(KEY_ENTER);
  type_number("7");
  press(KEY_ENTER);
  for (int key : settings_keys) press(key);
  check(flags.f.stack_lift_disable && xvalue() == 7,
      "display-setting commands preserve ENTER's disabled stack lift");
  type_number("2");
  check(xvalue() == 2 && ((vartype_real *) stack[sp - 2])->x == 5,
      "numeric input after display settings replaces the ENTER duplicate without shifting Z");

  reset_time();
  open_time();
  type_number("7");
  press(KEY_ENTER);
  type_number("2");
  press(KEY_ENTER);
  type_number("3");
  press(KEY_ADD);
  for (int key : settings_keys) press(key);
  bool queued;
  int repeat;
  core_undo(&queued, &repeat);
  assert(!queued);
  core_keyup();
  check(xvalue() == 3 && ((vartype_real *) stack[sp - 1])->x == 2,
      "display-setting commands preserve the preceding addition's undo record");
  core_redo(&queued, &repeat);
  assert(!queued);
  core_keyup();
  check(xvalue() == 5 && ((vartype_real *) stack[sp - 1])->x == 7,
      "the addition can still be redone after display settings change");
  core_repeat_last_command(&queued, &repeat);
  assert(!queued);
  core_keyup();
  check(xvalue() == 12,
      "AGAIN repeats the addition rather than a display-setting command");

  reset_time();
  open_time();
  press(KEY_SHIFT);
  press(KEY_RUN);
  unsigned char original_settings = time_display_save();
  type_number("9.0001416");
  for (int key : settings_keys) press(key);
  check(flags.f.prgm_mode && !mode_number_entry
      && program_commands().size() == 7 && time_display_save() == original_settings,
      "display settings confirm program numeric input and insert five instructions without executing them");
  std::vector<unsigned char> confirmed_program = program();
  press(KEY_XEQ);
  check(flags.f.message && program() == confirmed_program,
      "the day control cannot insert a new command ID into a saved program");

  press(KEY_EXIT);
  press(KEY_SHIFT);
  press(KEY_RUN);
  std::vector<int> saved_commands = program_commands();
  char raw_path[] = "/tmp/free42-time-program-XXXXXX";
  int descriptor = mkstemp(raw_path);
  assert(descriptor >= 0);
  close(descriptor);
  int index = current_prgm;
  core_export_programs(1, &index, raw_path);
  char state_path[] = "/tmp/free42-time-program-state-XXXXXX";
  descriptor = mkstemp(state_path);
  assert(descriptor >= 0);
  close(descriptor);
  core_save_state(state_path);
  core_cleanup();
  core_init(1, 0, state_path, 0);
  unlink(state_path);
  check(program_commands() == saved_commands,
      "state saving and loading preserve all TIME program instructions");

  reset_time();
  core_import_programs(0, raw_path);
  unlink(raw_path);
  check(program_commands() == saved_commands,
      "RAW export and import preserve all TIME program instructions");
  flags.f.message = flags.f.two_line_message = 0;
  open_time();
  pc = 0;
  auto step = []() {
    bool queued;
    int repeat;
    core_keydown_command("SST", false, &queued, &repeat);
    assert(!queued);
    core_keyup();
  };
  step();
  phloat hours = xvalue();
  check(hours == parsed_number("9.0001416"),
      "the imported program enters decimal hours without interpreting them as packed HMS");
  const char *displays[] = {"09:00", "09:00:01", "09:00:00.51", "09:00:00.51", "09:00:00.50"};
  for (int i = 0; i < 5; i++) {
    // Select truncation first so the saved rounding instruction must change it.
    if (i == 3) press(KEY_LN);
    step();
    expect_hours(hours, displays[i], "executing a saved TIME instruction changes the display format");
    check(xvalue() == hours, "executing a TIME instruction preserves the stack value");
  }
}

static void test_base_exclusion() {
  const int radix_commands[] = {CMD_BINM, CMD_OCTM, CMD_DECM, CMD_HEXM};
  const int radices[] = {2, 8, 10, 16};
  for (int i = 0; i < 4; i++) {
    int command = radix_commands[i];
    reset_time();
    open_time();
    type_number("1.30");
    press(KEY_ENTER);
    press(KEY_SHIFT);
    press(KEY_4);
    bool queued;
    int repeat;
    core_keydown_command_id(command, &queued, &repeat);
    assert(!queued);
    core_keyup();
    check(baseapp && !custom_time_active(),
        "a radix selection in BASE leaves time formatting disabled");
    open_time();
    check(!baseapp && get_base() == 10 && xvalue() == parsed_number("1.5"),
        "opening TIME leaves BASE and restores decimal input without changing hours");
    press(KEY_EXIT);
    check(!baseapp && get_base() == 10 && mode_appmenu == MENU_NONE,
        "EXIT from TIME does not restore the previous BASE application or radix");
    type_number("12.5");
    press(KEY_ENTER);
    check(xvalue() == parsed_number("12.5"),
        "ordinary input after leaving TIME remains decimal");

    // Standalone radix commands can change input without opening a BASE menu.
    core_keydown_command_id(command, &queued, &repeat);
    assert(!queued);
    core_keyup();
    check(!baseapp, "an ordinary standalone radix command retains its existing menu behavior");
    open_time();
    type_number("12.30");
    press(KEY_ENTER);
    check(get_base() == 10 && !baseapp && xvalue() == parsed_number("12.5"),
        "TIME entered after a standalone radix command still accepts twelve thirty as 12.5 hours");
    core_keydown_command_id(command, &queued, &repeat);
    assert(!queued);
    core_keyup();
    check(!baseapp && !custom_time_active() && mode_appmenu == MENU_NONE
        && get_base() == radices[i] && xvalue() == parsed_number("12.5"),
        "a radix command during TIME closes its menu and changes radix without changing the stack");
    press(KEY_EXIT);
    check(!baseapp && !custom_time_active() && get_base() == radices[i]
        && mode_appmenu == MENU_NONE && xvalue() == parsed_number("12.5"),
        "EXIT after standalone radix selection retains the radix without reopening TIME");
  }

  reset_time();
  open_time();
  press(KEY_SHIFT);
  press(KEY_ADD);
  press(KEY_SIGMA);
  for (int row = 0; row < 14; row++) press(KEY_DOWN);
  press(KEY_XEQ);
  check(!custom_time_active() && !baseapp && mode_appmenu == MENU_NONE && get_base() == 16,
      "HEXM selected from the real command catalog closes TIME without opening BASE");
  open_time();
  type_number("12.30");
  press(KEY_ENTER);
  check(xvalue() == parsed_number("12.5"),
      "returning to TIME after catalog HEXM accepts the decimal HMS input");

  // Restore metadata written before TIME and nondecimal input were made exclusive.
  unsigned char metadata = time_display_save();
  press(KEY_EXIT);
  bool queued;
  int repeat;
  core_keydown_command_id(CMD_HEXM, &queued, &repeat);
  assert(!queued);
  core_keyup();
  time_display_restore(metadata);
  type_number("12.30");
  press(KEY_ENTER);
  check(custom_time_active() && !baseapp && get_base() == 10
      && xvalue() == parsed_number("12.5"),
      "restoring active TIME metadata also leaves any standalone nondecimal radix behind");
}

static void test_shared_hms_display() {
  reset_time();
  type_number("12.5");
  bool queued;
  int repeat;
  core_keydown_command_id(CMD_TO_HMS, &queued, &repeat);
  assert(!queued);
  core_keyup();
  check(xvalue() == parsed_number("12.3") && ((vartype_real *) lastx)->x == parsed_number("12.5"),
      "the existing HMS command still converts decimal hours and preserves its original LAST X behavior");
  press(KEY_CHS);
  core_keydown_command_id(CMD_TO_HR, &queued, &repeat);
  assert(!queued);
  core_keyup();
  check(xvalue() == parsed_number("-12.5"),
      "the existing hour conversion still reverses a negative HMS command result");
  core_keydown_command_id(CMD_TO_HMS, &queued, &repeat);
  assert(!queued);
  core_keyup();
  check(xvalue() == parsed_number("-12.3"),
      "the shared HMS command retains negative-duration conversion");

  reset_time();
  open_time();
  type_number("0.0216");
  press(KEY_ENTER);
  phloat original = xvalue();
  press(KEY_LN);
  expect_row(0, "x\20000:02:16", "shared HMS conversion preserves entered seconds under TRNC");
  press(KEY_SQRT);
  expect_row(0, "x\20000:02:16.00", "the same HMS text also preserves exact seconds at hundredth precision");
  check(xvalue() == original,
      "displaying the shared HMS result leaves the original hours in the stack");

  type_number("0.02025");
  press(KEY_ENTER);
  press(KEY_INV);
  press(KEY_LOG);
  expect_row(0, "x\20000:02:03", "shared HMS conversion rounds entered half seconds at second precision");
  press(KEY_SQRT);
  press(KEY_LN);
  expect_row(0, "x\20000:02:02.50", "TRNC retains half seconds after the shared HMS conversion");
  press(KEY_CHS);
  expect_row(0, "x\200-00:02:02.50", "the same display conversion preserves the sign of a negative duration");

  reset_time();
  open_time();
  type_number("2");
  press(KEY_ENTER);
  type_number("1.5930");
  press(KEY_SUB);
  original = xvalue();
  check(same_hours(original, phloat(1) / 120),
      "ordinary subtraction leaves the half-minute result stored in hours");
  press(KEY_SIGMA);
  expect_row(0, "x\20000:01", "normal HMS display precision restores this half-minute boundary after subtraction");
  check(xvalue() == original,
      "rounding the HMS display of a subtraction does not round the calculation result");
}

static void test_locale_exponent_and_arithmetic() {
  reset_time();
  flags.f.decimal_point = 0;
  open_time();
  press(KEY_SQRT);
  type_number("23,020132");
  check(std::string(cmdline, cmdline_length) == "23,020132"
      && same_hours(xvalue(), 23 + parsed_number("121.32") / 3600),
      "comma locale stores hours while retaining the existing numeric input convention");
  expect_entry("23:02:01.32", "time input uses a period for fractional seconds in comma locale");
  press(KEY_E);
  char text[100];
  check(custom_time_entry(cmdline, cmdline_length, text, sizeof(text)) == -1,
      "an incomplete exponent uses the ordinary numeric input display");
  press(KEY_1);
  check(custom_time_entry(cmdline, cmdline_length, text, sizeof(text)) == -1,
      "a completed exponent also keeps the ordinary numeric input display until ENTER");
  press(KEY_ENTER);
  check(xvalue() == parsed_number("230.20132"), "exponent entry keeps its ordinary arithmetic meaning");

  reset_time();
  open_time();
  type_number("1.30");
  press(KEY_ENTER);
  check(xvalue() == parsed_number("1.5"), "one hour thirty minutes is stored as 1.5 hours");
  type_number("0.45");
  check(xvalue() == parsed_number("0.75"), "forty-five minutes is stored as 0.75 hours before a command");
  press(KEY_ADD);
  check(xvalue() == parsed_number("2.25"), "ordinary addition adds durations in hours");
  expect_row(0, "x\20002:15:00", "ordinary addition displays one hour thirty plus forty-five as two hours fifteen");
  type_number("0.30");
  press(KEY_SUB);
  check(xvalue() == parsed_number("1.75"), "ordinary subtraction subtracts half an hour without HMS commands");
  expect_row(0, "x\20001:45:00", "ordinary subtraction displays two hours fifteen minus thirty minutes");

  reset_time();
  open_time();
  type_number("0.30");
  press(KEY_ENTER);
  type_number("0.45");
  press(KEY_SUB);
  check(xvalue() == parsed_number("-0.25"), "ordinary subtraction produces negative durations as negative hours");
  expect_row(0, "x\200-00:15:00", "a negative arithmetic result is formatted as a negative duration");

  reset_time();
  open_time();
  type_number("23.45");
  press(KEY_ENTER);
  type_number("0.30");
  press(KEY_ADD);
  check(xvalue() == parsed_number("24.25"), "ordinary addition carries over a day while keeping decimal hours");
  expect_row(0, "x\2001d 00:15:00", "ordinary arithmetic crossing midnight is rendered with a day count");

  reset_time();
  open_time();
  type_number("12.30");
  expect_entry("12:30", "twelve thirty is displayed with a time separator while being entered");
  check(mode_number_entry && xvalue() == parsed_number("12.5"),
      "twelve thirty is already stored as twelve and a half hours during input");
  press(KEY_ENTER);
  check(xvalue() == parsed_number("12.5"), "confirming twelve thirty preserves its value in hours");
  press(KEY_EXIT);
  check(!custom_time_active() && xvalue() == parsed_number("12.5"),
      "EXIT restores the ordinary display with 12.5 instead of the packed input 12.3");
  open_time();
  expect_row(0, "x\20012:30:00", "reentering time mode interprets the existing 12.5 hours as twelve thirty");

  reset_time();
  open_time();
  type_number("12.30");
  press(KEY_EXIT);
  check(!mode_number_entry && !custom_time_active() && xvalue() == parsed_number("12.5"),
      "EXIT directly from unfinished time input confirms twelve thirty as 12.5 hours");

  reset_time();
  open_time();
  std::vector<unsigned char> before_program = program();
  type_number("1.30");
  press(KEY_SHIFT);
  press(KEY_RUN);
  check(flags.f.prgm_mode && !mode_number_entry && !custom_time_active()
      && xvalue() == parsed_number("1.5") && program() == before_program,
      "switching to program editing confirms time input as hours without inserting an instruction");
  press(KEY_SHIFT);
  press(KEY_RUN);
  check(!flags.f.prgm_mode && custom_time_active() && xvalue() == parsed_number("1.5"),
      "returning from program editing preserves the duration in hours");

  reset_time();
  type_number("12.5");
  phloat ordinary = xvalue();
  open_time();
  check(!mode_number_entry && xvalue() == ordinary && ordinary == parsed_number("12.5"),
      "entering time mode confirms an unfinished ordinary number without reinterpreting its digits");
  expect_row(0, "x\20012:30:00", "a value entered in normal mode is rendered as decimal hours");
}

static void test_day_constant() {
  reset_time();
  open_time();
  expect_menu_label(6, "1d", "F6 actually renders the 1d label rather than an empty command name");
  type_number("1.30");
  press(KEY_ENTER);
  press(KEY_XEQ);
  check(!mode_number_entry && custom_time_active() && xvalue() == 24
      && ((vartype_real *) stack[sp - 1])->x == parsed_number("1.5"),
      "the day key after confirmed input adds 24 hours below the previous X");
  expect_row(0, "x\2001d 00:00:00", "the day constant is displayed as one complete day");
  press(KEY_ADD);
  check(xvalue() == parsed_number("25.5"),
      "ordinary addition combines the day constant with the previous duration in hours");
  expect_row(0, "x\2001d 01:30:00", "adding one day preserves the previous time of day");
  press(KEY_XEQ);
  press(KEY_SUB);
  check(xvalue() == parsed_number("1.5"),
      "ordinary subtraction removes the day constant without converting the duration");

  press(KEY_XEQ);
  press(KEY_XEQ);
  check(xvalue() == 24 && ((vartype_real *) stack[sp - 1])->x == 24
      && ((vartype_real *) stack[sp - 2])->x == parsed_number("1.5"),
      "successive day presses each place another 24 hours on the stack");
  press(KEY_EXIT);
  check(!custom_time_active() && xvalue() == 24,
      "leaving time mode shows the day constant as the ordinary number 24");

  reset_time();
  bool allowed_big_stack = core_settings.allow_big_stack;
  core_settings.allow_big_stack = true;
  bool queued;
  int repeat;
  core_keydown_command_id(CMD_NSTK, &queued, &repeat);
  assert(!queued);
  core_keyup();
  core_keydown_command_id(CMD_CLST, &queued, &repeat);
  assert(!queued);
  core_keyup();
  check(flags.f.big_stack && sp == -1, "the day-key boundary test starts with an empty unlimited stack");
  open_time();
  press(KEY_XEQ);
  check(sp == 0 && xvalue() == 24,
      "the day key can place 24 hours on an empty unlimited stack");
  press(KEY_XEQ);
  check(sp == 1 && xvalue() == 24 && ((vartype_real *) stack[0])->x == 24,
      "the day key adds a separate value on the unlimited stack");
  type_number("2");
  press(KEY_XEQ);
  check(sp == 2 && xvalue() == 48 && ((vartype_real *) stack[1])->x == 24,
      "the day suffix replaces unfinished input without adding another unlimited-stack entry");
  press(KEY_XEQ);
  check(sp == 3 && xvalue() == 24 && ((vartype_real *) stack[2])->x == 48,
      "the next day press after the suffix adds an independent 24 hours");
  core_settings.allow_big_stack = allowed_big_stack;
}

static void test_day_input_suffix() {
  reset_time();
  open_time();
  type_number("7");
  press(KEY_ENTER);
  type_number("2");
  int depth = sp;
  phloat saved_lastx = ((vartype_real *) lastx)->x;
  bool queued;
  int repeat;
  bool running = core_keydown(KEY_XEQ, &queued, &repeat);
  check(!running && !queued && repeat == 0 && !mode_number_entry
      && xvalue() == 48 && sp == depth && ((vartype_real *) stack[sp - 1])->x == 7
      && ((vartype_real *) lastx)->x == saved_lastx,
      "pressing 1d during input confirms two days without lifting the stack or changing LAST X");
  expect_row(0, "x\2002d 00:00:00", "the input suffix immediately shows the confirmed day count");
  char pressed[sizeof(screen)];
  repaint_display();
  memcpy(pressed, screen, sizeof(pressed));
  core_keytimeout1();
  core_keytimeout2();
  core_keyup();
  repaint_display();
  check(xvalue() == 48 && memcmp(pressed, screen, sizeof(pressed)) == 0,
      "holding and releasing the day suffix neither rescales the value nor paints NULL");
  press(KEY_XEQ);
  check(xvalue() == 24 && ((vartype_real *) stack[sp - 1])->x == 48,
      "pressing 1d again after confirmation enters 24 hours instead of multiplying again");
  type_number("3");
  press(KEY_XEQ);
  check(xvalue() == 72 && ((vartype_real *) stack[sp - 1])->x == 24,
      "new numeric input lifts the prior value and the suffix replaces only the new X");

  reset_time();
  open_time();
  type_number("1.30");
  press(KEY_XEQ);
  check(xvalue() == 36 && !mode_number_entry,
      "the day suffix multiplies the entered 1.5 hours rather than the packed HMS digits");
  expect_row(0, "x\2001d 12:00:00", "the suffix normalizes the scaled fractional duration");
  type_number("2");
  press(KEY_CHS);
  press(KEY_XEQ);
  check(xvalue() == -48 && !mode_number_entry,
      "the day suffix preserves the sign of unfinished input");
  type_number("0");
  press(KEY_XEQ);
  check(xvalue() == 0 && !mode_number_entry,
      "unfinished zero is confirmed as zero rather than replaced with the day constant");

  reset_time();
  open_time();
  type_number("2");
  press(KEY_ENTER);
  type_number("3");
  press(KEY_ADD);
  type_number("2");
  press(KEY_XEQ);
  core_repeat_last_command(&queued, &repeat);
  assert(!queued);
  core_keyup();
  check(xvalue() == 53,
      "the input suffix preserves AGAIN's prior addition instead of becoming a calculation command");
  core_undo(&queued, &repeat);
  core_keyup();
  check(xvalue() == 48 && ((vartype_real *) stack[sp - 1])->x == 5,
      "undoing the subsequent addition restores the confirmed day input");

  reset_time();
  open_time();
  type_number("1");
  press(KEY_E);
#ifdef BCD_MATH
  type_number("6144");
#else
  type_number("308");
#endif
  phloat original = xvalue();
  flags.f.range_error_ignore = 0;
  press(KEY_XEQ);
  check(mode_number_entry && xvalue() == original && flags.f.message,
      "day-suffix overflow reports an error and preserves the unfinished number");
  flags.f.range_error_ignore = 1;
  press(KEY_XEQ);
  check(!mode_number_entry && xvalue() == POS_HUGE_PHLOAT,
      "ignored day-suffix overflow clamps to the normal maximum finite value");
}

static void test_saved_precision() {
  reset_time();
  open_time();
  press(KEY_SQRT);
  unsigned char fractional = time_display_save();
  press(KEY_SIGMA);
  time_display_restore(fractional);
  const phloat sample_hours = 1 + phloat(23) / 60 + parsed_number("45.678") / 3600;
  expect_hours(sample_hours, "01:23:45.68",
      "the DM42 metadata restores a precision selected with real menu keys");
  time_display_restore(fractional | 0xf0);
  expect_hours(sample_hours, "01:23:45.68",
      "metadata bits above the stored 4 bits do not affect time display");
  press(KEY_LN);
  unsigned char truncated = time_display_save();
  press(KEY_LOG);
  time_display_restore(truncated);
  expect_hours(sample_hours, "01:23:45.67",
      "the DM42 metadata remembers truncation selected by F5");
  time_display_restore(fractional);
  expect_hours(sample_hours, "01:23:45.68",
      "metadata saved with RND restores rounding after TRNC was selected");

  time_display_restore(0);
  check(!custom_time_active() && mode_appmenu == MENU_NONE,
      "an old zero metadata field leaves time mode inactive");
  open_time();
  expect_hours(sample_hours, "01:23:46",
      "an old zero metadata field restores the default whole-second precision");

  // Restoring time metadata must not replace another calculator application.
  press(KEY_SHIFT);
  press(KEY_DIV);
  int other_application = mode_appmenu;
  check(other_application != MENU_NONE && other_application != MENU_TIME,
      "the real STAT key opens another application before restoring time metadata");
  time_display_restore(fractional);
  check(mode_appmenu == other_application && !custom_time_active(),
      "an active time flag cannot replace an independently restored application");
  press(KEY_EXIT);
  open_time();
  expect_hours(sample_hours, "01:23:45.68",
      "time precision is restored even while another application is active");

  // Persist the ordinary core state separately from the DM42 display metadata.
  type_number("123.4567890123");
  press(KEY_ENTER);
  phloat original = xvalue();
  check(same_hours(original, 123 + phloat(45) / 60 + parsed_number("67.890123") / 3600),
      "the saved duration is represented as hours rather than packed HMS digits");
  std::vector<unsigned char> original_program = program();
  press(KEY_LN);
  press(KEY_SHIFT);
  press(KEY_E);
  unsigned char active_overlay = time_display_save();
  check(custom_time_active() && get_front_menu() == MENU_DISP,
      "time mode remains active beneath the overlay being saved");
  char state_path[] = "/tmp/free42-time-state-XXXXXX";
  int descriptor = mkstemp(state_path);
  assert(descriptor >= 0);
  close(descriptor);
  core_save_state(state_path);
  check(custom_time_active() && get_front_menu() == MENU_DISP && xvalue() == original,
      "writing an ordinary core state does not change the live time application or value");

  FILE *state = fopen(state_path, "rb");
  assert(state && fseek(state, 0, SEEK_END) == 0);
  long size = ftell(state);
  assert(size > 8 && fseek(state, 0, SEEK_SET) == 0);
  std::vector<unsigned char> bytes(size);
  assert(fread(bytes.data(), 1, bytes.size(), state) == bytes.size());
  fclose(state);
  int4 footer_magic, footer_version, header_version;
  memcpy(&footer_magic, bytes.data() + bytes.size() - 8, 4);
  memcpy(&footer_version, bytes.data() + bytes.size() - 4, 4);
  memcpy(&header_version, bytes.data() + 4, 4);
  check(footer_magic == FREE42_MAGIC && footer_version == header_version,
      "the normal core state still ends at the traditional magic and version footer");

  char ordinary_path[] = "/tmp/free42-ordinary-state-XXXXXX";
  descriptor = mkstemp(ordinary_path);
  assert(descriptor >= 0);
  close(descriptor);
  set_menu(MENULEVEL_APP, MENU_NONE);
  core_save_state(ordinary_path);
  state = fopen(ordinary_path, "rb");
  assert(state && fseek(state, 0, SEEK_END) == 0);
  check(ftell(state) == size,
      "saving time mode adds no bytes compared with the same ordinary core state");
  fclose(state);

  // A GTK restart uses no DM42 metadata and resets activation, precision and rounding.
  open_time();
  press(KEY_SIGMA);

  core_cleanup();
  core_init(1, 0, state_path, 0);
  check(mode_appmenu == MENU_NONE && !custom_time_active()
      && xvalue() == original && program() == original_program,
      "the actual core writer and loader preserve calculation data without saving the new application ID");
  check(get_front_menu() == MENU_DISP,
      "the core state preserves an ordinary menu overlay independently of time metadata");
  press(KEY_EXIT);
  open_time();
  expect_hours(xvalue(), "5d 03:46:08",
      "a GTK restart restores default precision and rounding instead of retaining process memory");

  // DM42 restores its separately saved header after loading the unchanged core state.
  core_cleanup();
  core_init(1, 0, state_path, 0);
  time_display_restore(active_overlay);
  check(custom_time_active() && get_front_menu() == MENU_DISP
      && xvalue() == original && program() == original_program,
      "separate DM42 metadata restores time mode while preserving the core overlay and calculation data");
  expect_hours(xvalue(), "5d 03:46:07.89",
      "the DM42 restart restores fractional-second precision from the separate metadata");

  press(KEY_EXIT);
  press(KEY_INV);
  expect_hours(xvalue(), "5d 03:46:07",
      "the DM42 restart also restores truncation instead of rounding seconds upward");
  press(KEY_SIGMA);
  press(KEY_EXIT);
  unsigned char inactive_minutes = time_display_save();
  check(!custom_time_active(), "time mode is inactive when its remembered precision is saved");
  core_save_state(state_path);
  open_time();
  press(KEY_SQRT);
  core_cleanup();
  core_init(1, 0, state_path, 0);
  time_display_restore(inactive_minutes);
  check(!custom_time_active() && mode_appmenu == MENU_NONE && xvalue() == original,
      "inactive DM42 metadata preserves the selected precision without activating time mode");
  open_time();
  expect_hours(xvalue(), "5d 03:46",
      "reentering after a DM42 restart uses the precision saved while the mode was inactive");
  press(KEY_INV);
  expect_hours(xvalue(), "5d 03:46:07",
      "inactive DM42 metadata also remembers truncation when the time mode is reopened");
  time_display_restore(0);
  check(!custom_time_active() && xvalue() == original,
      "a legacy DM42 header with zero metadata leaves calculation data intact");
  open_time();
  expect_hours(xvalue(), "5d 03:46:08",
      "a legacy DM42 header recovers the existing default whole-second display");
  assert(remove(state_path) == 0);
  assert(remove(ordinary_path) == 0);
  gfile = NULL;
}

int main() {
  core_init(0, 0, NULL, 0);
  test_open_command();
  test_input_and_stack();
  test_precision_and_boundaries();
  test_mode_lifecycle();
  test_rounding_selection();
  test_settings_command_lifecycle();
  test_base_exclusion();
  test_shared_hms_display();
  test_locale_exponent_and_arithmetic();
  test_day_constant();
  test_day_input_suffix();
  test_saved_precision();
  core_cleanup();
  printf("%d time display checks passed\n", checks);
}
