#include "core_home_menu_custom.h"

#include "core_globals.h"
#include "core_history_custom.h"
#include "core_keydown.h"
#include "core_program_tools_custom.h"
#include "core_tables.h"

namespace {

struct home_slot {
    core_home_binding unshifted;
    core_home_binding shifted;
};

template<size_t Size>
constexpr core_home_binding bind(const char (&label)[Size], bool (*action)(bool *, int *)) {
    return { label, Size - 1, action };
}

template<int Command>
bool run_command(bool *enqueued, int *repeat) {
    return core_keydown_command_id(Command, enqueued, repeat);
}

constexpr home_slot normal_home[CORE_HOME_KEY_COUNT] = {
    { bind("%", run_command<CMD_PERCENT>), bind("%CH", run_command<CMD_PERCENT_CH>) },
    { bind("nCr", run_command<CMD_COMB>), bind("nPr", run_command<CMD_PERM>) },
    {},
    {},
    { bind("AGAIN", core_repeat_last_command), {} },
    { bind("UNDO", core_undo), bind("REDO", core_redo) }
};

constexpr home_slot program_home[CORE_HOME_KEY_COUNT] = {
    { bind("LBLs", core_program_label_jump), {} },
    { bind("JUMP", core_program_follow), {} },
    { bind("FIND\016", core_program_find_downward), bind("FIND^", core_program_find_upward) },
    {},
    { bind("AGAIN", core_repeat_last_command), {} },
    { bind("UNDO", core_undo), bind("REDO", core_redo) }
};

} // namespace

core_home_binding core_home_menu_binding(unsigned int slot, bool shifted) {
    if (slot >= CORE_HOME_KEY_COUNT)
        return {};

    const home_slot &binding = flags.f.prgm_mode ? program_home[slot] : normal_home[slot];
    if (shifted && binding.shifted.action != nullptr)
        return binding.shifted;
    return binding.unshifted;
}
