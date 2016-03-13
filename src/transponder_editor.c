#include <stdlib.h>
#include "transponder_editor.h"
#include <string.h>
#include "ui.h"
#include "xdg_basedirs.h"

//default style for field
#define TRANSPONDER_ENTRY_DEFAULT_STYLE COLOR_PAIR(1)|A_UNDERLINE

//style of field when the cursor marker is in it
#define TRANSPONDER_ACTIVE_FIELDSTYLE COLOR_PAIR(5)

/**
 * Helper function for creating a FIELD in the transponder editor with the correct default style.
 *
 * \param field_height FIELD height
 * \param field_width FIELD width
 * \param field_row FIELD row
 * \param field_col FIELD col
 * \return Created FIELD struct
 **/
FIELD *create_transponder_editor_field(int field_height, int field_width, int field_row, int field_col)
{
	FIELD *ret_field = new_field(field_height, field_width, field_row, field_col, 0, 0);
	set_field_back(ret_field, TRANSPONDER_ENTRY_DEFAULT_STYLE);
	field_opts_off(ret_field, O_STATIC);
	set_max_field(ret_field, MAX_NUM_CHARS);
	return ret_field;
}

/**
 * Check whether transponder editor line has been edited.
 *
 * \param transponder_line Transponder editor line
 * \return True if it (actually, only the transponder name) has been edited, false if not
 **/
bool transponder_editor_line_is_edited(struct transponder_editor_line *transponder_line)
{
	return (field_status(transponder_line->name) == TRUE);
}

/**
 * Set transponder editor line visible according to input flag.
 *
 * \param transponder_line Transponder editor line
 * \param visible Visibility flag
 **/
void transponder_editor_line_set_visible(struct transponder_editor_line *transponder_line, bool visible)
{
	if (visible) {
		field_opts_on(transponder_line->name, O_VISIBLE);

		field_opts_on(transponder_line->uplink[0], O_VISIBLE);
		field_opts_on(transponder_line->uplink[1], O_VISIBLE);

		field_opts_on(transponder_line->downlink[0], O_VISIBLE);
		field_opts_on(transponder_line->downlink[1], O_VISIBLE);
	} else {
		field_opts_off(transponder_line->name, O_VISIBLE);

		field_opts_off(transponder_line->uplink[0], O_VISIBLE);
		field_opts_off(transponder_line->uplink[1], O_VISIBLE);

		field_opts_off(transponder_line->downlink[0], O_VISIBLE);
		field_opts_off(transponder_line->downlink[1], O_VISIBLE);
	}
}

/**
 * Clear transponder editor line of all editable information.
 *
 * \param line Transponder editor line to clear
 **/
void transponder_editor_line_clear(struct transponder_editor_line *line)
{
	set_field_buffer(line->name, 0, "");
	set_field_buffer(line->uplink[0], 0, "");
	set_field_buffer(line->uplink[1], 0, "");
	set_field_buffer(line->downlink[0], 0, "");
	set_field_buffer(line->downlink[1], 0, "");
}

//height of fields
#define FIELD_HEIGHT 1

//lengths of transponder editor line fields
#define TRANSPONDER_DESCRIPTION_LENGTH 42
#define TRANSPONDER_FREQUENCY_LENGTH 10
#define TRANSPONDER_NAME_LENGTH 20

//spacing between fields
#define SPACING 2

/**
 * Create new transponder editor line.
 *
 * \param row Row at which to place the fields. Will occupy the rows corresponding to `row` and `row+1`.
 * \return Create transponder editor line
 **/
struct transponder_editor_line* transponder_editor_line_create(int row)
{
	struct transponder_editor_line *ret_line = (struct transponder_editor_line*)malloc(sizeof(struct transponder_editor_line));

	ret_line->name = create_transponder_editor_field(FIELD_HEIGHT, TRANSPONDER_NAME_LENGTH, row, 1);

	ret_line->uplink[0] = create_transponder_editor_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row, TRANSPONDER_NAME_LENGTH+1+SPACING);
	ret_line->uplink[1] = create_transponder_editor_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row+2, TRANSPONDER_NAME_LENGTH+1+SPACING);

	ret_line->downlink[0] = create_transponder_editor_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row, TRANSPONDER_NAME_LENGTH+TRANSPONDER_FREQUENCY_LENGTH+1+2*SPACING);
	ret_line->downlink[1] = create_transponder_editor_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row+2, TRANSPONDER_NAME_LENGTH+TRANSPONDER_FREQUENCY_LENGTH+1+2*SPACING);

	return ret_line;
}

