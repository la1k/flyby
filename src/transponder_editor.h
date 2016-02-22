#ifndef TRANSPONDER_EDITOR_H_DEFINED
#define TRANSPONDER_EDITOR_H_DEFINED

#include <form.h>
#include "defines.h"
#include "transponder_db.h"

//number of fields needed for defining a transponder entry
#define NUM_TRANSPONDER_SPECIFIERS 2

#define TRANSPONDER_ENTRY_DEFAULT_STYLE COLOR_PAIR(1)|A_UNDERLINE

struct transponder_line {
	FIELD *transponder_name;
	FIELD *uplink[NUM_TRANSPONDER_SPECIFIERS];
	FIELD *downlink[NUM_TRANSPONDER_SPECIFIERS];
	FIELD *phase[NUM_TRANSPONDER_SPECIFIERS];
	FIELD *dayofweek;
};

struct transponder_entry {
	FIELD *alat;
	FIELD *alon;
	struct transponder_line *transponders[MAX_NUM_TRANSPONDERS];
};

struct transponder_entry* transponder_editor_entry_create();

void transponder_editor_entry_fill(struct transponder_entry *entry, struct sat_db_entry *db_entry);

void transponder_db_entry_from_editor(struct sat_db_entry *db_entry, struct transponder_entry *entry);

FORM *transponder_editor_form(struct transponder_entry *transponder_entry);

#endif
