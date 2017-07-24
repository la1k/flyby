#include <stdlib.h>
#include "transponder_editor.h"
#include <string.h>
#include "ui.h"
#include "xdg_basedirs.h"
#include <math.h>
#include <menu.h>
#include "filtered_menu.h"

#include <form.h>
#include "defines.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// transponder_form and related structs and functions, for handling fields and forms related to the transponder editor //
// defined further below.                                                                                              //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//number of fields needed for defining transponder frequencies
#define NUM_TRANSPONDER_SPECIFIERS 2

/**
 * Fields for single transponder.
 **/
struct transponder_form_line {
	///Field for editing transponder name
	FIELD *name;
	///Field for editing uplink frequency interval (position 0: Interval start, position 1: interval end)
	FIELD *uplink[NUM_TRANSPONDER_SPECIFIERS];
	///Field for editing downlink frequency interval (position 0: Interval start, position 1: interval end)
	FIELD *downlink[NUM_TRANSPONDER_SPECIFIERS];
};

/**
 * Form for full transponder database entry.
 **/
struct transponder_form {
	///Satellite number of the satellite to be edited
	long satellite_number;
	///Form containing all fields in the transponder form
	FORM *form;
	///Field array used in the form. Contains pointers to the FIELD entries defined below and in struct transponder_form_line.
	FIELD **field_list;
	///Field for editing attitude latitude for squint angle calculation
	FIELD *alat;
	///Field for editing attitude longitude for squint angle calculation
	FIELD *alon;
	///Squint field description
	FIELD *squint_description;
	///Transponder fields description
	FIELD *transponder_description;
	///Number of editable transponder entries
	int num_editable_transponders;
	///Transponder entries
	struct transponder_form_line *transponders[MAX_NUM_TRANSPONDERS];
	///Currently selected field in form
	FIELD *curr_selected_field;
	///Last selectable field in form
	FIELD *last_field_in_form;
	///Total number of available form pages, including invisible entries
	int tot_num_pages;
	///Number of visible form pages
	int num_pages;
	///Current page number
	int curr_page_number;
	///Number of transponders per form page
	int transponders_per_page;
	///Window editor within which editor is contained
	WINDOW *editor_window;
	///Number of rows in window
	int window_rows;
};

/**
 * Create transponder form.
 *
 * \param sat_info TLE entry, used for getting satellite name and number
 * \param window Window to put the FIELDs in. Will be resized to fit the FORM
 * \param db_entry Database entry
 **/
struct transponder_form* transponder_form_create(const struct tle_db_entry *sat_info, WINDOW *window, struct sat_db_entry *db_entry);

/**
 * Restore satellite transponder entry to the system default defined in XDG_DATA_DIRS.
 *
 * \param transponder_form Transponder form, in which the fields are restored to the system default
 * \param sat_db_entry Satellite database entry to restore to system default
 **/
void transponder_form_sysdefault(struct transponder_form *transponder_form, struct sat_db_entry *sat_db_entry);

/**
 * Destroy transponder form.
 *
 * \param transponder_form Transponder form to free
 **/
void transponder_form_destroy(struct transponder_form **transponder_form);

/**
 * Handle input character to transponder form.
 *
 * \param transponder_form Transponder form
 * \param c Input character
 **/
void transponder_form_handle(struct transponder_form *transponder_form, int c);

/**
 * Convert information in transponder form fields to database fields
 *
 * \param transponder_form Transponder form
 * \param db_entry Database entry
 **/
void transponder_form_to_db_entry(struct transponder_form *transponder_form, struct sat_db_entry *db_entry);

/**
 * Display transponder form form and edit the transponder entry.
 *
 * - Transponder entry is not changed: Nothing happens. Entries from XDG_DATA_HOME remain in the user database, nothing happens to entries defined in XDG_DATA_DIRS.
 * - Transponder entry is changed: Mark with LOCATION_TRANSIENT, will be written to user database.
 * - Transponder entry is restored to system default: Is marked with LOCATION_DATA_DIRS, will not be written to user database in order to not override the system database.
 *
 * \param sat_info TLE database entry, used for getting satellite name and satellite number for later lookup for entry defined in XDG_DATA_DIRS
 * \param form_win Window to put the editor in
 * \param sat_entry Satellite database entry to edit
 **/
