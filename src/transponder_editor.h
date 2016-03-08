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
	///Satellite number of the satellite to be edited
	long satellite_number;
	///Form containing all fields in the transponder editor
	FORM *form;
	///Field array used in the form. Contains pointers to the FIELD entries defined below and in struct transponder_editor_line.
	FIELD **field_list;
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
 * Create transponder editor.
 *
 * \param sat_info TLE entry, used for getting satellite name and number
 * \param window Window to put the FIELDs in. Will be resized to fit the FORM
 * \param db_entry Database entry
 **/
struct transponder_editor* transponder_editor_create(const struct tle_db_entry *sat_info, WINDOW *window, struct sat_db_entry *db_entry);

/**
 * Restore satellite transponder entry to the system default defined in XDG_DATA_DIRS.
 *
 * \param transponder_editor Transponder editor, in which the fields are restored to the system default
 * \param sat_db_entry Satellite database entry to restore to system default
 **/
void transponder_editor_sysdefault(struct transponder_editor *transponder_editor, struct sat_db_entry *sat_db_entry);

/**
 * Destroy transponder editor.
 *
 * \param transponder_editor Transponder editor to free
 **/
void transponder_editor_destroy(struct transponder_editor **transponder_editor);

/**
 * Handle input character to transponder editor.
 *
 * \param transponder_editor Transponder editor
 * \param c Input character
 **/
void transponder_editor_handle(struct transponder_editor *transponder_editor, int c);

/**
 * Convert information in transponder editor fields to database fields
 *
 * \param transponder_editor Transponder editor
 * \param db_entry Database entry
 **/
void transponder_editor_to_db_entry(struct transponder_editor *transponder_editor, struct sat_db_entry *db_entry);

#endif
