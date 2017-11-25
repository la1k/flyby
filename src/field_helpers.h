/**
 * Collection of functions and datatypes for making ncurses FORMs and FIELDs more convenient to create.
 **/

#ifndef FIELD_HELPERS_H_DEFINED
#define FIELD_HELPERS_H_DEFINED

#include <form.h>

/**
 * Convenience structure for containing all FORM-related properties created by prepare_form.
 **/
struct prepared_form {
	///Number of fields in form
	int num_fields;
	///Containing window
	WINDOW *window;
	///Subwindow for form
	WINDOW *subwindow;
	///Fields array
	FIELD **fields;
	///Actual form
	FORM *form;
};

/**
 * Create FORM and associated windows from FIELD array. For avoiding some boiler-plate code during form creation.
 *
 * \param num_fields Number of fields
 * \param fields Field array
 * \param window_row Window row
 * \param window_col Window column
 * \param window_padding Window padding
 **/
struct prepared_form prepare_form(int num_fields, FIELD **fields, int window_row, int window_col);

/**
 * Pad specification
 **/
struct padding {
	int top;
	int bottom;
	int left;
	int right;
};

/**
 * Add padding to form windows.
 *
 * \param form Form
 * \param padding Padding
 **/
void prepared_form_add_padding(struct prepared_form *form, struct padding padding);

/**
 * Free fields in form structure.
 *
 * \param form Form structure
 **/
void prepared_form_free_fields(struct prepared_form *form);

/**
 * Free fields in a standard field array.
 *
 * \param fields Field array
 **/
void free_field_array(FIELD **fields);

/**
 * Used for deciding styling of FIELDs.
 **/
enum field_type {
	///Title field with title styling
	TITLE_FIELD,
	///Description field with description/header styling
	DESCRIPTION_FIELD,
	///Field containing information displayed to the user (e.g. az/el coordinates, status messages, ...), unmutable
	VARYING_INFORMATION_FIELD
};

/**
 * Create a field with the given position and attributes.
 *
 * \param field_type Field type (title, description, ...)
 * \param row Row index as absolute window coordinates
 * \param col Column index as absolute window coordinates
 * \param content Initial content to display in field, field length will default to DEFAULT_FIELD_WIDTH if set to NULL
 * \return Created field
 **/
FIELD *field(enum field_type field_type, int row, int col, const char *content);

///Default field width used in field(...)
#define DEFAULT_FIELD_WIDTH 12

///Field style for VARYING_INFORMATION_FIELD
#define FIELDSTYLE_VARYING_INFORMATION COLOR_PAIR(2)|A_BOLD


#endif
