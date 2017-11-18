#include <stdlib.h>
#include <string.h>
#include "hamlib.h"
#include "ui.h"
#include "defines.h"
#include <form.h>

//Start row for settings window
#define HAMLIB_SETTINGS_WINDOW_START_ROW 5

///Column for settings window
#define HAMLIB_SETTINGS_WINDOW_COL 3

///Height of rigctld settings windows
#define RIGCTLD_SETTINGS_WINDOW_HEIGHT 4

///Height of rotctld settings windows
#define ROTCTLD_SETTINGS_WINDOW_HEIGHT 4

///Width of settings windows
#define SETTINGS_WINDOW_WIDTH (HAMLIB_SETTINGS_FIELD_WIDTH*4 + 7)

///Spacing between windows
#define WINDOW_SPACING 0

/**
 * Field type, deciding attributes to use for field returned in field().
 **/
enum field_type {
	///Title field with title styling, unmutable
	TITLE_FIELD,
	///Description field with description/header styling, unmutable
	DESCRIPTION_FIELD,
	///Unmutable field, normal styling
	DEFAULT_FIELD
};

///Spacing between settings fields
#define FIELD_SPACING 1

//Width of settings fields
#define HAMLIB_SETTINGS_FIELD_WIDTH 12

//Height of settings fields
#define HAMLIB_SETTINGS_FIELD_HEIGHT 1

/**
 * Create a field with the given position and attributes.
 *
 * \param field_type Field type (title, description, ...)
 * \param row Row index
 * \param col Column index
 * \param content Initial content to display in field
 * \return Created field
 **/
FIELD *field(enum field_type field_type, int row, int col, const char *content)
{
	int field_width = HAMLIB_SETTINGS_FIELD_WIDTH;
	if (field_type == TITLE_FIELD) {
		field_width = strlen(content);
	}

	FIELD *field = new_field(HAMLIB_SETTINGS_FIELD_HEIGHT, field_width, row, col*(HAMLIB_SETTINGS_FIELD_WIDTH + FIELD_SPACING), 0, 0);

	int field_attributes = 0;
	switch (field_type) {
		case TITLE_FIELD:
			field_attributes = A_BOLD;
			field_opts_off(field, O_ACTIVE);
			break;
		case DESCRIPTION_FIELD:
			field_attributes = FIELDSTYLE_DESCRIPTION;
			field_opts_off(field, O_ACTIVE);
			break;
		default:
			field_opts_off(field, O_ACTIVE);
			break;
	}
	set_field_back(field, field_attributes);

	if (content != NULL) {
		set_field_buffer(field, 0, content);
	}

	return field;
}

/**
 * Rotctld settings/status form.
 **/
struct rotctld_form {
	///Field for displaying host
	FIELD *host;
	///Field for displaying port
	FIELD *port;
	///Field for displaying tracking horizon
	FIELD *tracking_horizon;
	///Field displaying current connection status (disconnected, connected)
	FIELD *connection_status;
	///Field displaying current azimuth and elevation read from rotctld
	FIELD *aziele;
	///Array over all rotctld fields
	FIELD **field_array;
	///Form for fields in this struct
	FORM *form;
	///Window used for drawing the form
	WINDOW *window;
};

/**
 * Free fields in a standard field array.
 *
 * \param fields Field array
 **/
void free_field_array(FIELD **fields)
{
	int i=0;
	while (fields[i] != NULL) {
		free_field(fields[i]);
		i++;
	}
}

/**
 * Free memory associated with rotctld settings form.
 *
 * \param form Rotctld settings form
 **/
void rotctld_form_free(struct rotctld_form **form)
{
	free_field_array((*form)->field_array);
	free_form((*form)->form);
	free(*form);
}

///Title displayed on top of rotor form
#define ROTOR_FORM_TITLE "Rotor"

///Number of fields in rotctld form
#define NUM_ROTCTLD_FIELDS 10

/**
 * Create rotctld settings/status form struct.
 *
 * \param rotctld Rotctld connection instance
 * \param window Window in which to draw the form
 * \return Rotctld settings form
 **/
struct rotctld_form * rotctld_form_prepare(rotctld_info_t *rotctld, WINDOW *window)
{
	struct rotctld_form *form = (struct rotctld_form *) malloc(sizeof(struct rotctld_form));
	form->window = window;

	int row = 0;
	int col = 0;

	//title and connection status
	FIELD *title = field(TITLE_FIELD, row, col++, ROTOR_FORM_TITLE);
	col += 2;
	form->connection_status = field(DEFAULT_FIELD, row, col++, NULL);

	//field headers
	row += 1;
	col = 0;
	FIELD *host_description = field(DESCRIPTION_FIELD, row, col++, "Host");
	FIELD *port_description = field(DESCRIPTION_FIELD, row, col++, "Port");
	FIELD *tracking_horizon_description = field(DESCRIPTION_FIELD, row, col++, "Horizon");
	FIELD *aziele_description = field(DESCRIPTION_FIELD, row, col++, "Azi   Ele");
	row++;
	col = 0;

