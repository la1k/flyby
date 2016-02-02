#include <form.h>

enum tristate {UNCHECKED = 0, CHECKED = 1, HALF_CHECKED = 2};

struct nested_tle_menu {
	enum tristate *checked_filenames;
	enum tristate **checked_tles;

	ITEM **filename_items;
	ITEM ***tle_items;
};

typedef struct {
	int length;
	int x_position;
	int y_position;

	char *string;
	int position;
} pattern_field_t;

void pattern_field_refresh(pattern_field_t *pattern_field)
{
	//print current string in field
	attrset(COLOR_PAIR(1));
	mvprintw(pattern_field->y_position, pattern_field->x_position, pattern_field->string);
	
	//print cursor
	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(pattern_field->y_position, pattern_field->x_position + pattern_field->position, " ");
}

pattern_field_t* pattern_field_new(int ypos, int xpos, int length)
{
	pattern_field_t *ret = (pattern_field_t*)malloc(sizeof(pattern_field_t));

	ret->length = length;
	ret->x_position = xpos;
	ret->y_position = ypos;
	ret->position = 0;

	ret->string = (char*)malloc(sizeof(char)*(length + 1));
	for (int i=0; i < length; i++) {
		ret->string[i] = ' ';
	}
	ret->string[length-5] = '\0';
	pattern_field_refresh(ret);

	return ret;
}

void pattern_field_in(pattern_field_t *pattern_field, char c)
{
	if (c == 8) { //backspace
		pattern_field->position--;
		pattern_field->string[pattern_field->position] = ' ';
	} else if ((pattern_field->position < pattern_field->length) && (isalpha(c) || isdigit(c))) {
		pattern_field->string[pattern_field->position] = c;
		pattern_field->position++;
	}
	pattern_field_refresh(pattern_field);
}
	

void navigate_files()
{
	
}

void navigate_tles()
{
}

void remove_menu()
{
}

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

ITEM** prepare_menu_items(const struct tle_db *tle_db, const char *pattern)
{
	int max_num_choices = tle_db->num_tles;
	ITEM **my_items = (ITEM **)calloc(max_num_choices + 1, sizeof(ITEM *));
	int item_ind = 0;
	
	//get all TLE names corresponding to the input pattern
	for (int i = 0; i < max_num_choices; ++i) {
		if (check_pattern(tle_db->tles[i].name, pattern)) {
			my_items[item_ind] = new_item(tle_db->tles[i].name, "");
			item_ind++;
		}
	}
	my_items[item_ind] = NULL; //terminate the menu list
	return my_items;
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

void whitelist(struct tle_db *tle_db)
{
	int retval;
	int c;
	MENU *my_menu;
	WINDOW *my_menu_win;
	int n_choices, i, j;
	
	/* Print header */
	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	clear();

	int row = 0;
	mvprintw(row++,0,"                                                                                ");
	mvprintw(row++,0,"  flyby TLE whitelister                                                         ");
	mvprintw(row++,0,"                                                                                ");
	
	if (tle_db->num_tles >= MAX_NUM_SATS)
		mvprintw(LINES-3,46,"Truncated to %d satellites",MAX_NUM_SATS);
	else
		mvprintw(LINES-3,46,"%d satellites",tle_db->num_tles);
	
	/* Create form for query input */
	FIELD *field[2];
	FORM  *form;
	
	field[0] = new_field(1, 10, 1, 1, 0, 0);
	field[1] = NULL;

	set_field_back(field[0], A_UNDERLINE);
	field_opts_off(field[0], O_AUTOSKIP);

	form = new_form(field);
	int rows, cols;
	scale_form(form, &rows, &cols);

	int form_win_height = rows + 4;
	WINDOW *form_win = newwin(rows + 4, cols + 4, row, 4);
	row += form_win_height;
	keypad(form_win, TRUE);
	wattrset(form_win, COLOR_PAIR(4));
	box(form_win, 0, 0);

	/* Set main window and sub window */
	set_form_win(form, form_win);
	set_form_sub(form, derwin(form_win, rows, cols, 2, 2));

	post_form(form);
	wrefresh(form_win);
	
	mvprintw(4, 10, "yo");

	/* Create the window to be associated with the menu */
	int window_width = 40;
	int window_ypos = row;
	my_menu_win = newwin(LINES-window_ypos-1, window_width, window_ypos, 4);
	keypad(my_menu_win, TRUE);
	wattrset(my_menu_win, COLOR_PAIR(4));
	box(my_menu_win, 0, 0);
	
	attrset(COLOR_PAIR(3)|A_BOLD);
	mvprintw( row++,46,"Use cursor keys to move up/down");
	mvprintw( row++,46,"the list and then select with ");
	mvprintw( row++,46,"the 'Enter' key.");
	mvprintw( row++,46,"Press 'q' to return to menu.");


	refresh();

	/* Create items */
	if (tle_db->num_tles > 0) {
		ITEM **items = prepare_menu_items(tle_db, "");
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
		set_menu_format(my_menu, LINES-9, 1);

		/* Set menu mark to the string " * " */
		set_menu_mark(my_menu, " * ");

		menu_opts_off(my_menu, O_ONEVALUE);

		/* Post the menu */
		post_menu(my_menu);

		refresh();
		wrefresh(my_menu_win);

		while (true) {
			c = wgetch(my_menu_win);
			switch(c) {
				case 'q':
					return -1;
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
				case ' ':
					if (valid_menu) {
						pos_menu_cursor(my_menu);
						menu_driver(my_menu, REQ_TOGGLE_ITEM);
					}
					break;
				case KEY_BACKSPACE:
					form_driver(form, REQ_DEL_PREV);
				default:
					form_driver(form, c);
					form_driver(form, REQ_VALIDATION); //update buffer with field contents

					strncpy(field_contents, field_buffer(field[0], 0), MAX_NUM_CHARS);
					prepare_pattern(field_contents);
			
					strncpy(curr_item, item_name(current_item(my_menu)), MAX_NUM_CHARS);

					/* Update menu with new items */
					temp_items = prepare_menu_items(tle_db, field_contents);

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
		j = -1;
	}

	return j;
}

void orbital_elements_from_whitelist()
{
}

void whitelist_from_file()
{
}
