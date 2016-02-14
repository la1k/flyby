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
	///number of description strings describing the entry, to be used in pattern matching at various levels (e.g: put TLE name in one, international designator in one, filename, ...)
	int num_descriptors;
	///descriptor strings
	char **descriptors;
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
	///name of currently/last marked item (used for keeping cursor position)
	char curr_item[MAX_NUM_CHARS];
};

void filtered_menu_from_stringarray(struct filtered_menu *list, int num_descriptors, string_array_t *descriptors, WINDOW *my_menu_win);

void filtered_menu_from_tle_db(struct filtered_menu *list, const struct tle_db *db, WINDOW *my_menu_win);

void filtered_menu_free(struct filtered_menu *list);

void filtered_menu_pattern_match(struct filtered_menu *list, const char *pattern);


void filtered_menu_to_tle_db(struct filtered_menu *list, struct tle_db *db);

void filtered_menu_toggle(struct filtered_menu *list);

bool filtered_menu_handle(struct filtered_menu *list, int c);


#endif
