/* Custom menus for the local Free42 fork.
 * Included at the end of core_globals.cc's menus array. Keep the entries in
 * menu-ID order and append new menus so existing menu IDs remain unchanged.
 */
    { /* MENU_CONVERT_CUSTOM */ MENU_NONE, MENU_CONVERT2, MENU_CONVERT1,
                      { { 0x1000 + CMD_TO_INCH, 0, "" },
                        { 0x1000 + CMD_TO_METER, 0, "" },
                        { 0x1000 + CMD_NULL,    0, "" },
                        { 0x1000 + CMD_NULL,    0, "" },
                        { 0x1000 + CMD_NULL,    0, "" },
                        { 0x1000 + CMD_NULL,    0, "" } } }
