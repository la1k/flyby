#ifndef TRANSPONDER_EDITOR_H_DEFINED
#define TRANSPONDER_EDITOR_H_DEFINED

#include <form.h>
#include "defines.h"
#include "transponder_db.h"

//number of fields needed for defining a transponder entry
#define NUM_TRANSPONDER_SPECIFIERS 2

#define TRANSPONDER_ENTRY_DEFAULT_STYLE COLOR_PAIR(1)|A_UNDERLINE
#define TRANSPONDER_ACTIVE_FIELDSTYLE COLOR_PAIR(5)

struct transponder_line {
	FIELD *transponder_name;
	FIELD *uplink[NUM_TRANSPONDER_SPECIFIERS];
	FIELD *downlink[NUM_TRANSPONDER_SPECIFIERS];
};

struct transponder_entry {
	FORM *form;
	FIELD *alat;
	FIELD *alon;

	int num_editable_transponders;
	struct transponder_line *transponders[MAX_NUM_TRANSPONDERS];

	int num_displayed_transponders;

	FIELD *prev_selected_field;
	FIELD *last_field;
};

struct transponder_entry* transponder_editor_entry_create();

void transponder_db_entry_from_editor(struct sat_db_entry *db_entry, struct transponder_entry *entry);

void transponder_entry_handle(struct transponder_entry *transponder_entry, int c);

#endif
