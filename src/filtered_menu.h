#ifndef FILTERED_MENU_H_DEFINED
#define FILTERED_MENU_H_DEFINED

#include "defines.h"
#include "string_array.h"
#include "tle_db.h"

/**
 * Entry in filter-enabled menu.
 **/
struct filtered_menu_entry {
	///displayed name in menu
	char *displayed_name;
	///whether entry is enabled (selected/deselected in menu)
	bool enabled;
};

/**
 * Menu that can be filtered to display only specific entries.
 **/
struct filtered_menu {
	///number of entries in menu
	int num_entries;
	///entries in menu
	struct filtered_menu_entry *entries;
	///number of entries that are currently displayed in menu
	int num_displayed_entries;
	///currently displayed items in menu
	ITEM** displayed_entries;
	///menu
	MENU* menu;
	///mapping between displayed item indices and the actual entries in the menu
	int *entry_mapping;
	///mapping between actual indices and displayed items. Has -1 if item is not displayed
	int *inverse_entry_mapping;
	///name of currently/last marked item (used for keeping cursor position)
	char curr_item[MAX_NUM_CHARS];
	///Subwindow used for MENU
	WINDOW *sub_window;
};

/**
 * Create filtered menu from string array.
 *
 * \param list Returned menu struct
 * \param names List of names to use for the menu items
 * \param my_menu_win Ncurses window to display the menu in
 **/
void filtered_menu_from_stringarray(struct filtered_menu *list, string_array_t *names, WINDOW *my_menu_win);

/**
 * Create filtered menu from names in TLE database.
 *
 * \param list Returned menu struct
 * \param db TLE database
 * \param my_menu_win Ncurses window to display the menu inside
 **/
void filtered_menu_from_tle_db(struct filtered_menu *list, const struct tle_db *db, WINDOW *my_menu_win);

/**
 * Get true underlying index of currently selected item in menu.
 *
 * \param list Menu
 * \return Mapped index
 **/
int filtered_menu_index(struct filtered_menu *list);

/**
 * Select input index so that corresponding entry in displayed menu is selected.
 *
 * \param list Menu
 * \param index Index to select in original array
 **/
void filtered_menu_select_index(struct filtered_menu *list, int index);

/**
 * Free memory allocated in menu struct.
 *
 * \param list Menu struct to free
 **/
void filtered_menu_free(struct filtered_menu *list);

/**
 * Filter displayed menu entries based on input pattern.
 *
 * \param list Menu struct
 * \param pattern Pattern string
 **/
void filtered_menu_pattern_match(struct filtered_menu *list, const char *pattern);

/**
 * Filter displayed menu entries according to whitelist field in TLE db.
 *
 * \param list Menu
 * \param db TLE db
 **/
void filtered_menu_show_whitelisted(struct filtered_menu *list, const struct tle_db *db);

/**
 * Modify "enabled"-flag in TLE db entries based on the current enabled/disabled flags in the menu.
 *
 * \param list Menu struct
 * \param db TLE db to modify
 **/
void filtered_menu_to_tle_db(struct filtered_menu *list, struct tle_db *db);

/**
 * Toggle all currently _displayed_ menu entries (some/none enabled -> all enabled, all enabled -> none enabled)
 *
 * \param list Menu struct
 **/
void filtered_menu_toggle(struct filtered_menu *list);

/**
 * Handle keyboard commands to menu.
 *
 * \param list Menu struct
 * \param c Keyboard character
 * \return True if keyboard character was handled, false otherwise.
 **/
bool filtered_menu_handle(struct filtered_menu *list, int c);

/**
 * Set/unset option for being able to select multiple entries in menu.
 *
 * \param list Menu struct
 * \param Toggle True if multimark is to be set, false if only a single entry can be selected in the menu
 **/
void filtered_menu_set_multimark(struct filtered_menu *list, bool toggle);


#endif
