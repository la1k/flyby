#ifndef TRANSPONDER_EDITOR_H_DEFINED
#define TRANSPONDER_EDITOR_H_DEFINED

#include <form.h>
#include "defines.h"
#include "transponder_db.h"

//number of fields needed for defining transponder frequencies
#define NUM_TRANSPONDER_SPECIFIERS 2

/**
 * Editor for single transponder.
 **/
struct transponder_editor_line {
	///Field for editing transponder name
	FIELD *name;
	///Field for editing uplink frequency interval (position 0: Interval start, position 1: interval end)
	FIELD *uplink[NUM_TRANSPONDER_SPECIFIERS];
	///Field for editing downlink frequency interval (position 0: Interval start, position 1: interval end)
	FIELD *downlink[NUM_TRANSPONDER_SPECIFIERS];
};

/**
 * Editor for full transponder database entry.
 **/
struct transponder_editor {
	long satellite_number;
	///Form containing all fields in the transponder editor
	FORM *form;
	///Field for editing attitude latitude for squint angle calculation
	FIELD *alat;
	///Field for editing attitude longitude for squint angle calculation
	FIELD *alon;
	///Number of editable transponder entries
	int num_editable_transponders;
	///Transponder entries
	struct transponder_editor_line *transponders[MAX_NUM_TRANSPONDERS];
	///Currently selected field in form
	FIELD *curr_selected_field;
	///Last selectable field in form
	FIELD *last_field_in_form;
};

/**
 * Create transponder editor.
 *
 * \param sat_info TLE entry, used for getting satellite name and number
 * \param window Window to put the FIELDs in. Will be resized to fit the FORM
 * \param db_entry Database entry
 **/
struct transponder_editor* transponder_editor_create(const struct tle_db_entry *sat_info, WINDOW *window, struct sat_db_entry *db_entry);

void transponder_editor_sysdefault(struct transponder_editor *entry, struct sat_db_entry *sat_db_entry);

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
