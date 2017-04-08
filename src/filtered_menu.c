#include <form.h>
#include <menu.h>
#include "filtered_menu.h"
#include <libgen.h>
#include "defines.h"
#include "tle_db.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void free_menu_items(ITEM ***items)
{
	bool found_end = false;
	int i = 0;
	while (!found_end) {
		if ((*items)[i] == NULL) {
			found_end = true;
		} else {
			free_item((*items)[i]);
			(*items)[i] = NULL;
			i++;
		}
	}

	free(*items);
}

/**
 * Change case of string to uppercase.
 *
 * \param input Input string
 * \return String with all characters set to uppercase. Has to be freed manually.
 **/
char *str_to_uppercase(const char *input)
{
	char *ret_str = strdup(input);
	for (int i=0; i < strlen(ret_str); i++) {
		if (isalpha(ret_str[i])) {
			ret_str[i] = toupper(ret_str[i]);
		}
	}
	return ret_str;
}

bool pattern_match(const char *string, const char *pattern)
{
	return (strlen(pattern) == 0) || (strstr(string, pattern) != NULL);
}

void filtered_menu_entry_free(struct filtered_menu_entry *list_entry)
{
	free(list_entry->displayed_name);
}

void filtered_menu_free(struct filtered_menu *list)
{
	//free and unpost menu
	if (list->num_displayed_entries > 0) {
		unpost_menu(list->menu);
	}
	free_menu(list->menu);

	//free menu items and entries
	free_menu_items(&(list->displayed_entries));
	for (int i=0; i < list->num_entries; i++) {
		filtered_menu_entry_free(&(list->entries[i]));
	}
	free(list->entries);
	free(list->entry_mapping);
	free(list->inverse_entry_mapping);

	delwin(list->sub_window);
}

/**
 * Change displayed items in menu according to input boolean array.
 *
 * \param list Menu
 * \param items_to_display Boolean array with entries corresponding to each entry in the filtered menu, with 0 for entry to hide and 1 for entry to show
 **/
void filtered_menu_update(struct filtered_menu *list, bool *items_to_display)
{
	//keep currently selected item for later cursor jumping
	if (list->num_displayed_entries > 0) {
		strncpy(list->curr_item, item_name(current_item(list->menu)), MAX_NUM_CHARS);
	}

	//remove menu from display
	if (list->num_displayed_entries > 0) {
		unpost_menu(list->menu);
		list->num_displayed_entries = 0;
	}

	//create new entries based on input boolean array and update entry mapping
	ITEM **temp_items = (ITEM **)calloc(list->num_entries + 1, sizeof(ITEM *));
	int item_ind = 0;
	for (int i=0; i < list->num_entries; i++) {
		if (items_to_display[i]) {
			temp_items[item_ind] = new_item(list->entries[i].displayed_name, "");
			list->entry_mapping[item_ind] = i;
			list->inverse_entry_mapping[i] = item_ind;
			item_ind++;
		} else {
			list->inverse_entry_mapping[i] = -1;
		}
	}
	temp_items[item_ind] = NULL; //terminate the menu list

	if (temp_items[0] != NULL) {
		//we got a list of items. Updating menu and posting it again
		set_menu_items(list->menu, temp_items);
		list->num_displayed_entries = item_ind;
		free_menu_items(&(list->displayed_entries));
		list->displayed_entries = temp_items;
		post_menu(list->menu);
		set_menu_pattern(list->menu, list->curr_item);
	} else {
		//no valid list of entries, not doing anything. Menu is kept unposted.
		free(temp_items);
	}

	//select all displayed entries according to whether the canonical entry is selected or not
	for (int i=0; i < list->num_displayed_entries; i++) {
		int index = list->entry_mapping[i];
		if (list->entries[index].enabled) {
			set_item_value(list->displayed_entries[i], TRUE);
		} else {
			set_item_value(list->displayed_entries[i], FALSE);
		}
	}
}