void transponder_editor_print_page_number(struct transponder_editor *transponder_editor)
{
	mvwprintw(transponder_editor->editor_window, transponder_editor->window_rows-2, 61, "Page %d of %d", transponder_editor->curr_page_number+1, transponder_editor->num_pages);
}


/**
 * Set transponder line editors visible.
 *
 * \param transponder_editor Transponder editor
 * \param num_visible_entries Number of transponders to make visible
 **/
void transponder_editor_set_visible(struct transponder_editor *transponder_editor, int num_visible_entries)
{
	int end_ind = 0;
	for (int i=0; i < (num_visible_entries) && (i < MAX_NUM_TRANSPONDERS); i++) {
		transponder_editor_line_set_visible(transponder_editor->transponders[i], true);
		end_ind++;
	}
	for (int i=end_ind; i < MAX_NUM_TRANSPONDERS; i++) {
		transponder_editor_line_set_visible(transponder_editor->transponders[i], false);
	}
	transponder_editor->num_editable_transponders = end_ind;

	//update current last visible field in form
	transponder_editor->last_field_in_form = transponder_editor->transponders[end_ind-1]->downlink[1];

	//update page number
	transponder_editor->num_pages = (transponder_editor->num_editable_transponders-1)/transponder_editor->transponders_per_page+1;
	if (transponder_editor->num_pages > 1) {
		transponder_editor_print_page_number(transponder_editor);
	}
}

/**
 * Fill fields in transponder editor with the information contained in the database entry.
 *
 * \param transponder_editor Transponder editor
 * \param db_entry Database entry
 **/
void transponder_editor_fill(struct transponder_editor *transponder_editor, struct sat_db_entry *db_entry)
{
	char temp[MAX_NUM_CHARS];

	if (db_entry->squintflag) {
		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->alon);
		set_field_buffer(transponder_editor->alon, 0, temp);

		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->alat);
		set_field_buffer(transponder_editor->alat, 0, temp);
	}

	for (int i=0; i < db_entry->num_transponders; i++) {
		set_field_buffer(transponder_editor->transponders[i]->name, 0, db_entry->transponder_name[i]);

		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->uplink_start[i]);
		set_field_buffer(transponder_editor->transponders[i]->uplink[0], 0, temp);

		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->uplink_end[i]);
		set_field_buffer(transponder_editor->transponders[i]->uplink[1], 0, temp);

		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->downlink_start[i]);
		set_field_buffer(transponder_editor->transponders[i]->downlink[0], 0, temp);

		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->downlink_end[i]);
		set_field_buffer(transponder_editor->transponders[i]->downlink[1], 0, temp);
	}
}

/**
 * Print keybinding hint as KEY: DESCRIPTION with different colorformatting of KEY.
 *
 * \param window Window to print hints in
 * \param key Key
 * \param description Description
 * \param row Row
 * \param col Column
 **/
void print_hint(WINDOW *window, const char *key, const char *description, int row, int col)
{
	wattrset(window, A_BOLD);
	mvwprintw(window, row, col, "%s:", key);

	wattrset(window, COLOR_PAIR(3)|A_BOLD);
	mvwprintw(window, row+1, col, "%s", description);
}

/**
 * Display transponder editor keybinding hints.
 *
 * \param window Window in which the keybinding hints are to be displayed
 **/
void transponder_editor_keybindings(WINDOW *window, int row, int col)
{
	print_hint(window, "DELETE", "Clear field", row, col);
	row += 3;
	print_hint(window, "CTRL + W", "Clear all fields", row, col);
	row += 3;
	print_hint(window, "CTRL + R", "Restore entry to system default", row, col);
	row += 4;
	print_hint(window, "ESCAPE", "Save and exit", row, col);
	row += 3;
	print_hint(window, "PGUP/PGDN", "Next/previous page", row, col);
}

//length of fields for squint variable editing
#define SQUINT_LENGTH 10
#define SQUINT_DESCRIPTION_LENGTH 40

//number of fields in one transponder editor line
#define NUM_FIELDS_IN_ENTRY (NUM_TRANSPONDER_SPECIFIERS*2 + 1)

//number of allowed rows per transponder page, for scrolling
#define NUM_ROWS_PER_TRANSPONDER_PAGE 20

struct transponder_editor* transponder_editor_create(const struct tle_db_entry *sat_info, WINDOW *window, struct sat_db_entry *db_entry)
{
	struct transponder_editor *new_editor = (struct transponder_editor*)malloc(sizeof(struct transponder_editor));
	new_editor->satellite_number = sat_info->satellite_number;
	new_editor->editor_window = window;

