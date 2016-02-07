#include <form.h>

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

bool pattern_match(const char *string, const char *pattern) {
	return (strlen(pattern) == 0) || (strstr(string, pattern) != NULL);
}

struct selectable_list_entry {
	char *displayed_name;
	bool enabled;

	int num_descriptors;
	char **descriptors;
};

struct pattern_match {
	int num_matches;
	char **matches;
};

struct selectable_list {
	int max_num_items;
	ITEM** displayed_entries;
	int num_displayed_entries;
	MENU* menu;
	struct selectable_list_entry *entries;
	int *entry_mapping;
};

void selectable_list_entry_free(struct selectable_list_entry *list_entry) {
	free(list_entry->displayed_name);
	for (int i=0; i < list_entry->num_descriptors; i++) {
		free(list_entry->descriptors[i]);
	}
	free(list_entry->descriptors);
}

void selectable_list_free(struct selectable_list *list) {
	if (list->num_displayed_entries > 0) {
		unpost_menu(list->menu);
	}
	free_menu(list->menu);
	free_menu_items(&(list->displayed_entries));

	for (int i=0; i < list->max_num_items; i++) {
		selectable_list_entry_free(&(list->entries[i]));
	}
	free(list->entries);
	free(list->entry_mapping);
}

void selectable_list_pattern_match(struct selectable_list *list, const char *pattern);

void selectable_list_from_tle_db(struct selectable_list *list, const struct tle_db *db, WINDOW *my_menu_win) {
	list->num_displayed_entries = 0;
	list->max_num_items = db->num_tles;
	list->displayed_entries = (ITEM **)calloc(list->max_num_items + 1, sizeof(ITEM*));
	list->entry_mapping = (int*)calloc(list->max_num_items, sizeof(int));
	list->entries = (struct selectable_list_entry*)malloc(sizeof(struct selectable_list_entry)*list->max_num_items);
	for (int i=0; i < db->num_tles; i++) {
		list->entries[i].displayed_name = strdup(db->tles[i].name);
		list->entries[i].num_descriptors = 2;
		list->entries[i].descriptors = (char**)malloc(sizeof(char*)*list->entries[i].num_descriptors);
		list->entries[i].descriptors[0] = strdup(db->tles[i].name);
		list->entries[i].descriptors[1] = strdup(db->tles[i].filename);
		list->entries[i].enabled = tle_db_entry_enabled(db, i);

		list->displayed_entries[i] = new_item(list->entries[i].displayed_name, "");
	}
	list->displayed_entries[db->num_tles] = NULL;
	list->num_displayed_entries = db->num_tles;

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

	selectable_list_pattern_match(list, "");
}

void selectable_list_pattern_match(struct selectable_list *list, const char *pattern) {
	/*
	if (valid_menu) {
		strncpy(curr_item, item_name(current_item(my_menu)), MAX_NUM_CHARS);
	}*/

	if (list->num_displayed_entries > 0) {
		unpost_menu(list->menu);
		list->num_displayed_entries = 0;
	}

	ITEM **temp_items = (ITEM **)calloc(list->max_num_items + 1, sizeof(ITEM *));
	int item_ind = 0;

	//get all TLE names corresponding to the input pattern
	for (int i = 0; i < list->max_num_items; ++i) {
		if (pattern_match(list->entries[i].descriptors[0], pattern)) {
			temp_items[item_ind] = new_item(list->entries[i].displayed_name, "");
			list->entry_mapping[item_ind] = i;
			item_ind++;
		}
	}
	temp_items[item_ind] = NULL; //terminate the menu list

	if (temp_items[0] != NULL) {
		set_menu_items(list->menu, temp_items);
		list->num_displayed_entries = item_ind;
		free_menu_items(&(list->displayed_entries));
		list->displayed_entries = temp_items;
		post_menu(list->menu);
	} else {
		free(temp_items);
	}

	/* remove old items
	if (retval == E_OK) {
	free_menu_items(&items);
	items = temp_items;
	set_menu_pattern(my_menu, curr_item);
	mark_checked_tles(tle_db, tle_index, items);
	} else {
	free_menu_items(&temp_items);
	}
	return my_items;*/

	for (int i=0; i < list->max_num_items; i++) {
		if (list->displayed_entries[i] == NULL) {
			break;
		}

		int index = list->entry_mapping[i];
		if (list->entries[index].enabled) {
			set_item_value(list->displayed_entries[i], TRUE);
		} else {
			set_item_value(list->displayed_entries[i], FALSE);
		}
	}
}

void selectable_list_to_tle_db(struct selectable_list *list, struct tle_db *db) {
	for (int i=0; i < list->max_num_items; i++) {
		tle_db_entry_set_enabled(db, i, list->entries[i].enabled);
	}
}

void selectable_list_toggle(struct selectable_list *list);

bool selectable_list_handle(struct selectable_list *list, int c) {
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
			selectable_list_toggle(list);
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

void selectable_list_toggle(struct selectable_list *list) {
	//check if all items in menu are enabled
	bool all_enabled = true;
	for (int i=0; i < list->num_displayed_entries; i++) {
		if (item_value(list->displayed_entries[i]) == FALSE) {
			all_enabled = false;
		}
	}

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
	WINDOW *form_win = newwin(rows + 4, cols + 4, row, 16);
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
	int window_width = 40;
	int window_ypos = row;
	my_menu_win = newwin(LINES-window_ypos-1, window_width, window_ypos, 4);
	keypad(my_menu_win, TRUE);
	wattrset(my_menu_win, COLOR_PAIR(4));
	box(my_menu_win, 0, 0);

	attrset(COLOR_PAIR(3)|A_BOLD);
	int col = 46;
	mvprintw( row++,col,"Use cursor keys to move up/down");
	mvprintw( row++,col,"the list and then select with ");
	mvprintw( row++,col,"the 'Space' key.");
	row++;
	mvprintw( row++,col,"Use upper-case characters to ");
	mvprintw( row++,col,"filter satellites by name.");
	row++;
	mvprintw( row++,col,"Press 'q' to return to menu.");
	mvprintw( row++,col,"Press 'a' to toggle all TLES.");
	mvprintw( row++,col,"Press 'w' to wipe query field.");
	mvprintw(6, 4, "Filter TLEs:");

	refresh();

	struct selectable_list menu = {0};
	selectable_list_from_tle_db(&menu, tle_db, my_menu_win);

	char field_contents[MAX_NUM_CHARS] = {0};

	refresh();
	wrefresh(my_menu_win);
	form_driver(form, REQ_VALIDATION);
	wrefresh(form_win);
	bool run_menu = true;

	while (run_menu) {
		c = wgetch(my_menu_win);

		bool handled = selectable_list_handle(&menu, c);
		wrefresh(my_menu_win);

		if (!handled) {
			switch (c) {
				case 'q':
					run_menu = false;
					break;
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

					selectable_list_pattern_match(&menu, field_contents);

					wrefresh(form_win);
					break;
			}
		}
	}

	selectable_list_to_tle_db(&menu, tle_db);
	selectable_list_free(&menu);

	//update file
	whitelist_write_to_default(tle_db);

	free_field(field[0]);
	free(tle_index);
}