void filtered_menu_simple_pattern_match(struct filtered_menu *list, const char *pattern)
{
	//get boolean array over entries to display or not
	bool *display_items = (bool*)malloc(sizeof(bool)*list->num_entries);
	for (int i = 0; i < list->num_entries; ++i) {
		display_items[i] = false;
		if (pattern_match(list->entries[i].displayed_name, pattern)) {
			display_items[i] = true;
		}
	}

	//update menu
	filtered_menu_update(list, display_items);

	free(display_items);
}

void filtered_menu_pattern_match(struct filtered_menu *list, const struct tle_db *tle_db, const struct transponder_db *transponder_db, const char *pattern)
{
	//get boolean array over entries to display or not
	bool *display_items = (bool*)malloc(sizeof(bool)*list->num_entries);
	for (int i = 0; i < list->num_entries; ++i) {
		display_items[i] = false;

		if (list->display_only_entries_with_transponders && (transponder_db->sats[i].num_transponders == 0)) {
			continue;
		}

		//check against display name
		if (pattern_match(list->entries[i].displayed_name, pattern)) {
			display_items[i] = true;
		}

		//check against TLE filename
		char *fname_uppercase = str_to_uppercase(tle_db->tles[i].filename);
		if (pattern_match(fname_uppercase, pattern)) {
			display_items[i] = true;
		}
		free(fname_uppercase);

		//check against satellite number
		char satnum_str[MAX_NUM_CHARS];
		snprintf(satnum_str, MAX_NUM_CHARS, "%ld", tle_db->tles[i].satellite_number);
		if (pattern_match(satnum_str, pattern)) {
			display_items[i] = true;
		}
	}

	//update menu
	filtered_menu_update(list, display_items);

	free(display_items);
}

void filtered_menu_only_comsats(struct filtered_menu *list, bool on)
{
	list->display_only_entries_with_transponders = on;
}

void filtered_menu_from_stringarray(struct filtered_menu *list, string_array_t *names, WINDOW *my_menu_win)
{
	//initialize member variables based on tle database
	list->num_displayed_entries = 0;
	list->num_entries = string_array_size(names);
	list->displayed_entries = (ITEM **)calloc(list->num_entries + 1, sizeof(ITEM*));
	list->entry_mapping = (int*)calloc(list->num_entries, sizeof(int));
	list->inverse_entry_mapping = (int*)calloc(list->num_entries, sizeof(int));
	list->entries = (struct filtered_menu_entry*)malloc(sizeof(struct filtered_menu_entry)*list->num_entries);
	for (int i=0; i < list->num_entries; i++) {
		list->entries[i].displayed_name = strdup(string_array_get(names, i));
		list->entries[i].enabled = true;

		list->displayed_entries[i] = new_item(list->entries[i].displayed_name, "");
	}
	list->displayed_entries[list->num_entries] = NULL;
	list->num_displayed_entries = list->num_entries;

	//create menu, format menu
	MENU *my_menu = new_menu(list->displayed_entries);
	list->menu = my_menu;
	set_menu_back(my_menu,COLOR_PAIR(1));
	set_menu_fore(my_menu,COLOR_PAIR(5)|A_BOLD);
	set_menu_win(my_menu, my_menu_win);

	int max_width, max_height;
	getmaxyx(my_menu_win, max_height, max_width);
	list->sub_window = derwin(my_menu_win, max_height - 3, max_width - 2, 2, 1);
	set_menu_sub(my_menu, list->sub_window);
	set_menu_format(my_menu, max_height-4, 1);

	set_menu_mark(my_menu, " * ");
	menu_opts_off(my_menu, O_ONEVALUE);
	post_menu(my_menu);

	//display all items, ensure the rest of the variables are correctly set
	filtered_menu_simple_pattern_match(list, "");

	list->display_only_entries_with_transponders = false;
}

