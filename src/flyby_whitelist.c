#include <form.h>
#include <libgen.h>
#include "defines.h"
#include "tle_db.h"

void pattern_prepare(char *string)
{
	int length = strlen(string);

	//trim whitespaces from end
	for (int i=length-1; i >= 0; i--) {
		if (string[i] == ' ') {
			string[i] = '\0';
		} else if (isdigit(string[i]) || isalpha(string[i])) {
			break;
		}
	}

	//lowercase to uppercase
	for (int i=0; i < length; i++) {
		string[i] = toupper(string[i]);
	}
}

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

void EditWhitelist(struct tle_db *tle_db)
{
	/* Print header */
	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	clear();

	int row = 0;
	mvprintw(row++,0,"                                                                                ");
	mvprintw(row++,0,"  flyby TLE whitelister                                                         ");
	mvprintw(row++,0,"                                                                                ");

	int c;

	WINDOW *my_menu_win;

	int *tle_index = (int*)calloc(tle_db->num_tles, sizeof(int));

	attrset(COLOR_PAIR(3)|A_BOLD);
	if (tle_db->num_tles >= MAX_NUM_SATS)
		mvprintw(LINES-3,46,"Truncated to %d satellites",MAX_NUM_SATS);
	else
		mvprintw(LINES-3,46,"%d satellites",tle_db->num_tles);

	/* Create form for query input */
	FIELD *field[2];
	FORM  *form;

	field[0] = new_field(1, 24, 1, 1, 0, 0);
	field[1] = NULL;

	set_field_back(field[0], A_UNDERLINE);
	field_opts_off(field[0], O_AUTOSKIP);

	form = new_form(field);
	int rows, cols;
	scale_form(form, &rows, &cols);

	int form_win_height = rows + 4;
	WINDOW *form_win = newwin(rows + 4, cols + 4, row+1, 3);
	row += form_win_height;
	keypad(form_win, TRUE);
	wattrset(form_win, COLOR_PAIR(4));
	box(form_win, 0, 0);

	/* Set main window and sub window */
	set_form_win(form, form_win);
	set_form_sub(form, derwin(form_win, rows, cols, 2, 2));

	post_form(form);
	wrefresh(form_win);

	/* Create the window to be associated with the menu */
	int window_width = 35;
	int window_ypos = row;
	my_menu_win = newwin(LINES-window_ypos-1, window_width, window_ypos, 5);

	keypad(my_menu_win, TRUE);
	wattrset(my_menu_win, COLOR_PAIR(4));
	box(my_menu_win, 0, 0);

	attrset(COLOR_PAIR(3)|A_BOLD);
	int col = 46;
	row = 5;
	mvprintw( row++,col,"Use cursor keys to move up/down");
	mvprintw( row++,col,"the list and then select with ");
	mvprintw( row++,col,"the 'Space' key.");
	row++;
	mvprintw( row++,col,"Use upper-case characters to ");
	mvprintw( row++,col,"filter satellites by name.");
	row++;
	mvprintw( row++,col,"Press 'q' to return to menu.");
	mvprintw( row++,col,"Press 'a' to toggle all displayed");
	mvprintw( row++,col,"TLES.");
	mvprintw( row++,col,"Press 'w' to wipe query field.");
	mvprintw(5, 6, "Filter TLEs by name:");

	refresh();

	struct filtered_menu menu = {0};
	filtered_menu_from_tle_db(&menu, tle_db, my_menu_win);

	char field_contents[MAX_NUM_CHARS] = {0};

	refresh();
	wrefresh(my_menu_win);
	form_driver(form, REQ_VALIDATION);
	wrefresh(form_win);
	bool run_menu = true;

	while (run_menu) {
		//handle keyboard
		c = wgetch(my_menu_win);
		bool handled = false;

		handled = filtered_menu_handle(&menu, c);

		wrefresh(my_menu_win);

		if (!handled) {
			switch (c) {
				case 'q':
					strncpy(field_contents, field_buffer(field[0], 0), MAX_NUM_CHARS);
					pattern_prepare(field_contents);

					if (strlen(field_contents) > 0) {
						c = 'w'; //will jump to clearing of the field
					} else {
						run_menu = false;
						break;
					}
				case KEY_BACKSPACE:
					form_driver(form, REQ_DEL_PREV);
				default:
					if (isupper(c) || isdigit(c)) {
						form_driver(form, c);
					}
					if (c == 'w') {
						form_driver(form, REQ_CLR_FIELD);
					}

					form_driver(form, REQ_VALIDATION); //update buffer with field contents

					strncpy(field_contents, field_buffer(field[0], 0), MAX_NUM_CHARS);
					pattern_prepare(field_contents);

					filtered_menu_pattern_match(&menu, field_contents);

					wrefresh(form_win);
					break;
			}
		}
	}

	filtered_menu_to_tle_db(&menu, tle_db);
	filtered_menu_free(&menu);

	whitelist_write_to_default(tle_db);

	unpost_form(form);

	free(tle_index);
	free_form(form);

	free_field(field[0]);
}
