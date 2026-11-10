#include "core_home_menu_custom.h"

#include "core_globals.h"

namespace {

struct home_slot {
    core_home_binding unshifted;
    core_home_binding shifted;
};

template<size_t Size>
constexpr core_home_binding bind(const char (&label)[Size], bool (*action)(bool *, int *)) {
    return { label, Size - 1, action };
}

constexpr home_slot normal_home[CORE_HOME_KEY_COUNT] = {
    {},
    {},
    {},
    {},
    {},
    {},
};

constexpr home_slot program_home[CORE_HOME_KEY_COUNT] = {
    {},
    {},
    {},
    {},
    {},
    {},
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
