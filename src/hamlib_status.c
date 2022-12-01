#include "hamlib_status.h"
#include <stdlib.h>
#include <string.h>
#include "hamlib.h"
#include "ui.h"
#include "defines.h"
#include "field_helpers.h"

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

///Spacing between settings fields
#define FIELD_SPACING 1

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
	///Form for displaying the fields above
	struct prepared_form form;
};

/**
 * Free memory associated with rotctld settings form.
 *
 * \param form Rotctld settings form
 **/
void rotctld_form_free(struct rotctld_form **form)
{
	prepared_form_free_fields(&((*form)->form));
	free(*form);
}

/**
 * Convert column number for hamlib forms to an absolute column coordinate.
 *
 * \param col Column number (0, 1, ...)
 **/
int hamlib_form_col(int col)
{
	return DEFAULT_FIELD_WIDTH*col;
}

///Title displayed on top of rotor form
#define ROTOR_FORM_TITLE "Rotor"

///Number of fields in rotctld form
#define NUM_ROTCTLD_FIELDS 10

/**
 * Create rotctld settings/status form struct.
 *
 * \param rotctld Rotctld connection instance
 * \param window_row Window row
 * \param window_col Window column
 * \return Rotctld settings form
 **/
struct rotctld_form * rotctld_form_prepare(rotctld_info_t *rotctld, int window_row, int window_col)
{
	struct rotctld_form *form = (struct rotctld_form *) malloc(sizeof(struct rotctld_form));

	int row = 0;
	int col = 0;

	//title and connection status
	FIELD *title = field(TITLE_FIELD, row, hamlib_form_col(col++), ROTOR_FORM_TITLE);
	col += 2;
	form->connection_status = field(VARYING_INFORMATION_FIELD, row, hamlib_form_col(col++), NULL);

	//field headers
	row += 1;
	col = 0;
	FIELD *host_description = field(DESCRIPTION_FIELD, row, hamlib_form_col(col++), "Host");
	FIELD *port_description = field(DESCRIPTION_FIELD, row, hamlib_form_col(col++), "Port");
	FIELD *tracking_horizon_description = field(DESCRIPTION_FIELD, row, hamlib_form_col(col++), "Horizon");
	FIELD *aziele_description = field(DESCRIPTION_FIELD, row, hamlib_form_col(col++), "Azi   Ele");
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
	form->host = field(VARYING_INFORMATION_FIELD, row, hamlib_form_col(col++), host_str);
	form->port = field(VARYING_INFORMATION_FIELD, row, hamlib_form_col(col++), port_str);

	//tracking horizon
	char tracking_horizon_str[MAX_NUM_CHARS];
	snprintf(tracking_horizon_str, MAX_NUM_CHARS, "%f", rotctld->tracking_horizon);
	form->tracking_horizon = field(VARYING_INFORMATION_FIELD, row, hamlib_form_col(col++), tracking_horizon_str);

	//azimuth/elevation
	form->aziele = field(VARYING_INFORMATION_FIELD, row, hamlib_form_col(col++), NULL);

	//construct a FORM out of the FIELDs
	FIELD *fields[] = {title, form->connection_status,
		host_description, form->host, port_description, form->port, tracking_horizon_description, form->tracking_horizon, aziele_description, form->aziele, 0};
	form->form = prepare_form(NUM_ROTCTLD_FIELDS, fields, window_row, window_col);

	struct padding padding = {.top = 0, .bottom=1, .left=2, .right=2};
	prepared_form_add_padding(&(form->form), padding);

	//styling
	box(form->form.window, 0, 0);
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
		struct rotctld_read_position pos = {0};

		rotctld_error ret_err = rotctld_read_position(rotctld, &pos);
		if ((ret_err == ROTCTLD_NO_ERR) && (pos.is_set)) {
			snprintf(aziele_string, MAX_NUM_CHARS, "%3.0f   %3.0f", pos.azimuth, pos.elevation);
		}
	}
	set_field_buffer(form->aziele, 0, aziele_string);

	//refresh connection field
	set_connection_field(form->connection_status, rotctld->connected);

	wrefresh(form->form.window);
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
	///Form displaying fields above
	struct prepared_form form;
};

///Number of fields in rigctld form
#define NUM_RIGCTLD_FIELDS 10

/**
 * Prepare rigctld form.
 *
 * \param title_string Title to display on top of rigctld form
 * \param rigctld Rigctld connection instance
 * \param window_row Window row
 * \param window_col Window column
 * \return rigctld form
 **/
