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

/**
 * Set transponder line editors visible.
 *
 * \param transponder_editor Transponder editor
 * \param num_visible_entries Number of transponders to make visible
 **/
void transponder_editor_set_visible(struct transponder_editor *transponder_entry, int num_visible_entries)
{
	int end_ind = 0;
	for (int i=0; i < (num_visible_entries) && (i < MAX_NUM_TRANSPONDERS); i++) {
		transponder_editor_line_set_visible(transponder_entry->transponders[i], true);
		end_ind++;
	}
	for (int i=end_ind; i < MAX_NUM_TRANSPONDERS; i++) {
		transponder_editor_line_set_visible(transponder_entry->transponders[i], false);
	}
	transponder_entry->num_editable_transponders = end_ind;

	//update current last visible field in form
	transponder_entry->last_field_in_form = transponder_entry->transponders[end_ind-1]->downlink[1];
}

/**
 * Fill fields in transponder editor with the information contained in the database entry.
 *
 * \param entry Transponder editor
 * \param db_entry Database entry
 **/
void transponder_editor_fill(struct transponder_editor *entry, struct sat_db_entry *db_entry)
{
	char temp[MAX_NUM_CHARS];

	if (db_entry->squintflag) {
		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->alon);
		set_field_buffer(entry->alon, 0, temp);

		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->alat);
		set_field_buffer(entry->alat, 0, temp);
	}

	for (int i=0; i < db_entry->num_transponders; i++) {
		set_field_buffer(entry->transponders[i]->name, 0, db_entry->transponder_name[i]);

		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->uplink_start[i]);
		set_field_buffer(entry->transponders[i]->uplink[0], 0, temp);

		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->uplink_end[i]);
		set_field_buffer(entry->transponders[i]->uplink[1], 0, temp);

		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->downlink_start[i]);
		set_field_buffer(entry->transponders[i]->downlink[0], 0, temp);

		snprintf(temp, MAX_NUM_CHARS, "%f", db_entry->downlink_end[i]);
		set_field_buffer(entry->transponders[i]->downlink[1], 0, temp);
	}
}

//length of fields for squint variable editing
#define SQUINT_LENGTH 10
#define SQUINT_DESCRIPTION_LENGTH 40

//number of fields in one transponder editor line
#define NUM_FIELDS_IN_ENTRY (NUM_TRANSPONDER_SPECIFIERS*2 + 1)

struct transponder_editor* transponder_editor_create(const struct tle_db_entry *sat_info, WINDOW *window, struct sat_db_entry *db_entry)
{
	struct transponder_editor *new_entry = (struct transponder_editor*)malloc(sizeof(struct transponder_editor));
	new_entry->satellite_number = sat_info->satellite_number;

	//create FIELDs for squint angle properties
	int row = 0;
	FIELD *squint_description = new_field(FIELD_HEIGHT, SQUINT_DESCRIPTION_LENGTH, row++, 1, 0, 0);
	set_field_buffer(squint_description, 0, "Alon        Alat");
	field_opts_off(squint_description, O_ACTIVE);
	set_field_back(squint_description, COLOR_PAIR(4)|A_BOLD);

	new_entry->alon = create_transponder_editor_field(FIELD_HEIGHT, SQUINT_LENGTH, row, 1);
	new_entry->alat = create_transponder_editor_field(FIELD_HEIGHT, SQUINT_LENGTH, row++, 1 + SQUINT_LENGTH + SPACING);
	row++;

	//create FIELDs for each editable field
	FIELD *transponder_description = new_field(FIELD_HEIGHT, TRANSPONDER_DESCRIPTION_LENGTH, row++, 1, 0, 0);
	set_field_back(transponder_description, COLOR_PAIR(4)|A_BOLD);
	set_field_buffer(transponder_description, 0, "Transponder name      Uplink      Downlink");
	field_opts_off(transponder_description, O_ACTIVE);
	for (int i=0; i < MAX_NUM_TRANSPONDERS; i++) {
		new_entry->transponders[i] = transponder_editor_line_create(row);
		row += 4;
	}

	transponder_editor_set_visible(new_entry, db_entry->num_transponders+1);

	//create horrible FIELD array for input into the FORM
	FIELD **fields = calloc(NUM_FIELDS_IN_ENTRY*MAX_NUM_TRANSPONDERS + 5, sizeof(FIELD*));
	fields[0] = squint_description;
	fields[1] = new_entry->alon;
	fields[2] = new_entry->alat;
	fields[3] = transponder_description;

	for (int i=0; i < MAX_NUM_TRANSPONDERS; i++) {
		int field_index = i*NUM_FIELDS_IN_ENTRY + 4;
		fields[field_index] = new_entry->transponders[i]->name;
		fields[field_index + 1] = new_entry->transponders[i]->uplink[0];
		fields[field_index + 2] = new_entry->transponders[i]->downlink[0];
		fields[field_index + 3] = new_entry->transponders[i]->uplink[1];
		fields[field_index + 4] = new_entry->transponders[i]->downlink[1];
	}
	fields[NUM_FIELDS_IN_ENTRY*MAX_NUM_TRANSPONDERS + 4] = NULL;
	new_entry->form = new_form(fields);