void transponder_database_entry_editor(const struct tle_db_entry *sat_info, WINDOW *form_win, struct sat_db_entry *sat_entry);

/**
 * Display transponder database entry.
 *
 * \param name Satellite name
 * \param entry Transponder database entry to display
 * \param display_window Display window to display the entry in
 **/
void transponder_database_entry_displayer(const char *name, struct sat_db_entry *entry, WINDOW *display_window);


//default style for field
#define TRANSPONDER_ENTRY_DEFAULT_STYLE FIELDSTYLE_INACTIVE

//style of field when the cursor marker is in it
#define TRANSPONDER_ACTIVE_FIELDSTYLE FIELDSTYLE_ACTIVE

/**
 * Helper function for creating a FIELD in the transponder form with the correct default style.
 *
 * \param field_height FIELD height
 * \param field_width FIELD width
 * \param field_row FIELD row
 * \param field_col FIELD col
 * \return Created FIELD struct
 **/
FIELD *create_transponder_form_field(int field_height, int field_width, int field_row, int field_col)
{
	FIELD *ret_field = new_field(field_height, field_width, field_row, field_col, 0, 0);
	set_field_back(ret_field, TRANSPONDER_ENTRY_DEFAULT_STYLE);
	field_opts_off(ret_field, O_STATIC);
	set_max_field(ret_field, MAX_NUM_CHARS);
	return ret_field;
}

/**
 * Check whether transponder form line has been edited.
 *
 * \param transponder_line Transponder form line
 * \return True if it (actually, only the transponder name) has been edited, false if not
 **/
bool transponder_form_line_is_edited(struct transponder_form_line *transponder_line)
{
	return (field_status(transponder_line->name) == TRUE);
}

/**
 * Set transponder form line visible according to input flag.
 *
 * \param transponder_line Transponder form line
 * \param visible Visibility flag
 **/
void transponder_form_line_set_visible(struct transponder_form_line *transponder_line, bool visible)
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
 * Clear transponder form line of all editable information.
 *
 * \param line Transponder form line to clear
 **/
void transponder_form_line_clear(struct transponder_form_line *line)
{
	set_field_buffer(line->name, 0, "");
	set_field_buffer(line->uplink[0], 0, "");
	set_field_buffer(line->uplink[1], 0, "");
	set_field_buffer(line->downlink[0], 0, "");
	set_field_buffer(line->downlink[1], 0, "");
}

//height of fields
#define FIELD_HEIGHT 1

//height of description field (Transponder     Uplink ...)
#define DESCRIPTION_FIELD_HEIGHT (FIELD_HEIGHT+1)

//lengths of transponder form line fields
#define TRANSPONDER_DESCRIPTION_LENGTH 42
#define TRANSPONDER_FREQUENCY_LENGTH 10
#define TRANSPONDER_NAME_LENGTH 20

//spacing between fields
#define SPACING 2

/**
 * Create new transponder form line.
 *
 * \param row Row at which to place the fields. Will occupy the rows corresponding to `row` and `row+1`.
 * \return Create transponder form line
 **/
struct transponder_form_line* transponder_form_line_create(int row)
{
	struct transponder_form_line *ret_line = (struct transponder_form_line*)malloc(sizeof(struct transponder_form_line));

	ret_line->name = create_transponder_form_field(FIELD_HEIGHT, TRANSPONDER_NAME_LENGTH, row, 1);

