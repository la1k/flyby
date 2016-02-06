#include <form.h>

void prepare_pattern(char *string)
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

bool check_pattern(const char *string, const char *pattern) {
	return (strlen(pattern) == 0) || (strstr(string, pattern) != NULL);
}

ITEM** prepare_menu_items(const struct tle_db *tle_db, const char *pattern, int *tle_index)
{
	int max_num_choices = tle_db->num_tles;
	ITEM **my_items = (ITEM **)calloc(max_num_choices + 1, sizeof(ITEM *));
	int item_ind = 0;
	
	//get all TLE names corresponding to the input pattern
	for (int i = 0; i < max_num_choices; ++i) {
		if (check_pattern(tle_db->tles[i].name, pattern)) {
			my_items[item_ind] = new_item(tle_db->tles[i].name, "");
			tle_index[item_ind] = i;
			item_ind++;
		}
	}
	my_items[item_ind] = NULL; //terminate the menu list
	return my_items;
}

void mark_checked_tles(int num_tles, bool *enabled_tles, int *tle_index, ITEM** items)
{
	for (int i=0; i < num_tles; i++) {
		if (items[i] == NULL) {
			break;
		}

		int index = tle_index[i];
		if (enabled_tles[index]) {
			set_item_value(items[i], TRUE);
		} else {
			set_item_value(items[i], FALSE);
		}
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

void toggle_all(int num_tles, ITEM** items, bool *enabled_tles, int *tle_index)
{
	//check if all items in menu are enabled
	bool all_enabled = true;
	for (int i=0; i < num_tles; i++) {
		if (items[i] == NULL) {
			break;
		}
		if (item_value(items[i]) == FALSE) {
			all_enabled = false;
		}
	}

	for (int i=0; i < num_tles; i++) {
		if (items[i] == NULL) {
			break;
		}

		if (all_enabled) {
			set_item_value(items[i], FALSE);
			enabled_tles[tle_index[i]] = false;
		} else {
			set_item_value(items[i], TRUE);
			enabled_tles[tle_index[i]] = true;
		}
	}
}

void whitelist(struct tle_db *tle_db)
{
	int retval;
	int c;
	MENU *my_menu;
	WINDOW *my_menu_win;

	bool *enabled_tles = (bool*)calloc(tle_db->num_tles, sizeof(bool));
	int *tle_index = (int*)calloc(tle_db->num_tles, sizeof(int));
	//FIXME: Read from file, read from settings, read from whatever.
	
	/* Print header */
	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	clear();

	int row = 0;
	mvprintw(row++,0,"                                                                                ");
	mvprintw(row++,0,"  flyby TLE whitelister                                                         ");
	mvprintw(row++,0,"                                                                                ");
	
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
	mvprintw( row++,col,"Use lower-case characters to ");
	mvprintw( row++,col,"filter satellites by name.");
	row++;
	mvprintw( row++,col,"Press 'Q' to return to menu.");
	mvprintw( row++,col,"Press 'A' to toggle all TLES.");
	mvprintw( row++,col,"Press 'W' to wipe query field.");
	mvprintw(6, 4, "Filter TLEs:");

	refresh();

	/* Create items */
	if (tle_db->num_tles > 0) {
		ITEM **items = prepare_menu_items(tle_db, "", tle_index);
		ITEM **temp_items = NULL;
		char field_contents[MAX_NUM_CHARS] = {0};
		char curr_item[MAX_NUM_CHARS] = {0};

		bool valid_menu = true;

		/* Create menu */
		my_menu = new_menu(items);

		set_menu_back(my_menu,COLOR_PAIR(1));
		set_menu_fore(my_menu,COLOR_PAIR(5)|A_BOLD);

		/* Set main window and sub window */
		set_menu_win(my_menu, my_menu_win);
		set_menu_sub(my_menu, derwin(my_menu_win, LINES-(window_ypos + 5), window_width - 2, 2, 1));
		set_menu_format(my_menu, LINES-14, 1);

		/* Set menu mark to the string " * " */
		set_menu_mark(my_menu, " * ");

		menu_opts_off(my_menu, O_ONEVALUE);
		mark_checked_tles(tle_db->num_tles, enabled_tles, tle_index, items);

		/* Post the menu */
		post_menu(my_menu);

		refresh();
		wrefresh(my_menu_win);
		form_driver(form, REQ_VALIDATION);
		wrefresh(form_win);

		while (true) {
			c = wgetch(my_menu_win);
			switch(c) {
				case 'Q':
					return;
				case KEY_DOWN:
					if (valid_menu) menu_driver(my_menu, REQ_DOWN_ITEM);
					break;
				case KEY_UP:
					if (valid_menu) menu_driver(my_menu, REQ_UP_ITEM);
					break;
				case KEY_NPAGE:
					if (valid_menu) menu_driver(my_menu, REQ_SCR_DPAGE);
					break;
				case KEY_PPAGE:
					if (valid_menu) menu_driver(my_menu, REQ_SCR_UPAGE);
					break;
				case 'A':
					if (valid_menu) toggle_all(tle_db->num_tles, items, enabled_tles, tle_index);
					break;
				case ' ':
					if (valid_menu) {
						pos_menu_cursor(my_menu);
						menu_driver(my_menu, REQ_TOGGLE_ITEM);
						int index = tle_index[item_index(current_item(my_menu))];
						enabled_tles[index] = !enabled_tles[index];
					}
					break;
				case KEY_BACKSPACE:
					form_driver(form, REQ_DEL_PREV);
				default:
					if (islower(c) || isdigit(c)) {
						form_driver(form, c);
					}
					if (c == 'W') {
						form_driver(form, REQ_CLR_FIELD);
					}

					form_driver(form, REQ_VALIDATION); //update buffer with field contents

					strncpy(field_contents, field_buffer(field[0], 0), MAX_NUM_CHARS);
					prepare_pattern(field_contents);
			
					strncpy(curr_item, item_name(current_item(my_menu)), MAX_NUM_CHARS);

					/* Update menu with new items */
					temp_items = prepare_menu_items(tle_db, field_contents, tle_index);

					if (valid_menu) {
						unpost_menu(my_menu);
					}

					if (temp_items[0] != NULL) {
						retval = set_menu_items(my_menu, temp_items);
					} else {
						retval = E_BAD_ARGUMENT;
					}

					if (retval != E_OK) {
						valid_menu = false;
					} else {
						valid_menu = true;
						post_menu(my_menu);
					}

					/* remove old items */
					if (retval == E_OK) {
						free_menu_items(&items);
						items = temp_items;
						set_menu_pattern(my_menu, curr_item);
						mark_checked_tles(tle_db->num_tles, enabled_tles, tle_index, items);
					} else {
						free_menu_items(&temp_items);
					}

					wrefresh(form_win);
					break;
			}
			wrefresh(my_menu_win);
		}

		/* Unpost and free all the memory taken up */
		unpost_menu(my_menu);
		free_menu(my_menu);

		free_menu_items(&items);
	} else {
		refresh();
		wrefresh(my_menu_win);
		c = wgetch(my_menu_win);
	}

	free(enabled_tles);
}

void orbital_elements_from_whitelist()
{
}

void whitelist_from_file()
{
}
