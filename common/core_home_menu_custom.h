#ifndef CORE_HOME_MENU_CUSTOM_H
#define CORE_HOME_MENU_CUSTOM_H

#include <stddef.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

enum { CORE_HOME_KEY_COUNT = 6 };

typedef struct {
    // Empty slots have a null label and action.
    // Labels use Free42 character codes and may contain zero bytes.
    const char *label;
    // Number of label bytes to read, excluding any terminator.
    size_t label_length;
    bool (*action)(bool *enqueued, int *repeat);
} core_home_binding;

#ifdef __cplusplus
extern "C" {
#endif

// Look up a zero-based HOME slot without executing it.
core_home_binding core_home_menu_binding(unsigned int slot, bool shifted);

#ifdef __cplusplus
}
#endif

#endif