	ret_line->uplink[0] = create_transponder_form_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row, TRANSPONDER_NAME_LENGTH+1+SPACING);
	ret_line->uplink[1] = create_transponder_form_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row+2, TRANSPONDER_NAME_LENGTH+1+SPACING);

	ret_line->downlink[0] = create_transponder_form_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row, TRANSPONDER_NAME_LENGTH+TRANSPONDER_FREQUENCY_LENGTH+1+2*SPACING);
	ret_line->downlink[1] = create_transponder_form_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row+2, TRANSPONDER_NAME_LENGTH+TRANSPONDER_FREQUENCY_LENGTH+1+2*SPACING);

	return ret_line;
}

void transponder_form_print_page_number(struct transponder_form *transponder_form)
{
	mvwprintw(transponder_form->editor_window, transponder_form->window_rows-2, 61, "Page %d of %d", transponder_form->curr_page_number+1, transponder_form->num_pages);
}


/**
 * Set transponder line editors visible.
 *
 * \param transponder_form Transponder form
 * \param num_visible_entries Number of transponders to make visible
 **/
void transponder_form_set_visible(struct transponder_form *transponder_form, int num_visible_entries)
{
	int end_ind = 0;
	for (int i=0; i < (num_visible_entries) && (i < MAX_NUM_TRANSPONDERS); i++) {
		transponder_form_line_set_visible(transponder_form->transponders[i], true);
		end_ind++;
	}
	for (int i=end_ind; i < MAX_NUM_TRANSPONDERS; i++) {
		transponder_form_line_set_visible(transponder_form->transponders[i], false);
	}
	transponder_form->num_editable_transponders = end_ind;

	//update current last visible field in form
	transponder_form->last_field_in_form = transponder_form->transponders[end_ind-1]->downlink[1];

	//update page number
	if (transponder_form->num_editable_transponders > (transponder_form->transponders_per_page-1)) {
		transponder_form->num_pages = ceil((transponder_form->num_editable_transponders+1)/(transponder_form->transponders_per_page*1.0)); //+1 to account for alon/alat fields
	} else {
		transponder_form->num_pages = 1;
	}
	if (transponder_form->num_pages > 1) {
		transponder_form_print_page_number(transponder_form);
	}
}

/**
 * Fill fields in transponder form with the information contained in the database entry.
 *
 * \param transponder_form Transponder form
 * \param db_entry Database entry
 **/