	//create FIELDs for squint angle properties
	int row = 0;
	new_editor->squint_description = new_field(FIELD_HEIGHT, SQUINT_DESCRIPTION_LENGTH, row++, 1, 0, 0);
	set_field_buffer(new_editor->squint_description, 0, "Alon        Alat");
	field_opts_off(new_editor->squint_description, O_ACTIVE);
	set_field_back(new_editor->squint_description, COLOR_PAIR(4)|A_BOLD);

	new_editor->alon = create_transponder_editor_field(FIELD_HEIGHT, SQUINT_LENGTH, row, 1);
	new_editor->alat = create_transponder_editor_field(FIELD_HEIGHT, SQUINT_LENGTH, row++, 1 + SQUINT_LENGTH + SPACING);
	row++;

	//create FIELDs for each editable field
	new_editor->transponder_description = new_field(FIELD_HEIGHT, TRANSPONDER_DESCRIPTION_LENGTH, row++, 1, 0, 0);
	set_field_back(new_editor->transponder_description, COLOR_PAIR(4)|A_BOLD);
	set_field_buffer(new_editor->transponder_description, 0, "Transponder name      Uplink      Downlink");
	field_opts_off(new_editor->transponder_description, O_ACTIVE);

	new_editor->tot_num_pages = 1;
	new_editor->num_pages = 1;
	new_editor->transponders_per_page = 0;
	bool first_page = false;
	for (int i=0; i < MAX_NUM_TRANSPONDERS; i++) {
		bool page_break = false;
		if (row > NUM_ROWS_PER_TRANSPONDER_PAGE) {
			row = 0;
			page_break = true;
			new_editor->tot_num_pages++;
			first_page = true;
		}

		if (!first_page) {
			new_editor->transponders_per_page++;
		}

		new_editor->transponders[i] = transponder_editor_line_create(row);
		row += 4;

		//set page break at appropriate entry
		if (page_break) {
			set_new_page(new_editor->transponders[i]->name, true);
		}
	}
	new_editor->curr_page_number = 0;

	//create horrible FIELD array for input into the FORM
	FIELD **fields = calloc(NUM_FIELDS_IN_ENTRY*MAX_NUM_TRANSPONDERS + 5, sizeof(FIELD*));
	fields[0] = new_editor->squint_description;
	fields[1] = new_editor->alon;
	fields[2] = new_editor->alat;
	fields[3] = new_editor->transponder_description;

	for (int i=0; i < MAX_NUM_TRANSPONDERS; i++) {
		int field_index = i*NUM_FIELDS_IN_ENTRY + 4;
		fields[field_index] = new_editor->transponders[i]->name;
		fields[field_index + 1] = new_editor->transponders[i]->uplink[0];
		fields[field_index + 2] = new_editor->transponders[i]->downlink[0];
		fields[field_index + 3] = new_editor->transponders[i]->uplink[1];
		fields[field_index + 4] = new_editor->transponders[i]->downlink[1];
	}
	fields[NUM_FIELDS_IN_ENTRY*MAX_NUM_TRANSPONDERS + 4] = NULL;
	new_editor->form = new_form(fields);
	new_editor->field_list = fields;

	//scale input window to the form
	int rows, cols;
	scale_form(new_editor->form, &rows, &cols);
	int win_width = cols+30;
	int win_height = rows+6;
	wresize(window, win_height, win_width);
	keypad(window, TRUE);
	wattrset(window, COLOR_PAIR(4));
	box(window, 0, 0);
	new_editor->window_rows = win_height;

	//sub window for form
	set_form_win(new_editor->form, window);
	set_form_sub(new_editor->form, derwin(window, rows, cols, 2, 2));

	post_form(new_editor->form);
	refresh();

	form_driver(new_editor->form, REQ_VALIDATION);

	//mark first field with different background color
	new_editor->curr_selected_field = current_field(new_editor->form);
	set_field_back(new_editor->curr_selected_field, TRANSPONDER_ACTIVE_FIELDSTYLE);

	//fill fields with transponder database information
	transponder_editor_fill(new_editor, db_entry);

	//display satellite name on top
	mvwprintw(window, 0, 5, "%s", sat_info->name);

	//display key binding hints
	int key_binding_col = cols + 4;
	WINDOW *key_binding_window = derwin(window, 0, win_width-key_binding_col-2, 2, key_binding_col);
	transponder_editor_keybindings(key_binding_window, 0, 0);

	if (new_editor->num_pages > 1) {
		transponder_editor_print_page_number(new_editor);
	}

