#include <form.h>
#include <menu.h>
#include "filtered_menu.h"
#include <libgen.h>
#include "defines.h"
#include "tle_db.h"
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

bool pattern_match(const char *string, const char *pattern)
{
	return (strlen(pattern) == 0) || (strstr(string, pattern) != NULL);
}

void filtered_menu_entry_free(struct filtered_menu_entry *list_entry)
{
	free(list_entry->displayed_name);
	for (int i=0; i < list_entry->num_descriptors; i++) {
		free(list_entry->descriptors[i]);
	}
	free(list_entry->descriptors);
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
}

void filtered_menu_pattern_match(struct filtered_menu *list, const char *pattern)
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

	ITEM **temp_items = (ITEM **)calloc(list->num_entries + 1, sizeof(ITEM *));
	int item_ind = 0;

	//put all items corresponding to input pattern into current list of displayed items, update entry mapping
	for (int i = 0; i < list->num_entries; ++i) {
		if (pattern_match(list->entries[i].descriptors[0], pattern)) {
			temp_items[item_ind] = new_item(list->entries[i].displayed_name, "");
			list->entry_mapping[item_ind] = i;
			item_ind++;
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

void filtered_menu_from_stringarray(struct filtered_menu *list, int num_descriptors, string_array_t *descriptors, WINDOW *my_menu_win)
{
	//initialize member variables based on tle database
	list->num_displayed_entries = 0;
	list->num_entries = string_array_size(&(descriptors[0]));
	list->displayed_entries = (ITEM **)calloc(list->num_entries + 1, sizeof(ITEM*));
	list->entry_mapping = (int*)calloc(list->num_entries, sizeof(int));
	list->entries = (struct filtered_menu_entry*)malloc(sizeof(struct filtered_menu_entry)*list->num_entries);
	for (int i=0; i < list->num_entries; i++) {
		list->entries[i].displayed_name = strdup(string_array_get(&(descriptors[0]), i));
		list->entries[i].num_descriptors = num_descriptors;
		list->entries[i].descriptors = (char**)malloc(sizeof(char*)*list->entries[i].num_descriptors);
		for (int j=0; j < num_descriptors; j++) {
			list->entries[i].descriptors[j] = strdup(string_array_get(&(descriptors[j]), i));
		}
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
	set_menu_sub(my_menu, derwin(my_menu_win, max_height - 3, max_width - 2, 2, 1));
	set_menu_format(my_menu, LINES-14, 1);

	set_menu_mark(my_menu, " * ");
	menu_opts_off(my_menu, O_ONEVALUE);
	post_menu(my_menu);

	//display all items, ensure the rest of the variables are correctly set
	filtered_menu_pattern_match(list, "");
}

void filtered_menu_from_tle_db(struct filtered_menu *list, const struct tle_db *db, WINDOW *my_menu_win)
{
	int num_descriptors = 2;
	string_array_t *string_list = (string_array_t*)calloc(num_descriptors, sizeof(string_array_t));
	for (int i=0; i < db->num_tles; i++) {
		string_array_add(&(string_list[0]), db->tles[i].name);
		string_array_add(&(string_list[1]), db->tles[i].filename);
	}

	filtered_menu_from_stringarray(list, num_descriptors, string_list, my_menu_win);

	for (int i=0; i < db->num_tles; i++) {
		list->entries[i].enabled = tle_db_entry_enabled(db, i);
	}
	filtered_menu_pattern_match(list, "");

	string_array_free(&(string_list[0]));
	string_array_free(&(string_list[1]));
	free(string_list);
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

bool filtered_menu_handle(struct filtered_menu *list, int c)
{
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

			int index = list->entry_mapping[item_index(current_item(list->menu))];
			list->entries[index].enabled = !(list->entries[index].enabled);
			break;
		default:
			return false;
			break;
	}
	return true;
}

