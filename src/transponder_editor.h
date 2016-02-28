#ifndef TRANSPONDER_EDITOR_H_DEFINED
#define TRANSPONDER_EDITOR_H_DEFINED

#include <form.h>
#include "defines.h"
#include "transponder_db.h"

//number of fields needed for defining a transponder entry
#define NUM_TRANSPONDER_SPECIFIERS 2

struct transponder_editor_line {
	FIELD *transponder_name;
	FIELD *uplink[NUM_TRANSPONDER_SPECIFIERS];
	FIELD *downlink[NUM_TRANSPONDER_SPECIFIERS];
};

struct transponder_editor {
	FORM *form;
	FIELD *alat;
	FIELD *alon;

	int num_editable_transponders;
	struct transponder_editor_line *transponders[MAX_NUM_TRANSPONDERS];

	int num_displayed_transponders;

	FIELD *prev_selected_field;
	FIELD *last_field;
};

#endif