void transponder_form_fill(struct transponder_form *transponder_form, struct sat_db_entry *db_entry)
{
	char temp[MAX_NUM_CHARS];

	if (db_entry->squintflag) {
		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->alon);
		set_field_buffer(transponder_form->alon, 0, temp);

		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->alat);
		set_field_buffer(transponder_form->alat, 0, temp);
	}

	for (int i=0; i < db_entry->num_transponders; i++) {
		struct transponder *transponder = &(db_entry->transponders[i]);
		set_field_buffer(transponder_form->transponders[i]->name, 0, transponder->name);

		if (transponder->uplink_start != 0.0) {
			snprintf(temp, MAX_NUM_CHARS, "%f", transponder->uplink_start);
			set_field_buffer(transponder_form->transponders[i]->uplink[0], 0, temp);

			snprintf(temp, MAX_NUM_CHARS, "%f", transponder->uplink_end);
			set_field_buffer(transponder_form->transponders[i]->uplink[1], 0, temp);
		}

		if (transponder->downlink_start != 0.0) {
			snprintf(temp, MAX_NUM_CHARS, "%f", transponder->downlink_start);
			set_field_buffer(transponder_form->transponders[i]->downlink[0], 0, temp);

			snprintf(temp, MAX_NUM_CHARS, "%f", transponder->downlink_end);
			set_field_buffer(transponder_form->transponders[i]->downlink[1], 0, temp);
		}
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
 * Display transponder form keybinding hints.
 *
 * \param window Window in which the keybinding hints are to be displayed
 **/
void transponder_form_keybindings(WINDOW *window, int row, int col)
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

//number of fields in one transponder form line
#define NUM_FIELDS_IN_ENTRY (NUM_TRANSPONDER_SPECIFIERS*2 + 1)

//number of rows occupied by one transponder field
#define NUM_ROWS_PER_TRANSPONDER 4

struct transponder_form* transponder_form_create(const struct tle_db_entry *sat_info, WINDOW *window, struct sat_db_entry *db_entry)
{
	struct transponder_form *new_editor = (struct transponder_form*)malloc(sizeof(struct transponder_form));
	new_editor->satellite_number = sat_info->satellite_number;
	new_editor->editor_window = window;

	//create FIELDs for squint angle properties
	int row = 0;
	new_editor->squint_description = new_field(FIELD_HEIGHT, SQUINT_DESCRIPTION_LENGTH, row++, 1, 0, 0);
	set_field_buffer(new_editor->squint_description, 0, "Alon        Alat");
	field_opts_off(new_editor->squint_description, O_ACTIVE);
	set_field_back(new_editor->squint_description, FIELDSTYLE_DESCRIPTION);

	new_editor->alon = create_transponder_form_field(FIELD_HEIGHT, SQUINT_LENGTH, row, 1);
	new_editor->alat = create_transponder_form_field(FIELD_HEIGHT, SQUINT_LENGTH, row++, 1 + SQUINT_LENGTH + SPACING);
	row++;

	//create FIELDs for each editable field
	new_editor->transponder_description = new_field(DESCRIPTION_FIELD_HEIGHT, TRANSPONDER_DESCRIPTION_LENGTH, row, 1, 0, 0);
	row += DESCRIPTION_FIELD_HEIGHT;
	set_field_back(new_editor->transponder_description, FIELDSTYLE_DESCRIPTION);
	set_field_buffer(new_editor->transponder_description, 0, "Transponder name      Uplink      Downlink"
								 "                      (MHz)       (Mhz)   ");
	field_opts_off(new_editor->transponder_description, O_ACTIVE);

	int win_row = getbegy(window);
	int num_rows_per_transponder_page = LINES-win_row-6;

	new_editor->tot_num_pages = 1;
	new_editor->num_pages = 1;
	new_editor->transponders_per_page = 0;
	bool first_page = false;
	for (int i=0; i < MAX_NUM_TRANSPONDERS; i++) {
		bool page_break = false;
		if ((row + NUM_ROWS_PER_TRANSPONDER) > num_rows_per_transponder_page) {
			row = 0;
			page_break = true;
			new_editor->tot_num_pages++;
			first_page = true;
		}

		if (!first_page) {
			new_editor->transponders_per_page++;
		}

		new_editor->transponders[i] = transponder_form_line_create(row);
		row += NUM_ROWS_PER_TRANSPONDER;

		//set page break at appropriate entry
		if (page_break) {
			set_new_page(new_editor->transponders[i]->name, true);
		}
	}
	new_editor->transponders_per_page++; //account for the fact that we would have had space for one transponder extra on the first page if we didn't have alon/alat info there
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
	int win_height = rows+5;
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
	transponder_form_fill(new_editor, db_entry);

	//display satellite name on top
	mvwprintw(window, 0, 5, "%s", sat_info->name);

	//display key binding hints
	int key_binding_col = cols + 4;
	WINDOW *key_binding_window = derwin(window, 0, win_width-key_binding_col-2, 2, key_binding_col);
	transponder_form_keybindings(key_binding_window, 0, 0);

	if (new_editor->num_pages > 1) {
		transponder_form_print_page_number(new_editor);
	}

	transponder_form_set_visible(new_editor, db_entry->num_transponders+1);
	form_driver(new_editor->form, REQ_VALIDATION);

	return new_editor;
}

void transponder_form_line_destroy(struct transponder_form_line **line)
{
	free_field((*line)->name);
	free_field((*line)->uplink[0]);
	free_field((*line)->uplink[1]);
	free_field((*line)->downlink[0]);
	free_field((*line)->downlink[1]);

	free(*line);
	*line = NULL;
}

void transponder_form_destroy(struct transponder_form **transponder_form)
{
	wclear((*transponder_form)->editor_window);
	unpost_form((*transponder_form)->form);
	free_form((*transponder_form)->form);
	free_field((*transponder_form)->alat);
	free_field((*transponder_form)->alon);
	free_field((*transponder_form)->squint_description);
	free_field((*transponder_form)->transponder_description);
	for (int i=0; i < MAX_NUM_TRANSPONDERS; i++) {
		transponder_form_line_destroy(&((*transponder_form)->transponders[i]));
	}
	free((*transponder_form)->field_list);
	free(*transponder_form);
	*transponder_form = NULL;
}

void transponder_form_sysdefault(struct transponder_form *transponder_form, struct sat_db_entry *sat_db_entry)
{
	//create dummy TLE database with a single entry corresponding to the satellite number
	struct tle_db *dummy_tle_db = tle_db_create();
	struct tle_db_entry dummy_entry;
	dummy_entry.satellite_number = transponder_form->satellite_number;
	tle_db_add_entry(dummy_tle_db, &dummy_entry);
	struct transponder_db *dummy_transponder_db = transponder_db_create(dummy_tle_db);

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

	//copy entry fields to input satellite database entry and the transponder form fields
	transponder_db_entry_copy(sat_db_entry, &(dummy_transponder_db->sats[0]));
	transponder_form_fill(transponder_form, &(dummy_transponder_db->sats[0]));

	transponder_form_set_visible(transponder_form, sat_db_entry->num_transponders+1);

	tle_db_destroy(&dummy_tle_db);
	transponder_db_destroy(&dummy_transponder_db);
}

/**
 * Clear transponder form of all information.
 *
 * \param transponder_form Transponder form
 **/
void transponder_form_clear(struct transponder_form *transponder_form)
{
	set_field_buffer(transponder_form->alon, 0, "");
	set_field_buffer(transponder_form->alat, 0, "");

	//clear editable lines
	for (int i=0; i < transponder_form->num_editable_transponders-1; i++) {
		transponder_form_line_clear(transponder_form->transponders[i]);
	}

	//clear last line if it has been edited
	struct transponder_form_line *last_line = transponder_form->transponders[transponder_form->num_editable_transponders-1];
	if (transponder_form_line_is_edited(last_line)) {
		transponder_form_line_clear(last_line);
	}
}

void transponder_form_handle(struct transponder_form *transponder_form, int c)
{
	//handle keyboard
	switch (c) {
		case KEY_UP:
			form_driver(transponder_form->form, REQ_UP_FIELD);
			break;
		case KEY_DOWN:
			form_driver(transponder_form->form, REQ_DOWN_FIELD);
			break;
		case KEY_LEFT:
			form_driver(transponder_form->form, REQ_LEFT_FIELD);
			break;
		case KEY_RIGHT:
			form_driver(transponder_form->form, REQ_RIGHT_FIELD);
			break;
		case 10:
			form_driver(transponder_form->form, REQ_NEXT_FIELD);
			break;
		case KEY_BACKSPACE:
			form_driver(transponder_form->form, REQ_DEL_PREV);
			form_driver(transponder_form->form, REQ_VALIDATION);
			break;
		case KEY_PPAGE:
			if (transponder_form->curr_page_number > 0) {
				form_driver(transponder_form->form, REQ_PREV_PAGE);
				transponder_form->curr_page_number = (transponder_form->curr_page_number - 1 + transponder_form->num_pages) % (transponder_form->num_pages);
			}
			break;
		case KEY_NPAGE:
			if (transponder_form->curr_page_number+1 < transponder_form->num_pages) {
				form_driver(transponder_form->form, REQ_NEXT_PAGE);
				transponder_form->curr_page_number = (transponder_form->curr_page_number + 1 + transponder_form->num_pages) % (transponder_form->num_pages);
			}
			break;
		case KEY_DC:
			form_driver(transponder_form->form, REQ_CLR_FIELD);
			break;
		case 23: //CTRL + W
			transponder_form_clear(transponder_form);
			break;
		default:
			form_driver(transponder_form->form, c);
			form_driver(transponder_form->form, REQ_VALIDATION); //update buffer with field contents
			break;
	}

	if (transponder_form->num_pages > 1) {
		transponder_form_print_page_number(transponder_form);
	}

	//switch background color of currently marked field, reset previous field
	FIELD *curr_field = current_field(transponder_form->form);
	if (curr_field != transponder_form->curr_selected_field) {
		set_field_back(transponder_form->curr_selected_field, TRANSPONDER_ENTRY_DEFAULT_STYLE);
		set_field_back(curr_field, TRANSPONDER_ACTIVE_FIELDSTYLE);
		form_driver(transponder_form->form, REQ_VALIDATION);
		transponder_form->curr_selected_field = curr_field;
	}

	//add a new transponder form field if last entry has been edited
	if ((transponder_form->num_editable_transponders < MAX_NUM_TRANSPONDERS) && (transponder_form_line_is_edited(transponder_form->transponders[transponder_form->num_editable_transponders-1]))) {
		transponder_form_set_visible(transponder_form, transponder_form->num_editable_transponders+1);
	}
}

void transponder_form_to_db_entry(struct transponder_form *transponder_form, struct sat_db_entry *db_entry)
{
	//get squint angle variables
	char *alon_str = strdup(field_buffer(transponder_form->alon, 0));
	trim_whitespaces_from_end(alon_str);
	char *alat_str = strdup(field_buffer(transponder_form->alat, 0));
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
	for (int i=0; i < transponder_form->num_editable_transponders; i++) {
		//get name from transponder entry
		struct transponder_form_line *line = transponder_form->transponders[i];
		char temp[MAX_NUM_CHARS];
		strncpy(temp, field_buffer(line->name, 0), MAX_NUM_CHARS);
		trim_whitespaces_from_end(temp);

		//get uplink and downlink frequencies
		double uplink_start = strtod(field_buffer(line->uplink[0], 0), NULL);
		double uplink_end = strtod(field_buffer(line->uplink[1], 0), NULL);

		double downlink_start = strtod(field_buffer(line->downlink[0], 0), NULL);
		double downlink_end = strtod(field_buffer(line->downlink[1], 0), NULL);

		//add to returned database entry if transponder name is defined
		if (strlen(temp) > 0) {
			strncpy(db_entry->transponders[entry_index].name, temp, MAX_NUM_CHARS);

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

			db_entry->transponders[entry_index].uplink_start = uplink_start;
			db_entry->transponders[entry_index].uplink_end = uplink_end;
			db_entry->transponders[entry_index].downlink_start = downlink_start;
			db_entry->transponders[entry_index].downlink_end = downlink_end;

			entry_index++;
		}
	}

	db_entry->num_transponders = entry_index;
	db_entry->location |= LOCATION_TRANSIENT;
}

//////////////////////////////////////////////////
// Transponder editor UI handlers/entry points. //
//////////////////////////////////////////////////

void transponder_database_entry_editor(const struct tle_db_entry *sat_info, WINDOW *form_win, struct sat_db_entry *sat_entry)
{
	struct transponder_form *transponder_form = transponder_form_create(sat_info, form_win, sat_entry);

	wrefresh(form_win);
	bool run_form = true;
	while (run_form) {
		int c = wgetch(form_win);
		if ((c == 27) || ((c == 10) && (transponder_form->curr_selected_field == transponder_form->last_field_in_form))) {
			run_form = false;
		} else if (c == 18) { //CTRL + R
			transponder_form_sysdefault(transponder_form, sat_entry);
		} else {
			transponder_form_handle(transponder_form, c);
		}

		wrefresh(form_win);
	}

	struct sat_db_entry new_entry;
	transponder_db_entry_copy(&new_entry, sat_entry);

	transponder_form_to_db_entry(transponder_form, &new_entry);

	//ensure that we don't write an empty entry (or the same system database entry) to the file database unless we are actually trying to override a system database entry
	if (!transponder_db_entry_equal(&new_entry, sat_entry)) {
		transponder_db_entry_copy(sat_entry, &new_entry);
	}

	transponder_form_destroy(&transponder_form);

	delwin(form_win);
}

void transponder_database_entry_displayer(const char *name, struct sat_db_entry *entry, WINDOW *display_window)
{
	werase(display_window);

	//display satellite name
	wattrset(display_window, A_BOLD);

	int data_col = 15;
	int info_col = 1;
	int start_row = 0;
	int row = start_row;

	//file location information
	wattrset(display_window, FIELDSTYLE_DESCRIPTION);
	if (entry->location & LOCATION_TRANSIENT) {
		mvwprintw(display_window, row++, info_col, "To be written to user database.");
	} else if (entry->location & LOCATION_DATA_HOME) {
		mvwprintw(display_window, row++, info_col, "Loaded from user database.");
	} else if (entry->location & LOCATION_DATA_DIRS) {
		mvwprintw(display_window, row++, info_col, "Loaded from system dirs.");
	}

	if ((entry->location & LOCATION_DATA_DIRS) && ((entry->location & LOCATION_DATA_HOME) || (entry->location & LOCATION_TRANSIENT))) {
		mvwprintw(display_window, row++, info_col, "A system default exists.");
	}

	row = start_row+3;

	//display squint angle information
	wattrset(display_window, FIELDSTYLE_DESCRIPTION);
	mvwprintw(display_window, row, info_col, "Squint angle:");

	wattrset(display_window, COLOR_PAIR(2)|A_BOLD);
	if (entry->squintflag) {
		mvwprintw(display_window, row++, data_col, "Enabled.");

		wattrset(display_window, FIELDSTYLE_DESCRIPTION);
		mvwprintw(display_window, row++, info_col, "alat: ");
		mvwprintw(display_window, row, info_col, "alon: ");

		wattrset(display_window, COLOR_PAIR(2)|A_BOLD);
		mvwprintw(display_window, row-1, data_col, "%f", entry->alat);
		mvwprintw(display_window, row, data_col, "%f", entry->alon);
	} else {
		mvwprintw(display_window, row, data_col, "Disabled");
	}
	row = start_row+6;

	//display transponder information
	int rows_per_entry = 3;
	int prev_row_diff = 0;
	for (int i=0; i < entry->num_transponders; i++) {
		int display_row = row;
		struct transponder transponder = entry->transponders[i];

		if (display_row + rows_per_entry < LINES-8) {
			int start_row = display_row;
			int info_col = 1;
			int data_col = 4;
			if ((i % 2) == 1) {
				info_col = 25;
				data_col = info_col+3;
			}

			wattrset(display_window, A_BOLD);
			mvwprintw(display_window, ++display_row, info_col, "%.20s", transponder.name);

			//uplink
			if (transponder.uplink_start != 0.0) {
				wattrset(display_window, FIELDSTYLE_DESCRIPTION);
				mvwprintw(display_window, ++display_row, info_col, "U:");

				wattrset(display_window, COLOR_PAIR(2)|A_BOLD);
				mvwprintw(display_window, display_row, data_col, "%.2f-%.2f", transponder.uplink_start, transponder.uplink_end);
			}

			//downlink
			if (transponder.downlink_start != 0.0) {
				wattrset(display_window, FIELDSTYLE_DESCRIPTION);
				mvwprintw(display_window, ++display_row, info_col, "D:");

				wattrset(display_window, COLOR_PAIR(2)|A_BOLD);
				mvwprintw(display_window, display_row, data_col, "%.2f-%.2f", transponder.downlink_start, transponder.downlink_end);
			}

			//no uplink/downlink defined
			if ((transponder.uplink_start == 0.0) && (transponder.downlink_start == 0.0)) {
				wattrset(display_window, COLOR_PAIR(2)|A_BOLD);
				mvwprintw(display_window, ++display_row, info_col, "Neither downlink or");
				mvwprintw(display_window, ++display_row, info_col, "uplink is defined.");
				mvwprintw(display_window, ++display_row, info_col, "(Will be ignored)");
			}
			display_row++;

			int diff = display_row-start_row;
			if ((i % 2) == 1) {
				if (diff < prev_row_diff) {
					diff = prev_row_diff;
				}
				row += diff;
			} else {
				prev_row_diff = diff;
			}
		} else {
			wattrset(display_window, FIELDSTYLE_DESCRIPTION);
			mvwprintw(display_window, ++display_row, info_col, "Truncated to %d of %d transponder entries.", i, entry->num_transponders);
			break;
		}
	}

	//default text when no transponders are defined
	if (entry->num_transponders <= 0) {
		wattrset(display_window, FIELDSTYLE_DESCRIPTION);
		mvwprintw(display_window, ++row, info_col, "No transponders defined.");
	}
}

void transponder_database_editor(int start_index, struct tle_db *tle_db, struct transponder_db *sat_db)
{
	//print header
	clear();
	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);

	int header_height = 3;
	int win_width = 251;
	mvprintw(0,0,"                                                                                ");
	mvprintw(1,0,"  flyby Transponder Database Editor                                             ");
	mvprintw(2,0,"                                                                                ");

	//prepare the other windows
	WINDOW *main_win = newwin(LINES-header_height, win_width, header_height, 0);
	int window_width = 25;
	int window_ypos = header_height+1;
	WINDOW *menu_win = subwin(main_win, LINES-window_ypos-1, window_width, window_ypos, 1);
	WINDOW *display_win = subwin(main_win, LINES-window_ypos-1, 50, window_ypos, window_width+5);
	WINDOW *editor_win = newwin(LINES, 500, 4, 1);

	keypad(menu_win, TRUE);
	wattrset(menu_win, COLOR_PAIR(4));

	//prepare menu
	struct filtered_menu menu = {0};
	filtered_menu_from_tle_db(&menu, tle_db, menu_win);
	filtered_menu_set_multimark(&menu, false);
	filtered_menu_show_whitelisted(&menu, tle_db);

	if (start_index < 0) {
		start_index = 0;
	}
	if ((start_index == 0) && (menu.inverse_entry_mapping[0] == -1)) {
		start_index = menu.entry_mapping[0];
	}

	if (menu.num_displayed_entries > 0) {
		int tle_index = start_index;
		transponder_database_entry_displayer(tle_db->tles[tle_index].name, &(sat_db->sats[tle_index]), display_win);
	}

	filtered_menu_select_index(&menu, start_index);

	box(menu_win, 0, 0);

	refresh();
	wrefresh(display_win);
	wrefresh(menu_win);

	bool run_menu = true;
	while (run_menu) {
		//handle keyboard
		int c = wgetch(menu_win);
		filtered_menu_handle(&menu, c);
		int menu_index = filtered_menu_current_index(&menu);

		if ((c == 10) && (menu.num_displayed_entries > 0)) { //enter
			transponder_database_entry_editor(&(tle_db->tles[menu_index]), editor_win, &(sat_db->sats[menu_index]));

			//clear leftovers from transponder editor
			wclear(main_win);
			wrefresh(main_win);
			refresh();

			//force menu update
			unpost_menu(menu.menu);
			post_menu(menu.menu);

			//refresh the rest and redraw window boxes
			box(menu_win, 0, 0);
			wrefresh(menu_win);
		} else if (c == 'q') {
			run_menu = false;
		}

		//display/refresh transponder entry displayer
		if (menu.num_displayed_entries > 0) {
			transponder_database_entry_displayer(tle_db->tles[menu_index].name, &(sat_db->sats[menu_index]), display_win);
		}
		wrefresh(display_win);
	}
	filtered_menu_free(&menu);

	//write transponder database to file
	if (tle_db->num_tles > 0) {
		transponder_db_write_to_default(tle_db, sat_db);
	}

	//read transponder database from file again in order to set the flags correctly
	transponder_db_from_search_paths(tle_db, sat_db);

	delwin(display_win);
	delwin(main_win);
	delwin(menu_win);
	delwin(editor_win);
}