	transponder_editor_set_visible(new_editor, db_entry->num_transponders+1);
	form_driver(new_editor->form, REQ_VALIDATION);

	return new_editor;
}

void transponder_editor_line_destroy(struct transponder_editor_line **line)
{
	free_field((*line)->name);
	free_field((*line)->uplink[0]);
	free_field((*line)->uplink[1]);
	free_field((*line)->downlink[0]);
	free_field((*line)->downlink[1]);

	free(*line);
	*line = NULL;
}

void transponder_editor_destroy(struct transponder_editor **transponder_editor)
{
	wclear((*transponder_editor)->editor_window);
	unpost_form((*transponder_editor)->form);
	free_form((*transponder_editor)->form);
	free_field((*transponder_editor)->alat);
	free_field((*transponder_editor)->alon);
	free_field((*transponder_editor)->squint_description);
	free_field((*transponder_editor)->transponder_description);
	for (int i=0; i < MAX_NUM_TRANSPONDERS; i++) {
		transponder_editor_line_destroy(&((*transponder_editor)->transponders[i]));
	}
	free((*transponder_editor)->field_list);
	free(*transponder_editor);
	*transponder_editor = NULL;
}

void transponder_editor_sysdefault(struct transponder_editor *transponder_editor, struct sat_db_entry *sat_db_entry)
{
	//create dummy TLE database with a single entry corresponding to the satellite number
	struct tle_db *dummy_tle_db = tle_db_create();
	struct transponder_db *dummy_transponder_db = transponder_db_create();
	struct tle_db_entry dummy_entry;
	dummy_entry.satellite_number = transponder_editor->satellite_number;
	tle_db_add_entry(dummy_tle_db, &dummy_entry);

	//read from XDG_DATA_DIRS
	string_array_t data_dirs = {0};
	char *data_dirs_str = xdg_data_dirs();
	stringsplit(data_dirs_str, &data_dirs);
	free(data_dirs_str);

	for (int i=string_array_size(&data_dirs)-1; i >= 0; i--) {
		char db_path[MAX_NUM_CHARS] = {0};
		snprintf(db_path, MAX_NUM_CHARS, "%s%s", string_array_get(&data_dirs, i), DB_RELATIVE_FILE_PATH);
		transponder_db_from_file(db_path, dummy_tle_db, dummy_transponder_db, LOCATION_DATA_DIRS);
	}
	string_array_free(&data_dirs);

	//copy entry fields to input satellite database entry and the transponder editor fields
	transponder_db_entry_copy(sat_db_entry, &(dummy_transponder_db->sats[0]));
	transponder_editor_fill(transponder_editor, &(dummy_transponder_db->sats[0]));

	transponder_editor_set_visible(transponder_editor, sat_db_entry->num_transponders+1);

	tle_db_destroy(&dummy_tle_db);
	transponder_db_destroy(&dummy_transponder_db);
}

/**
 * Clear transponder editor of all information.
 *
 * \param transponder_editor Transponder editor
 **/
void transponder_editor_clear(struct transponder_editor *transponder_editor)
{
	set_field_buffer(transponder_editor->alon, 0, "");
	set_field_buffer(transponder_editor->alat, 0, "");

	//clear editable lines
	for (int i=0; i < transponder_editor->num_editable_transponders-1; i++) {
		transponder_editor_line_clear(transponder_editor->transponders[i]);
	}

	//clear last line if it has been edited
	struct transponder_editor_line *last_line = transponder_editor->transponders[transponder_editor->num_editable_transponders-1];
	if (transponder_editor_line_is_edited(last_line)) {
		transponder_editor_line_clear(last_line);
	}
}