struct rigctld_form *rigctld_form_prepare(const char *title_string, rigctld_info_t *rigctld, int window_row, int window_col)
{
	int row = 0;
	int col = 0;

	struct rigctld_form *form = (struct rigctld_form *) malloc(sizeof(struct rigctld_form));

	//title and connection status
	FIELD *title = field(TITLE_FIELD, row, hamlib_form_col(col++), title_string);
	col += 2;
	form->connection_status = field(VARYING_INFORMATION_FIELD, row, hamlib_form_col(col++), NULL);

	//description headers
	row += 1;
	col = 0;
	FIELD *host_description = field(DESCRIPTION_FIELD, row, hamlib_form_col(col++), "Host");
	FIELD *port_description = field(DESCRIPTION_FIELD, row, hamlib_form_col(col++), "Port");
	FIELD *vfo_description = field(DESCRIPTION_FIELD, row, hamlib_form_col(col++), "VFO");
	FIELD *frequency_description = field(DESCRIPTION_FIELD, row, hamlib_form_col(col++), "Frequency");

	//settings fields
	row++;
	col = 0;
	const char *host_str = "N/A";
	const char *port_str = "N/A";
	if (rigctld->connected) {
		host_str = rigctld->host;
		port_str = rigctld->port;
	}
	form->host = field(VARYING_INFORMATION_FIELD, row, hamlib_form_col(col++), host_str);
	form->port = field(VARYING_INFORMATION_FIELD, row, hamlib_form_col(col++), port_str);

	const char *vfo_str = "N/A";
	if (strlen(rigctld->vfo_name) > 0) {
		vfo_str = rigctld->vfo_name;
	}
	form->vfo = field(VARYING_INFORMATION_FIELD, row, hamlib_form_col(col++), vfo_str);
	form->frequency = field(VARYING_INFORMATION_FIELD, row, hamlib_form_col(col++), "N/A");

	//create FORM from FIELDs
	FIELD *fields[NUM_RIGCTLD_FIELDS+1] = {title, form->connection_status, host_description, form->host, port_description, form->port, vfo_description, form->vfo, frequency_description, form->frequency, 0};

	form->form = prepare_form(NUM_RIGCTLD_FIELDS, fields, window_row, window_col);

	struct padding padding = {.top = 0, .bottom=1, .left=2, .right=2};
	prepared_form_add_padding(&(form->form), padding);

	//form styling
	box(form->form.window, 0, 0);
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
	prepared_form_free_fields(&((*form)->form));
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

	wrefresh(form->form.window);
}

void hamlib_status(rotctld_info_t *rotctld, rigctld_info_t *downlink, rigctld_info_t *uplink, enum hamlib_status_background_clearing clear)
{
	halfdelay(HALF_DELAY_TIME);

	//prepare status forms
	int row = HAMLIB_SETTINGS_WINDOW_START_ROW;
	int col = HAMLIB_SETTINGS_WINDOW_COL;
	struct rotctld_form *rotctld_form = rotctld_form_prepare(rotctld, row, col);
	row += ROTCTLD_SETTINGS_WINDOW_HEIGHT + WINDOW_SPACING;
	struct rigctld_form *downlink_form = rigctld_form_prepare("Downlink", downlink, row, col);
	row += ROTCTLD_SETTINGS_WINDOW_HEIGHT + WINDOW_SPACING;
	struct rigctld_form *uplink_form = rigctld_form_prepare("Uplink", uplink, row, col);
	row += ROTCTLD_SETTINGS_WINDOW_HEIGHT + WINDOW_SPACING;

	//clear background
	if (clear == HAMLIB_STATUS_CLEAR_BACKGROUND) {
		for (int i=HAMLIB_SETTINGS_WINDOW_START_ROW-1; i < row+2; i++) {
			move(i, 0);
			clrtoeol();
		}
		refresh();
	}

	while (true) {
		//update with current rig/rotctld status
		rigctld_form_update(downlink, downlink_form);
		rigctld_form_update(uplink, uplink_form);
		rotctld_form_update(rotctld, rotctld_form);

		//key input handling
		int key = getch();
		if (key != ERR) {
			break;
		}
	}

	rigctld_form_free(&downlink_form);
	rigctld_form_free(&uplink_form);
	rotctld_form_free(&rotctld_form);
}
