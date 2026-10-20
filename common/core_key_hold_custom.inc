/* Custom key-hold behavior for the local Free42 fork.
 * Included by core_main.cc to keep key-down and timer helpers private.
 */

// Hold duration from key-down in milliseconds, between the 250 ms display and 2000 ms cancellation.
#define CUSTOM_KEY_HOLD_MS 800

struct custom_key_hold_binding {
    int key;
    bool shift;
    int command;
};

static const custom_key_hold_binding custom_key_hold_bindings[] = {
    // { key_to_press, is_shifted, command_to_execute }
    { KEY_BSP, false, CMD_CLST },
};

static int pending_key_hold_command = CMD_NONE;

static const custom_key_hold_binding *custom_key_hold_prepare(int shift, int key) {
    // Clear the previous candidate even when this key is excluded from hold detection.
    pending_key_hold_command = CMD_NONE;
    // Editing and menu soft keys keep their existing behavior, including auto-repeat.
    if (mode_number_entry || mode_command_entry || alpha_active()
            || flags.f.prgm_mode || mode_running || mode_getkey || mode_clall
            || matedit_mode >= 2
            || (key >= KEY_SIGMA && key <= KEY_XEQ && get_front_menu() != MENU_NONE))
        return NULL;

    for (unsigned int i = 0;
            i < sizeof(custom_key_hold_bindings) / sizeof(custom_key_hold_bindings[0]); i++) {
        const custom_key_hold_binding *binding = &custom_key_hold_bindings[i];
        if (binding->key == key && binding->shift == (shift != 0)
                && cmd_array[binding->command].argtype == ARG_NONE
                && (cmd_array[binding->command].flags & FLAG_PRGM_ONLY) == 0)
            return binding;
    }
    return NULL;
}

static void custom_key_hold_after_keydown(const custom_key_hold_binding *binding) {
    if (repeating != 0 || pending_command < 0
            || (cmd_array[pending_command].flags & FLAG_NO_SHOW) != 0)
        return;

    // Only the target command must survive until the later timer notification.
    pending_key_hold_command = binding->command;
}

static void custom_key_hold_start_timer() {
    // The shell calls timeout1 at 250 ms, so wait only the remaining hold duration.
    if (pending_key_hold_command >= 0 && pending_command >= 0)
        shell_request_timeout3(CUSTOM_KEY_HOLD_MS - 250);
}

static bool custom_key_hold_timeout(bool repaint) {
    // Negative values mark no pending command, cancellation, or a lingering display.
    if (pending_key_hold_command < 0 || pending_command < 0)
        return false;

    int command = pending_key_hold_command;
    pending_key_hold_command = CMD_NONE;
    if (!repaint)
        return true;

    // Use the normal key-up execution path, including command tracing and errors.
    pending_command = command;
    pending_command_arg.type = ARGTYPE_NONE;
    display_command(0);
    display_x(1);
    flush_display();
    return true;
}
