#include <stdlib.h>
#include <string.h>
#include "hamlib.h"
#include "ui.h"
#include "defines.h"
#include <form.h>

/**
 * Field type, deciding attributes to use for field returned in field().
 **/
enum field_type {
	///Title field with title styling, unmutable
	TITLE_FIELD,
	///Description field with description/header styling, unmutable
	DESCRIPTION_FIELD,
	///Mutable field, underscore styling
	FREE_ENTRY_FIELD,
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
		case FREE_ENTRY_FIELD:
			field_attributes = FIELDSTYLE_INACTIVE;
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
	///Field for modifying host
	FIELD *host;
	///Field for modifying port
	FIELD *port;
	///Field for modifying tracking horizon
	FIELD *tracking_horizon;
	///Field displaying current connection status (disconnected, connected, connecting)
	FIELD *connection_status;
	///Field displaying current azimuth and elevation read from rotctld
	FIELD *aziele;
	///Array over all rotctld fields
	FIELD **field_array;
	///Form for fields in this struct
	FORM *form;
	///Row index of last row in the form, used for controlling when to jump to the next form
	int last_row;
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

/**
 * Obtain currently set horizon from horizon input field.
 **/
double rotctld_form_horizon(struct rotctld_form *form)
{
	char *horizon_string = strdup(field_buffer(form->tracking_horizon, 0));
	trim_whitespaces_from_end(horizon_string);

	double horizon = strtod(horizon_string, NULL);
	free(horizon_string);
	return horizon;
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
	row += 2;
	col = 0;
	FIELD *host_description = field(DESCRIPTION_FIELD, row, col++, "Host");
	FIELD *port_description = field(DESCRIPTION_FIELD, row, col++, "Port");
	FIELD *tracking_horizon_description = field(DESCRIPTION_FIELD, row, col++, "Horizon");
	FIELD *aziele_description = field(DESCRIPTION_FIELD, row, col++, "Azi   Ele");
	row++;
	col = 0;

	//host and port
	form->host = field(FREE_ENTRY_FIELD, row, col++, rotctld->host);
	form->port = field(FREE_ENTRY_FIELD, row, col++, rotctld->port);

	//tracking horizon
	char tracking_horizon_str[MAX_NUM_CHARS];
	snprintf(tracking_horizon_str, MAX_NUM_CHARS, "%f", rotctld->tracking_horizon);
	form->tracking_horizon = field(FREE_ENTRY_FIELD, row, col++, tracking_horizon_str);

	//azimuth/elevation
	form->aziele = field(DEFAULT_FIELD, row, col++, NULL);

	form->last_row = row;

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

///Style (black on yellow) used for displaying "Connecting..." in connection status field
#define CONNECTING_STYLE COLOR_PAIR(10)

/**
 * Set connection status field to "Connecting..." with given styling.
 *
 * \param field Field
 **/
void set_connection_attempt(FIELD *field)
{
	set_field_buffer(field, 0, "Connecting...");
	set_field_back(field, CONNECTING_STYLE);
}

/**
 * Attempt rotctld reconnection with the current settings in the rotctld settings form.
 *
 * \param form Rotctld settings form
 * \param rotctld Rotctld connection instance to manipulate
 **/
void rotctld_form_attempt_reconnection(struct rotctld_form *form, rotctld_info_t *rotctld)
{
	//get settings from fields
	char *host_field = strdup(field_buffer(form->host, 0));
	trim_whitespaces_from_end(host_field);
	char *port_field = strdup(field_buffer(form->port, 0));
	trim_whitespaces_from_end(port_field);

	//set transient status message
	set_connection_attempt(form->connection_status);
	form_driver(form->form, REQ_VALIDATION);
	wrefresh(form->window);

	//attempt reconnection
	rotctld_disconnect(rotctld);
	rotctld_connect(host_field, port_field, rotctld);
	free(host_field);
	free(port_field);

	//set status message
	set_connection_field(form->connection_status, rotctld->connected);
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

	//obtain currently set horizon setting from field
	double horizon = rotctld_form_horizon(form);
	rotctld_set_tracking_horizon(rotctld, horizon);
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
	///Last row index in form, used for jumping between forms
	int last_row;
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
	row += 2;
	col = 0;
	FIELD *host_description = field(DESCRIPTION_FIELD, row, col++, "Host");
	FIELD *port_description = field(DESCRIPTION_FIELD, row, col++, "Port");
	FIELD *vfo_description = field(DESCRIPTION_FIELD, row, col++, "VFO");
	FIELD *frequency_description = field(DESCRIPTION_FIELD, row, col++, "Frequency");

	//settings fields
	row++;
	col = 0;
	form->host = field(FREE_ENTRY_FIELD, row, col++, rigctld->host);
	form->port = field(FREE_ENTRY_FIELD, row, col++, rigctld->port);
	form->vfo = field(FREE_ENTRY_FIELD, row, col++, rigctld->vfo_name);
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
	form->last_row = row;

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
 * Get currently set VFO name from corresponding field.
 *
 * \param form Rigctld settings form
 * \return VFO name
 **/
char *rigctld_form_vfo(struct rigctld_form *form)
{
	char *vfo_field = strdup(field_buffer(form->vfo, 0));
	trim_whitespaces_from_end(vfo_field);
	return vfo_field;
}

/**
 * Attempt reconnection to rigctld using the current host:port settings in rigctld settings form.
 *
 * \param form rigctld settings form
 * \param rigctld Rigctld connection instance
 **/
void rigctld_form_attempt_reconnection(struct rigctld_form *form, rigctld_info_t *rigctld)
{
	//get current settings from fields
	char *host_field = strdup(field_buffer(form->host, 0));
	trim_whitespaces_from_end(host_field);
	char *port_field = strdup(field_buffer(form->port, 0));
	trim_whitespaces_from_end(port_field);

	//update status field
	set_connection_attempt(form->connection_status);
	form_driver(form->form, REQ_VALIDATION);
	wrefresh(form->window);

	//attempt reconnection
	rigctld_disconnect(rigctld);
	rigctld_connect(host_field, port_field, rigctld);

	//update status field
	set_connection_field(form->connection_status, rigctld->connected);
	free(host_field);
	free(port_field);
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

	//get current VFO from field
	char *vfo = rigctld_form_vfo(form);
	rigctld_set_vfo(rigctld, vfo);
	free(vfo);
}

/**
 * Get row index of field.
 *
 * \param field Field
 * \return Row of field
 **/
int rownumber(FIELD *field)
{
	int rows, cols, frow, fcol, nrow, nbuf;
	field_info(field, &rows, &cols, &frow, &fcol, &nrow, &nbuf);
	return frow;
}

///Spacing between settings form windows
#define VERTICAL_SPACING 4
///Row for settings windows
#define HAMLIB_SETTINGS_WINDOW_ROW 0
///Column for settings window
#define HAMLIB_SETTINGS_WINDOW_COL 0
///Height of rigctld settings windows
#define RIGCTLD_SETTINGS_WINDOW_HEIGHT 6
///Height of rotctld settings windows
#define ROTCTLD_SETTINGS_WINDOW_HEIGHT 6
///Width of settings windows
#define SETTINGS_WINDOW_WIDTH (HAMLIB_SETTINGS_FIELD_WIDTH*4 + 7)
///Spacing between windows
#define WINDOW_SPACING 1
///Number of settings forms
#define NUM_FORMS 3

void hamlib_settings(rotctld_info_t *rotctld, rigctld_info_t *downlink, rigctld_info_t *uplink)
{
	clear();
	refresh();
	halfdelay(HALF_DELAY_TIME);

	//header
	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(0,0,"                                                                                ");
	mvprintw(1,0,"  flyby Hamlib Settings                                                         ");
	mvprintw(2,0,"                                                                                ");

	//usage instructions
	int col = HAMLIB_SETTINGS_WINDOW_COL + SETTINGS_WINDOW_WIDTH + 1;
	WINDOW *description_window = newwin(60, 80 - col, VERTICAL_SPACING, col);
	mvwprintw(description_window, 0, 0, "Navigate using the arrowbuttons.");
	mvwprintw(description_window, 3, 0, "Press ENTER to attempt  reconnection.");
	mvwprintw(description_window, 6, 0, "Press ESC to escape backto previous screen.");
	wrefresh(description_window);

	//windows for each form
	WINDOW *rotctld_window = newwin(ROTCTLD_SETTINGS_WINDOW_HEIGHT, SETTINGS_WINDOW_WIDTH, VERTICAL_SPACING, HAMLIB_SETTINGS_WINDOW_COL);
	WINDOW *downlink_window = newwin(RIGCTLD_SETTINGS_WINDOW_HEIGHT, SETTINGS_WINDOW_WIDTH, VERTICAL_SPACING+ROTCTLD_SETTINGS_WINDOW_HEIGHT+WINDOW_SPACING, HAMLIB_SETTINGS_WINDOW_COL);
	WINDOW *uplink_window = newwin(RIGCTLD_SETTINGS_WINDOW_HEIGHT, SETTINGS_WINDOW_WIDTH, VERTICAL_SPACING+ROTCTLD_SETTINGS_WINDOW_HEIGHT + RIGCTLD_SETTINGS_WINDOW_HEIGHT + 2*WINDOW_SPACING, HAMLIB_SETTINGS_WINDOW_COL);

	//prepare settings forms
	struct rigctld_form *downlink_form = rigctld_form_prepare("Downlink", downlink, downlink_window);
	struct rigctld_form *uplink_form = rigctld_form_prepare("Uplink", uplink, uplink_window);
	struct rotctld_form *rotctld_form = rotctld_form_prepare(rotctld, rotctld_window);

	//convenience arrays for jumping between forms in keyboard input handling
	int curr_form_index = 0;
	FORM *forms[NUM_FORMS] = {rotctld_form->form, downlink_form->form, uplink_form->form};
	int last_rows[NUM_FORMS] = {rotctld_form->last_row, downlink_form->last_row, uplink_form->last_row};

	FIELD *curr_field = current_field(forms[curr_form_index]);
	set_field_back(curr_field, FIELDSTYLE_ACTIVE);

	//event loop
	bool should_run = true;
	while (should_run) {
		FORM *curr_form = forms[curr_form_index];

		//update form properties, get current settings
		rigctld_form_update(downlink, downlink_form);
		rigctld_form_update(uplink, uplink_form);
		rotctld_form_update(rotctld, rotctld_form);

		wrefresh(uplink_window);
		wrefresh(downlink_window);
		wrefresh(rotctld_window);

		//key input handling
		int key = getch();
		switch (key) {
			case KEY_UP:
				form_driver(curr_form, REQ_UP_FIELD);

				//jump to previous form if at start of form
				if (rownumber(current_field(curr_form)) == last_rows[curr_form_index]) {
					curr_form_index--;
					if (curr_form_index < 0) {
						curr_form_index = 0;
					}
				}
				break;
			case KEY_DOWN:
				//jump to next form if at end of current form
				if (rownumber(current_field(curr_form)) == last_rows[curr_form_index]) {
					curr_form_index++;
					if (curr_form_index >= NUM_FORMS) {
						curr_form_index = NUM_FORMS-1;
					}
				} else {
					form_driver(curr_form, REQ_DOWN_FIELD);
				}
				break;
			case KEY_LEFT:
				form_driver(curr_form, REQ_LEFT_FIELD);
				break;
			case KEY_RIGHT:
				form_driver(curr_form, REQ_RIGHT_FIELD);
				break;
			case 10:
				//attempt reconnection to daemon considered in current form
				if (curr_form == rotctld_form->form) {
					rotctld_form_attempt_reconnection(rotctld_form, rotctld);
				} else if (curr_form == downlink_form->form) {
					rigctld_form_attempt_reconnection(downlink_form, downlink);
				} else if (curr_form == uplink_form->form) {
					rigctld_form_attempt_reconnection(uplink_form, uplink);
				}
				break;
			case KEY_BACKSPACE:
				form_driver(curr_form, REQ_DEL_PREV);
				form_driver(curr_form, REQ_VALIDATION);
				break;
			case 27:
				should_run = false;
				break;
			default:
				form_driver(curr_form, key);
				form_driver(curr_form, REQ_VALIDATION);
				break;
		}

		//highlight current active field
		FIELD *curr_field_candidate = current_field(forms[curr_form_index]);
		if (curr_field_candidate != curr_field) {
			set_field_back(curr_field, FIELDSTYLE_INACTIVE);
			set_field_back(curr_field_candidate, FIELDSTYLE_ACTIVE);
			curr_field = curr_field_candidate;
		}
	}

	rigctld_form_free(&downlink_form);
	rigctld_form_free(&uplink_form);
	rotctld_form_free(&rotctld_form);

	delwin(rotctld_window);
	delwin(uplink_window);
	delwin(downlink_window);
}
