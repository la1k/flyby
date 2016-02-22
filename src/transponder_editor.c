#include <stdlib.h>
#include "transponder_editor.h"


#define TRANSPONDER_FREQUENCY_LENGTH 10
#define TRANSPONDER_NAME_LENGTH 30
#define TRANSPONDER_PHASE_LENGTH 10
#define TRANSPONDER_DOW_LENGTH 10

#define NUM_FIELDS_IN_ENTRY (NUM_TRANSPONDER_SPECIFIERS*3 + 2)

#define FIELD_HEIGHT 1
#define SPACING 2

FIELD *create_field(int field_height, int field_width, int field_row, int field_col) {
	FIELD *ret_field = new_field(field_height, field_width, field_row, field_col, 0, 0);
	set_field_back(ret_field, TRANSPONDER_ENTRY_DEFAULT_STYLE);
	return ret_field;
}

struct transponder_line* transponder_editor_line_create(int row)
{
	struct transponder_line *ret_line = (struct transponder_line*)malloc(sizeof(struct transponder_line));

	ret_line->transponder_name = create_field(FIELD_HEIGHT, TRANSPONDER_NAME_LENGTH, row, 1);

	ret_line->uplink[0] = create_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row, TRANSPONDER_NAME_LENGTH+1+SPACING);
	ret_line->uplink[1] = create_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row+2, TRANSPONDER_NAME_LENGTH+1+SPACING);

	ret_line->downlink[0] = create_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row, TRANSPONDER_NAME_LENGTH+TRANSPONDER_FREQUENCY_LENGTH+1+2*SPACING);
	ret_line->downlink[1] = create_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row+2, TRANSPONDER_NAME_LENGTH+TRANSPONDER_FREQUENCY_LENGTH+1+2*SPACING);

	ret_line->phase[0] = create_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row, TRANSPONDER_NAME_LENGTH+TRANSPONDER_FREQUENCY_LENGTH*2+1+3*SPACING);
	ret_line->phase[1] = create_field(FIELD_HEIGHT, TRANSPONDER_FREQUENCY_LENGTH, row+2, TRANSPONDER_NAME_LENGTH+TRANSPONDER_FREQUENCY_LENGTH*2+1+3*SPACING);

	ret_line->dayofweek = create_field(FIELD_HEIGHT, TRANSPONDER_DOW_LENGTH, row, TRANSPONDER_NAME_LENGTH+TRANSPONDER_FREQUENCY_LENGTH*2+TRANSPONDER_PHASE_LENGTH+4*SPACING);

	return ret_line;
}

struct transponder_entry* transponder_editor_entry_create()
{
	struct transponder_entry *new_entry = (struct transponder_entry*)malloc(sizeof(struct transponder_entry));

	//create FIELDs for each editable field
	int row = 1;
	for (int i=0; i < MAX_NUM_TRANSPONDERS; i++) {
		new_entry->transponders[i] = transponder_editor_line_create(row);
		row += 4;
	}
	return new_entry;
}

FORM *transponder_editor_form(struct transponder_entry *transponder_entry)
{
	//create horrible FIELD array for input into the form
	FIELD **fields = calloc(NUM_FIELDS_IN_ENTRY*MAX_NUM_TRANSPONDERS + 1, sizeof(FIELD*));
	for (int i=0; i < MAX_NUM_TRANSPONDERS; i++) {
		int field_index = i*NUM_FIELDS_IN_ENTRY;
		fields[field_index] = transponder_entry->transponders[i]->transponder_name;
		fields[field_index + 1] = transponder_entry->transponders[i]->uplink[0];
		fields[field_index + 2] = transponder_entry->transponders[i]->downlink[0];
		fields[field_index + 3] = transponder_entry->transponders[i]->phase[0];
		fields[field_index + 4] = transponder_entry->transponders[i]->dayofweek;
		fields[field_index + 5] = transponder_entry->transponders[i]->uplink[1];
		fields[field_index + 6] = transponder_entry->transponders[i]->downlink[1];
		fields[field_index + 7] = transponder_entry->transponders[i]->phase[1];
	}
	fields[NUM_FIELDS_IN_ENTRY*MAX_NUM_TRANSPONDERS] = NULL;

	FORM *form = new_form(fields);
	return form;
}

void transponder_editor_entry_clear(struct transponder_entry *entry) {

}

void transponder_editor_entry_fill(struct transponder_entry *entry, struct sat_db_entry *db_entry)
{
	for (int i=0; i < db_entry->num_transponders; i++) {
		set_field_buffer(entry->transponders[i]->transponder_name, 0, db_entry->transponder_name[i]);
	}
}

void transponder_db_entry_from_editor(struct sat_db_entry *db_entry, struct transponder_entry *entry)
{
}