void filtered_menu_from_tle_db(struct filtered_menu *list, const struct tle_db *db, WINDOW *my_menu_win)
{
	string_array_t string_list = {0};
	for (int i=0; i < db->num_tles; i++) {
		string_array_add(&string_list, db->tles[i].name);
	}

	filtered_menu_from_stringarray(list, &string_list, my_menu_win);

	for (int i=0; i < db->num_tles; i++) {
		list->entries[i].enabled = tle_db_entry_enabled(db, i);
	}
	filtered_menu_simple_pattern_match(list, "");

	string_array_free(&string_list);
}

void filtered_menu_to_tle_db(struct filtered_menu *list, struct tle_db *db)
{
	for (int i=0; i < list->num_entries; i++) {
		tle_db_entry_set_enabled(db, i, list->entries[i].enabled);
	}
}

void filtered_menu_toggle(struct filtered_menu *list)
{
	//check if all items in menu are enabled
	bool all_enabled = true;
	for (int i=0; i < list->num_displayed_entries; i++) {
		if (item_value(list->displayed_entries[i]) == FALSE) {
			all_enabled = false;
		}
	}

	//disable all items if all were selected, enable all otherwise
	for (int i=0; i < list->num_displayed_entries; i++) {
		if (all_enabled) {
			set_item_value(list->displayed_entries[i], FALSE);
			list->entries[list->entry_mapping[i]].enabled = false;
		} else {
			set_item_value(list->displayed_entries[i], TRUE);
			list->entries[list->entry_mapping[i]].enabled = true;
		}
	}
}

int filtered_menu_index(struct filtered_menu *list, int index)
{
	return list->entry_mapping[index];
}

int filtered_menu_current_index(struct filtered_menu *list)
{
	return filtered_menu_index(list, item_index(current_item(list->menu)));
}

void filtered_menu_select_index(struct filtered_menu *list, int index)
{
	int display_index = list->inverse_entry_mapping[index];
	if (display_index >= 0) {
		set_current_item(list->menu, list->displayed_entries[display_index]);
	}
}

void filtered_menu_show_whitelisted(struct filtered_menu *list, const struct tle_db *db)
{
	bool *display_items = (bool*)malloc(sizeof(bool)*list->num_entries);
	for (int i = 0; i < list->num_entries; ++i) {
		display_items[i] = false;
		if (tle_db_entry_enabled(db, i)) {
			display_items[i] = true;
		}
	}

	filtered_menu_update(list, display_items);

	free(display_items);
}

bool filtered_menu_handle(struct filtered_menu *list, int c)
{
	int index = 0;

	if (list->num_displayed_entries <= 0) {
		return false;
	}

	switch(c) {
		case KEY_DOWN:
		 	menu_driver(list->menu, REQ_DOWN_ITEM);
			break;
		case KEY_UP:
			menu_driver(list->menu, REQ_UP_ITEM);
			break;
		case KEY_NPAGE:
			menu_driver(list->menu, REQ_SCR_DPAGE);
			break;
		case KEY_PPAGE:
			menu_driver(list->menu, REQ_SCR_UPAGE);
			break;
		case 'a':
			filtered_menu_toggle(list);
			break;
		case ' ':
			pos_menu_cursor(list->menu);
			menu_driver(list->menu, REQ_TOGGLE_ITEM);

			index = filtered_menu_current_index(list);
			list->entries[index].enabled = !(list->entries[index].enabled);
			break;
		default:
			return false;
			break;
	}
	return true;
}

void filtered_menu_set_multimark(struct filtered_menu *list, bool toggle)
{
	if (toggle) {
		unpost_menu(list->menu);
		menu_opts_off(list->menu, O_ONEVALUE);
		post_menu(list->menu);
	} else {
		unpost_menu(list->menu);
		menu_opts_on(list->menu, O_ONEVALUE);
		post_menu(list->menu);
	}
}
