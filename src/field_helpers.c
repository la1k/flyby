#include "field_helpers.h"
#include <stdlib.h>
#include <string.h>
#include "defines.h"

struct prepared_form prepare_form(int num_fields, FIELD **fields, int window_row, int window_col)
{
	//prepare allocated field array
	struct prepared_form form;
	form.num_fields = num_fields;
	form.fields = calloc(num_fields, sizeof(FIELD*));
	memcpy(form.fields, fields, num_fields*sizeof(FIELD*));

	//create form
	form.form = new_form(form.fields);

	//create form window
	int rows, cols;
	scale_form(form.form, &rows, &cols);
	form.window = newwin(rows, cols, window_row, window_col);
	set_form_win(form.form, form.window);
	form.subwindow = derwin(form.window, rows, cols, 0, 0);
	set_form_sub(form.form, form.subwindow);
	post_form(form.form);
	form_driver(form.form, REQ_VALIDATION);

	wrefresh(form.window);

	return form;
}

void prepared_form_free_fields(struct prepared_form *form)
{
	unpost_form(form->form);
	free_field_array(form->fields);
	free_form(form->form);
	delwin(form->subwindow);
	delwin(form->window);
}

void free_field_array(FIELD **fields)
{
	int i=0;
	while (fields[i] != NULL) {
		free_field(fields[i]);
		i++;
	}
}

void prepared_form_add_padding(struct prepared_form *form, struct padding padding)
{
	//resize main window
	int rows, cols;
	getmaxyx(form->window, rows, cols);
	wresize(form->window, rows + padding.top + padding.bottom, cols + padding.left + padding.right);
	getmaxyx(form->window, rows, cols);
	set_form_win(form->form, form->window);

	//resize and move sub window
	wresize(form->subwindow, rows - padding.bottom - padding.top, cols - padding.right - padding.left);
	mvderwin(form->subwindow, padding.top, padding.left);

	//refresh form
	unpost_form(form->form);
	wclear(form->window);
	post_form(form->form);
	form_driver(form->form, REQ_VALIDATION);
}

FIELD *field(enum field_type field_type, int row, int col, const char *content)
{
	int field_width = DEFAULT_FIELD_WIDTH;
	if (content != NULL) {
		field_width = strlen(content);
	}

	FIELD *field = new_field(1, field_width, row, col, 0, 0);

	int field_attributes = 0;
	switch (field_type) {
		case TITLE_FIELD:
			field_attributes = A_BOLD;
			break;
		case DESCRIPTION_FIELD:
			field_attributes = FIELDSTYLE_DESCRIPTION;
			break;
		case VARYING_INFORMATION_FIELD:
			field_attributes = FIELDSTYLE_VARYING_INFORMATION;
			break;
	}
	field_opts_off(field, O_ACTIVE);
	set_field_back(field, field_attributes);

	if (content != NULL) {
		set_field_buffer(field, 0, content);
	}

	return field;
}
