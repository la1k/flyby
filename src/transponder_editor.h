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

/**
 * Create transponder editor.
 *
 * \param satellite_name Name of satellite
 * \param window Window to put the FIELDs in. Will be resized to fit the FORM
 * \param db_entry Database entry
 **/
struct transponder_editor* transponder_editor_create(const char *satellite_name, WINDOW *window, struct sat_db_entry *db_entry);

/**
 * Handle input character to transponder editor.
 *
 * \param transponder_entry Transponder editor
 * \param c Input character
 **/
void transponder_editor_handle(struct transponder_editor *transponder_entry, int c);

/**
 * Convert information in transponder editor fields to database fields
 *
 * \param entry Transponder editor
 * \param db_entry Database entry
 **/
void transponder_editor_to_db_entry(struct transponder_editor *entry, struct sat_db_entry *db_entry);

#endif