	//host and port
	const char *host_str, *port_str;
	if (rotctld->connected) {
		host_str = rotctld->host;
		port_str = rotctld->port;
	} else {
		host_str = "N/A";
		port_str = "N/A";
	}
	form->host = field(DEFAULT_FIELD, row, col++, host_str);
	form->port = field(DEFAULT_FIELD, row, col++, port_str);

	//tracking horizon
	char tracking_horizon_str[MAX_NUM_CHARS];
	snprintf(tracking_horizon_str, MAX_NUM_CHARS, "%f", rotctld->tracking_horizon);
	form->tracking_horizon = field(DEFAULT_FIELD, row, col++, tracking_horizon_str);

	//azimuth/elevation
	form->aziele = field(DEFAULT_FIELD, row, col++, NULL);

	//construct a FORM out of the FIELDs
	form->field_array = calloc(NUM_ROTCTLD_FIELDS+1, sizeof(FIELD*));
	FIELD *fields[NUM_ROTCTLD_FIELDS+1] = {title, form->connection_status,
		host_description, form->host, port_description, form->port, tracking_horizon_description, form->tracking_horizon, aziele_description, form->aziele, 0};
	memcpy(form->field_array, fields, sizeof(FIELD*)*NUM_ROTCTLD_FIELDS);
	form->form = new_form(form->field_array);

	//set form window
	int rows, cols;
	scale_form(form->form, &rows, &cols);
	set_form_win(form->form, window);
	set_form_sub(form->form, derwin(window, rows, cols, 0, 2));
	post_form(form->form);
	form_driver(form->form, REQ_VALIDATION);

	//styling
	box(window, 0, 0);
	set_field_buffer(title, 0, ROTOR_FORM_TITLE);

	return form;
}

///Style (black on green) used for displaying "Connected" in connection status field
#define CONNECTED_STYLE COLOR_PAIR(9)

///Style (white on red) used for displaying "Disconnected" in connection status field
#define DISCONNECTED_STYLE COLOR_PAIR(5)

/**
 * Set connection status field to either "Connected" or "Disconnected" with given styling.
 *
 * \param field Field
 * \param connected Whether connected or not
 **/
void set_connection_field(FIELD *field, bool connected)
{
	if (connected) {
		set_field_buffer(field, 0, "Connected");
		set_field_back(field, CONNECTED_STYLE);
	} else {
		set_field_buffer(field, 0, "Disconnected");
		set_field_back(field, DISCONNECTED_STYLE);
	}
}

/**
 * Update rotctld settings from rotctld form, and update status displayed
 * in rotctld form from information read from the rotctld connection instance.
 *
 * \param rotctld Rotctld connection instance
 * \param form Rotctld settings/status form
 **/
void rotctld_form_update(rotctld_info_t *rotctld, struct rotctld_form *form)
{
	//read current azimuth/elevation from rotctld and display in field
	char aziele_string[MAX_NUM_CHARS] = "N/A   N/A";
	if (rotctld->connected) {
		float azimuth = 0, elevation = 0;
		rotctld_error ret_err = rotctld_read_position(rotctld, &azimuth, &elevation);
		if (ret_err == ROTCTLD_NO_ERR) {
			snprintf(aziele_string, MAX_NUM_CHARS, "%3.0f   %3.0f", azimuth, elevation);
		}
	}
	set_field_buffer(form->aziele, 0, aziele_string);

	//refresh connection field
	set_connection_field(form->connection_status, rotctld->connected);
}

/**
 * Settings/status form for rigctld connection.
 **/
struct rigctld_form {
	///Current connection status
	FIELD *connection_status;
	///Host settings field
	FIELD *host;
	///Port settings field
	FIELD *port;
	///VFO settings field
	FIELD *vfo;
	///Current frequency
	FIELD *frequency;
	///Field array containing all displayed fields
	FIELD **field_array;
	///Form displaying fields in the struct
	FORM *form;
	///Window used for drawing the form
	WINDOW *window;
};

///Number of fields in rigctld form
#define NUM_RIGCTLD_FIELDS 10

/**
 * Prepare rigctld form.
 *
 * \param title_string Title to display on top of rigctld form
 * \param rigctld Rigctld connection instance
 * \param window Window for drawing
 * \return rigctld form
 **/
struct rigctld_form *rigctld_form_prepare(const char *title_string, rigctld_info_t *rigctld, WINDOW *window)
{
	int row = 0;
	int col = 0;

	struct rigctld_form *form = (struct rigctld_form *) malloc(sizeof(struct rigctld_form));
	form->window = window;

	//title and connection status
	FIELD *title = field(TITLE_FIELD, row, col++, title_string);
	col += 2;
	form->connection_status = field(DEFAULT_FIELD, row, col++, "N/A");

	//description headers
	row += 1;
	col = 0;
	FIELD *host_description = field(DESCRIPTION_FIELD, row, col++, "Host");
	FIELD *port_description = field(DESCRIPTION_FIELD, row, col++, "Port");
	FIELD *vfo_description = field(DESCRIPTION_FIELD, row, col++, "VFO");
	FIELD *frequency_description = field(DESCRIPTION_FIELD, row, col++, "Frequency");