void transponder_editor_handle(struct transponder_editor *transponder_editor, int c)
{
	//handle keyboard
	switch (c) {
		case KEY_UP:
			form_driver(transponder_editor->form, REQ_UP_FIELD);
			break;
		case KEY_DOWN:
			form_driver(transponder_editor->form, REQ_DOWN_FIELD);
			break;
		case KEY_LEFT:
			form_driver(transponder_editor->form, REQ_LEFT_FIELD);
			break;
		case KEY_RIGHT:
			form_driver(transponder_editor->form, REQ_RIGHT_FIELD);
			break;
		case 10:
			form_driver(transponder_editor->form, REQ_NEXT_FIELD);
			break;
		case KEY_BACKSPACE:
			form_driver(transponder_editor->form, REQ_DEL_PREV);
			form_driver(transponder_editor->form, REQ_VALIDATION);
			break;
		case KEY_PPAGE:
			if (transponder_editor->curr_page_number > 0) {
				form_driver(transponder_editor->form, REQ_PREV_PAGE);
				transponder_editor->curr_page_number = (transponder_editor->curr_page_number - 1 + transponder_editor->num_pages) % (transponder_editor->num_pages);
			}
			break;
		case KEY_NPAGE:
			if (transponder_editor->curr_page_number+1 < transponder_editor->num_pages) {
				form_driver(transponder_editor->form, REQ_NEXT_PAGE);
				transponder_editor->curr_page_number = (transponder_editor->curr_page_number + 1 + transponder_editor->num_pages) % (transponder_editor->num_pages);
			}
			break;
		case KEY_DC:
			form_driver(transponder_editor->form, REQ_CLR_FIELD);
			break;
		case 23: //CTRL + W
			transponder_editor_clear(transponder_editor);
			break;
		default:
			form_driver(transponder_editor->form, c);
			form_driver(transponder_editor->form, REQ_VALIDATION); //update buffer with field contents
			break;
	}

	if (transponder_editor->num_pages > 1) {
		transponder_editor_print_page_number(transponder_editor);
	}

	//switch background color of currently marked field, reset previous field
	FIELD *curr_field = current_field(transponder_editor->form);
	if (curr_field != transponder_editor->curr_selected_field) {
		set_field_back(transponder_editor->curr_selected_field, TRANSPONDER_ENTRY_DEFAULT_STYLE);
		set_field_back(curr_field, TRANSPONDER_ACTIVE_FIELDSTYLE);
		form_driver(transponder_editor->form, REQ_VALIDATION);
		transponder_editor->curr_selected_field = curr_field;
	}

	//add a new transponder editor field if last entry has been edited
	if ((transponder_editor->num_editable_transponders < MAX_NUM_TRANSPONDERS) && (transponder_editor_line_is_edited(transponder_editor->transponders[transponder_editor->num_editable_transponders-1]))) {
		transponder_editor_set_visible(transponder_editor, transponder_editor->num_editable_transponders+1);
	}
}

void transponder_editor_to_db_entry(struct transponder_editor *transponder_editor, struct sat_db_entry *db_entry)
{
	//get squint angle variables
	char *alon_str = strdup(field_buffer(transponder_editor->alon, 0));
	trim_whitespaces_from_end(alon_str);
	char *alat_str = strdup(field_buffer(transponder_editor->alat, 0));
	trim_whitespaces_from_end(alat_str);
	double alon = strtod(alon_str, NULL);
	double alat = strtod(alat_str, NULL);
	db_entry->squintflag = false;
	if ((strlen(alon_str) > 0) || (strlen(alat_str) > 0)) {
		db_entry->alon = alon;
		db_entry->alat = alat;
		db_entry->squintflag = true;
	}
	free(alon_str);
	free(alat_str);

	int entry_index = 0;
	for (int i=0; i < transponder_editor->num_editable_transponders; i++) {
		//get name from transponder entry
		struct transponder_editor_line *line = transponder_editor->transponders[i];
		char temp[MAX_NUM_CHARS];
		strncpy(temp, field_buffer(line->name, 0), MAX_NUM_CHARS);
		trim_whitespaces_from_end(temp);

		//get uplink and downlink frequencies
		double uplink_start = strtod(field_buffer(line->uplink[0], 0), NULL);
		double uplink_end = strtod(field_buffer(line->uplink[1], 0), NULL);

		double downlink_start = strtod(field_buffer(line->downlink[0], 0), NULL);
		double downlink_end = strtod(field_buffer(line->downlink[1], 0), NULL);

		//add to database if transponder name is defined and there are non-zero starting frequencies
		if ((strlen(temp) > 0) && ((uplink_start != 0.0) || (downlink_start != 0.0))) {
			strncpy(db_entry->transponder_name[entry_index], temp, MAX_NUM_CHARS);

			if (uplink_end == 0.0) {
				uplink_end = uplink_start;
			}
			if (uplink_start == 0.0) {
				uplink_end = 0.0;
			}

			if (downlink_end == 0.0) {
				downlink_end = downlink_start;
			}
			if (downlink_start == 0.0) {
				downlink_end = 0.0;
			}

			db_entry->uplink_start[entry_index] = uplink_start;
			db_entry->uplink_end[entry_index] = uplink_end;
			db_entry->downlink_start[entry_index] = downlink_start;
			db_entry->downlink_end[entry_index] = downlink_end;

			entry_index++;
		}
	}

	db_entry->num_transponders = entry_index;
	db_entry->location |= LOCATION_TRANSIENT;
}