	//scale input window to the form
	int rows, cols;
	scale_form(new_entry->form, &rows, &cols);
	wresize(window, rows+4, cols+4);
	keypad(window, TRUE);
	wattrset(window, COLOR_PAIR(4));
	box(window, 0, 0);

	//sub window for form
	set_form_win(new_entry->form, window);
	set_form_sub(new_entry->form, derwin(window, rows, cols, 2, 2));

	post_form(new_entry->form);
	refresh();

	form_driver(new_entry->form, REQ_VALIDATION);

	//mark first field with different background color
	new_entry->curr_selected_field = current_field(new_entry->form);
	set_field_back(new_entry->curr_selected_field, TRANSPONDER_ACTIVE_FIELDSTYLE);

	//fill fields with transponder database information
	transponder_editor_fill(new_entry, db_entry);

	//display satellite name on top
	mvwprintw(window, 0, 5, "%s", sat_info->name);

	return new_entry;
}

void transponder_editor_sysdefault(struct transponder_editor *entry, struct sat_db_entry *sat_db_entry)
{
	//create dummy TLE database with a single entry corresponding to the satellite number
	struct tle_db *dummy_tle_db = tle_db_create();
	struct transponder_db *dummy_transponder_db = transponder_db_create();
	struct tle_db_entry dummy_entry;
	dummy_entry.satellite_number = entry->satellite_number;
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
	transponder_editor_fill(entry, &(dummy_transponder_db->sats[0]));

	transponder_editor_set_visible(entry, sat_db_entry->num_transponders+1);

	tle_db_destroy(&dummy_tle_db);
	transponder_db_destroy(&dummy_transponder_db);
}

/**
 * Clear transponder editor of all information.
 *
 * \param entry Transponder editor
 **/
void transponder_editor_clear(struct transponder_editor *entry)
{
	set_field_buffer(entry->alon, 0, "");
	set_field_buffer(entry->alat, 0, "");

	//clear editable lines
	for (int i=0; i < entry->num_editable_transponders-1; i++) {
		transponder_editor_line_clear(entry->transponders[i]);
	}

	//clear last line if it has been edited
	struct transponder_editor_line *last_line = entry->transponders[entry->num_editable_transponders-1];
	if (transponder_editor_line_is_edited(last_line)) {
		transponder_editor_line_clear(last_line);
	}
}

void transponder_editor_handle(struct transponder_editor *transponder_entry, int c)
{
	//handle keyboard
	switch (c) {
		case KEY_UP:
			form_driver(transponder_entry->form, REQ_UP_FIELD);
			break;
		case KEY_DOWN:
			form_driver(transponder_entry->form, REQ_DOWN_FIELD);
			break;
		case KEY_LEFT:
			form_driver(transponder_entry->form, REQ_LEFT_FIELD);
			break;
		case KEY_RIGHT:
			form_driver(transponder_entry->form, REQ_RIGHT_FIELD);
			break;
		case 10:
			form_driver(transponder_entry->form, REQ_NEXT_FIELD);
			break;
		case KEY_BACKSPACE:
			form_driver(transponder_entry->form, REQ_DEL_PREV);
			form_driver(transponder_entry->form, REQ_VALIDATION);
			break;
		case KEY_DC:
			form_driver(transponder_entry->form, REQ_CLR_FIELD);
			break;
		case 23: //CTRL + W
			transponder_editor_clear(transponder_entry);
			break;
		default:
			form_driver(transponder_entry->form, c);
			form_driver(transponder_entry->form, REQ_VALIDATION); //update buffer with field contents
			break;
	}

	//switch background color of currently marked field, reset previous field
	FIELD *curr_field = current_field(transponder_entry->form);
	if (curr_field != transponder_entry->curr_selected_field) {
		set_field_back(transponder_entry->curr_selected_field, TRANSPONDER_ENTRY_DEFAULT_STYLE);
		set_field_back(curr_field, TRANSPONDER_ACTIVE_FIELDSTYLE);
		transponder_entry->curr_selected_field = curr_field;
	}

	//add a new transponder editor field if last entry has been edited
	if ((transponder_entry->num_editable_transponders < MAX_NUM_TRANSPONDERS) && (transponder_editor_line_is_edited(transponder_entry->transponders[transponder_entry->num_editable_transponders-1]))) {
		transponder_editor_set_visible(transponder_entry, transponder_entry->num_editable_transponders+1);
	}
}

void transponder_editor_to_db_entry(struct transponder_editor *entry, struct sat_db_entry *db_entry)
{
	//get squint angle variables
	char *alon_str = strdup(field_buffer(entry->alon, 0));
	trim_whitespaces_from_end(alon_str);
	char *alat_str = strdup(field_buffer(entry->alat, 0));
	trim_whitespaces_from_end(alat_str);
	double alon = strtod(alon_str, NULL);
	double alat = strtod(alat_str, NULL);
	db_entry->squintflag = false;
	if ((strlen(alon_str) > 0) || (strlen(alat_str) > 0)) {
		db_entry->alon = alon;
		db_entry->alat = alat;
		db_entry->squintflag = true;
	}

	int entry_index = 0;
	for (int i=0; i < entry->num_editable_transponders; i++) {
		//get name from transponder entry
		struct transponder_editor_line *line = entry->transponders[i];
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