	//settings fields
	row++;
	col = 0;
	const char *host_str = "N/A";
	const char *port_str = "N/A";
	if (rigctld->connected) {
		host_str = rigctld->host;
		port_str = rigctld->port;
	}
	form->host = field(DEFAULT_FIELD, row, col++, host_str);
	form->port = field(DEFAULT_FIELD, row, col++, port_str);

	const char *vfo_str = "N/A";
	if (strlen(rigctld->vfo_name) > 0) {
		vfo_str = rigctld->vfo_name;
	}
	form->vfo = field(DEFAULT_FIELD, row, col++, vfo_str);
	form->frequency = field(DEFAULT_FIELD, row, col++, "N/A");

	//create FORM from FIELDs
	form->field_array = calloc(NUM_RIGCTLD_FIELDS+1, sizeof(FIELD*));
	FIELD *fields[NUM_RIGCTLD_FIELDS+1] = {title, form->connection_status, host_description, form->host, port_description, form->port, vfo_description, form->vfo, frequency_description, form->frequency, 0};
	memcpy(form->field_array, fields, sizeof(FIELD*)*NUM_RIGCTLD_FIELDS);
	form->form = new_form(form->field_array);

	//set form window
	int rows, cols;
	scale_form(form->form, &rows, &cols);
	set_form_win(form->form, window);
	set_form_sub(form->form, derwin(window, rows, cols, 0, 2));
	post_form(form->form);
	form_driver(form->form, REQ_VALIDATION);

	//form styling
	box(window, 0, 0);
	set_field_buffer(title, 0, title_string);

	return form;
}

/**
 * Free memory associated with rigctld settings form.
 *
 * \param form Rigctld form
 **/
void rigctld_form_free(struct rigctld_form **form)
{
	free_field_array((*form)->field_array);
	free_form((*form)->form);
	free(*form);
}

/**
 * Update rigctld connection instance with settings read from rigctld settings form,
 * and update rigctld form status messages according to properties read from the
 * rigctld connection instance.
 *
 * \param rigctld Rigctld connection instance
 * \param form Rigctld settings form
 **/
void rigctld_form_update(rigctld_info_t *rigctld, struct rigctld_form *form)
{
	//get current frequency from rigctld
	char frequency_string[MAX_NUM_CHARS] = "N/A";
	if (rigctld->connected) {
		double frequency;
		rigctld_error ret_err = rigctld_read_frequency(rigctld, &frequency);
		if (ret_err == RIGCTLD_NO_ERR) {
			snprintf(frequency_string, MAX_NUM_CHARS, "%.3f MHz\n", frequency);
		}
	}
	set_field_buffer(form->frequency, 0, frequency_string);

	//update connection status field
	set_connection_field(form->connection_status, rigctld->connected);
}

void hamlib_status(rotctld_info_t *rotctld, rigctld_info_t *downlink, rigctld_info_t *uplink)
{
	halfdelay(HALF_DELAY_TIME);

	//windows for each form
	int row = HAMLIB_SETTINGS_WINDOW_START_ROW;
	WINDOW *rotctld_window = newwin(ROTCTLD_SETTINGS_WINDOW_HEIGHT, SETTINGS_WINDOW_WIDTH, row, HAMLIB_SETTINGS_WINDOW_COL);
	row += ROTCTLD_SETTINGS_WINDOW_HEIGHT + WINDOW_SPACING;
	WINDOW *downlink_window = newwin(RIGCTLD_SETTINGS_WINDOW_HEIGHT, SETTINGS_WINDOW_WIDTH, row, HAMLIB_SETTINGS_WINDOW_COL);
	row += ROTCTLD_SETTINGS_WINDOW_HEIGHT + WINDOW_SPACING;
	WINDOW *uplink_window = newwin(RIGCTLD_SETTINGS_WINDOW_HEIGHT, SETTINGS_WINDOW_WIDTH, row, HAMLIB_SETTINGS_WINDOW_COL);

	//prepare status forms
	struct rigctld_form *downlink_form = rigctld_form_prepare("Downlink", downlink, downlink_window);
	struct rigctld_form *uplink_form = rigctld_form_prepare("Uplink", uplink, uplink_window);
	struct rotctld_form *rotctld_form = rotctld_form_prepare(rotctld, rotctld_window);

	while (true) {
		//update with current rig/rotctld status
		rigctld_form_update(downlink, downlink_form);
		rigctld_form_update(uplink, uplink_form);
		rotctld_form_update(rotctld, rotctld_form);

		wrefresh(uplink_window);
		wrefresh(downlink_window);
		wrefresh(rotctld_window);

		//key input handling
		int key = getch();
		if (key != ERR) {
			break;
		}
	}

	rigctld_form_free(&downlink_form);
	rigctld_form_free(&uplink_form);
	rotctld_form_free(&rotctld_form);

	delwin(rotctld_window);
	delwin(uplink_window);
	delwin(downlink_window);
}
