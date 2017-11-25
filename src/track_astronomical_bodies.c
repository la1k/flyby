#include <curses.h>
#include <ctype.h>
#include "field_helpers.h"

#include "defines.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "xdg_basedirs.h"

#include "singletrack.h"
#include "track_astronomical_bodies.h"

/**
 * Get name of astronomical body as string.
 *
 * \param type Type of body
 * \return Name of body, e.g "Sun" for PREDICT_SUN
 **/
const char *astronomical_body_to_name(enum astronomical_body type)
{
	switch(type) {
		case PREDICT_SUN:
			return "Sun";
		case PREDICT_MOON:
			return "Moon";
		default:
			return "Unknown body";
	}
}

/**
 * Add a line to a window.
 **/
void add_line_to_window(WINDOW *window, int line_attribute)
{
	int window_height = getmaxy(window);
	wattrset(window, line_attribute);
	wmove(window, 0, 0);
	wvline(window, 0, window_height);
	wrefresh(window);
}

void observe_astronomical_body(enum astronomical_body type, predict_observer_t *qth, predict_julian_date_t day, struct predict_observation *observation)
{
	switch (type) {
		case PREDICT_SUN:
			predict_observe_sun(qth, day, observation);
			break;

		case PREDICT_MOON:
			predict_observe_moon(qth, day, observation);
			break;

		default:
			observation->azimuth = NAN;
			observation->elevation = NAN;
			break;
	}
}

/**
 * Form structure for displaying astronomical body properties.
 **/
struct astronomical_body_form {
	///Type of astronomical body
	enum astronomical_body type;
	///Displayed name
	FIELD *name;
	///Displayed azimuth
	FIELD *azimuth;
	///Displayed elevation
	FIELD *elevation;
	///Field after azimuth providing description 'Az'
	FIELD *azi_desc;
	///Field after elevation providing description 'El'
	FIELD *ele_desc;
	///Form containing the fields above
	struct prepared_form form;
	///Predicted relative coordinates of astronomical body
	struct predict_observation observation;
};

/**
 * Update astronomical body form structure with the currently observed properties of the astronomical body.
 *
 * \param astronomical_body_form Form for displaying properties
 * \param qth Ground station
 * \param day Time for prediction
 **/
void astronomical_body_form_update(struct astronomical_body_form *astronomical_body_form, predict_observer_t *qth, predict_julian_date_t day)
{
	//calculate coordinates according to type of astronomical body
	observe_astronomical_body(astronomical_body_form->type, qth, day, &astronomical_body_form->observation);

	//update fields with the given coordinates
	char temp_string[MAX_NUM_CHARS];
	snprintf(temp_string, MAX_NUM_CHARS, "%-7.2f", astronomical_body_form->observation.azimuth*180.0/M_PI);
	set_field_buffer(astronomical_body_form->azimuth, 0, temp_string);
	snprintf(temp_string, MAX_NUM_CHARS, "%+-6.2f", astronomical_body_form->observation.elevation*180.0/M_PI);
	set_field_buffer(astronomical_body_form->elevation, 0, temp_string);

	//default box styling
	wattrset(astronomical_body_form->form.window, COLOR_PAIR(0));
	box(astronomical_body_form->form.window, 0, 0);
	wrefresh(astronomical_body_form->form.window);
}

///Red/black border for selected window
#define ATTRIBUTES_SELECTED_WINDOW COLOR_PAIR(11)
///Black/red border for tracked window
#define ATTRIBUTES_TRACKED_WINDOW COLOR_PAIR(11)|A_REVERSE

/**
 * Mark form window with red color in order to indicate that the cursor is residing here.
 *
 * \param form Form that will be marked
 **/
void astronomical_body_mark_as_selected(struct astronomical_body_form *form)
{
	wattrset(form->form.window, ATTRIBUTES_SELECTED_WINDOW);
	box(form->form.window, 0, 0);
	wrefresh(form->form.window);
}

/**
 * Mark form window with red line to the right in order to indicate that this astronomical body currently is being tracked.
 *
 * \param form Form that will be marked
 **/
void astronomical_body_mark_as_tracked(struct astronomical_body_form *form)
{
	add_line_to_window(form->form.window, ATTRIBUTES_TRACKED_WINDOW);
}

///Padding from start of name of object until azimuth/elevation fields
#define OBJECT_NAME_PADDING 10

///Assumed length of azel field + padding
#define AZEL_LENGTH 8

///Column for body displayers
#define ASTRONOMICAL_BODY_COLUMN 5

/**
 * Create form structure for displaying astronomical object properties.
 *
 * \param window_row Row at which to place the containing window
 * \param type Type of astronomical body
 * \return Form
 **/
struct astronomical_body_form *astronomical_body_form_create(int window_row, enum astronomical_body type)
{
	struct astronomical_body_form *astronomical_body_form = (struct astronomical_body_form*)malloc(sizeof(struct astronomical_body_form));
	astronomical_body_form->type = type;

	int col = 0;
	int row = 0;

	//name field
	const char *name = astronomical_body_to_name(type);
	astronomical_body_form->name = field(TITLE_FIELD, row, col, name);
	col = OBJECT_NAME_PADDING;

	//azimuth/elevation fields
	astronomical_body_form->azimuth = field(VARYING_INFORMATION_FIELD, row++, col, "       ");
	astronomical_body_form->elevation = field(VARYING_INFORMATION_FIELD, row, col, "       ");

	//azimuth/elevation description fields
	col += AZEL_LENGTH;
	astronomical_body_form->azi_desc = field(DESCRIPTION_FIELD, --row, col, "Az ");
	astronomical_body_form->ele_desc = field(DESCRIPTION_FIELD, ++row, col, "El ");

	//field array for form creation
	FIELD *fields[] = {astronomical_body_form->name, astronomical_body_form->azimuth, astronomical_body_form->elevation, astronomical_body_form->azi_desc, astronomical_body_form->ele_desc, 0};
	int num_fields = sizeof(fields)/sizeof(FIELD*);
	astronomical_body_form->form = prepare_form(num_fields, fields, window_row, ASTRONOMICAL_BODY_COLUMN);

	struct padding padding = {.top = 1, .bottom=1, .left=1, .right=1};
	prepared_form_add_padding(&(astronomical_body_form->form), padding);

	return astronomical_body_form;
}

/**
 * Free memory associated with astronomical body displayer.
 *
 * \param form Body form
 **/
void astronomical_body_form_free(struct astronomical_body_form **form)
{
	prepared_form_free_fields(&((*form)->form));
	free(*form);
}

/**
 * Fields for displaying tracking information.
 **/
struct tracking_info {
	///Form
	struct prepared_form form;
	///Title field
	FIELD *title;
	///Current status message
	FIELD *status_message;
	///Whether an object currently is tracked
	bool tracking;
};

/**
 * Create string consisting of n spaces.
 *
 * \param num_spaces
 * \return String consisting of num_spaces number of spaces. Free after use.
 **/
char *spaces(int num_spaces)
{
	char *ret_spaces = malloc((num_spaces+1)*sizeof(char));
	memset(ret_spaces, (int)(' '), num_spaces);
	ret_spaces[num_spaces] = '\0';
	return ret_spaces;
}

///Length of status field
#define STATUS_FIELD_LENGTH 20

/**
 * Create a form for displaying tracking info.
 *
 * \param row Row at which to place the form
 * \param col Column at which to place the form
 **/
struct tracking_info *tracking_info_create(int window_row, int window_col)
{
	struct tracking_info *info = (struct tracking_info*)malloc(sizeof(struct tracking_info));
	info->tracking = false;

	int col = 0;
	int row = 0;

	const char *title = "rotctld tracking";
	info->title = field(TITLE_FIELD, row++, col, title);

	char *filler = spaces(STATUS_FIELD_LENGTH);
	info->status_message = field(VARYING_INFORMATION_FIELD, row++, col, filler);
	free(filler);

	FIELD *fields[] = {info->title, info->status_message, 0};
	info->form = prepare_form(3, fields, window_row, window_col);

	struct padding padding = {.top = 0, .bottom=0, .left=1, .right=0};
	prepared_form_add_padding(&(info->form), padding);

	return info;
}

/**
 * Update status message in tracking info window.
 *
 * \param info Info box
 * \param obs Current observation that is being tracked
 * \param rotctld Rotctld instance
 * \param do_tracking Whether user has selected that tracking should be done
 **/
void tracking_info_update(struct tracking_info *info, const struct predict_observation *obs, rotctld_info_t *rotctld, bool do_tracking)
{
	if (rotctld->connected) {
		if (!do_tracking) {
			set_field_buffer(info->status_message, 0, "Waiting for user");
		} else if (obs->elevation>=rotctld->tracking_horizon) {
			char active[STATUS_FIELD_LENGTH+1];
			snprintf(active, STATUS_FIELD_LENGTH, "Active (%d, %d)", (int)round(rotctld->prev_cmd_azimuth), (int)round(rotctld->prev_cmd_elevation));
			set_field_buffer(info->status_message, 0, active);
		} else {
			set_field_buffer(info->status_message, 0, "Standing by");
		}
	} else {
		set_field_buffer(info->status_message, 0, "Not connected");
	}

	if (do_tracking && rotctld->connected) {
		//add line to the left of same styling as tracking marker in the body selector
		info->tracking = do_tracking;
		add_line_to_window(info->form.window, ATTRIBUTES_TRACKED_WINDOW);
	} else if (!do_tracking && info->tracking) {
		//erase border styling
		unpost_form(info->form.form);
		werase(info->form.window);
		post_form(info->form.form);
		info->tracking = false;
	}

	wrefresh(info->form.window);
}

/**
 * Free memory associated with the tracking info form.
 *
 * \param form Form
 **/
void tracking_info_free(struct tracking_info **form)
{
	prepared_form_free_fields(&((*form)->form));
	free(*form);
}

///Row at which to place first astronomical body form
#define FORM_START_ROW 5

///Spacing between each astronomical body start
#define FORM_SPACING 4

///Height of main menu in astronomical body tracking UI
#define ASTRONOMICAL_BODY_MENU_HEIGHT 1

///Row for printing rotctld status messages
#define ROTCTLD_STATUS_ROW FORM_START_ROW

//Column for printing rotctld status messages
#define ROTCTLD_STATUS_COL 60

/**
 * Print menu at bottom of the terminal.
 *
 * \param window Window in which to print the menu
 **/
void track_astronomical_body_menu(WINDOW *window)
{
	int row = 0;
	int column = 0;

	column = print_main_menu_option(window, row, column, "Up/Down", "Move cursor  ");
	column = print_main_menu_option(window, row, column, "Space/Enter", "Select body for tracking  ");
	column = print_main_menu_option(window, row, column, "Q", "Return                ");

	wrefresh(window);
}

///Settings file field for tracking status
#define SETTINGS_DO_TRACKING "do_tracking = "

///Settings file field for tracked body index
#define SETTINGS_TRACKED_INDEX "tracked_index = "

/**
 * Save current status of tracking to file. Extend to a struct if we ever extend the number of parameters.
 *
 * \param tracked_body Index of tracked body
 * \param do_tracking Whether we are doing tracking
 **/
void track_astronomical_body_settings_to_file(int tracked_body, bool do_tracking)
{
	char *writepath = settings_filepath(TRACK_ASTRONOMICAL_BODY_SETTINGS_FILE);

	FILE *write_file = fopen(writepath, "w");
	if (write_file != NULL) {
		fprintf(write_file, "%s%d\n", SETTINGS_DO_TRACKING, do_tracking);
		fprintf(write_file, "%s%d\n", SETTINGS_TRACKED_INDEX, tracked_body);
		fclose(write_file);
	}

	free(writepath);
}

/**
 * Load current status of tracking from file. If index of tracked body is outside valid range, default to no tracking.
 *
 * \param tracked_body Returned index of tracked body
 * \param do_tracking Returned boolean specifying whether are currently tracking
 * \param num_bodies Number of bodies, 0 <= tracked_body < num_bodies
 **/
void track_astronomical_body_settings_from_file(int *tracked_body, bool *do_tracking, int num_bodies)
{
	char *filepath = settings_filepath(TRACK_ASTRONOMICAL_BODY_SETTINGS_FILE);
	FILE *read_file = fopen(filepath, "r");

	if (read_file != NULL) {
		char line[MAX_NUM_CHARS];

		//do_tracking field
		int tracking;
		fgets(line, MAX_NUM_CHARS, read_file);
		sscanf(line, SETTINGS_DO_TRACKING "%d", &tracking);
		*do_tracking = (tracking == 1);

		//tracked_body field
		fgets(line, MAX_NUM_CHARS, read_file);
		sscanf(line, SETTINGS_TRACKED_INDEX "%d", tracked_body);

		fclose(read_file);
	}

	//input check
	if ((*tracked_body >= num_bodies) || (*tracked_body < 0)) {
		*tracked_body = 0;
		do_tracking = false;
	}

	free(filepath);
}

///Attributes for header on top
#define HEADER_ATTRIBUTES COLOR_PAIR(6)|A_REVERSE|A_BOLD

void track_astronomical_body(predict_observer_t *qth, rotctld_info_t *rotctld)
{
	clear();
	refresh();

	//prepare astronomical_body_form coordinate displayers
	struct astronomical_body_form *astronomical_bodies[NUM_ASTRONOMICAL_BODIES];
	for (int i=0; i < NUM_ASTRONOMICAL_BODIES; i++) {
		astronomical_bodies[i] = astronomical_body_form_create(FORM_START_ROW + FORM_SPACING*i, i);
	}

	halfdelay(HALF_DELAY_TIME);

	//print window header
	attrset(HEADER_ATTRIBUTES);
	mvprintw(0,0,"                                                                                ");
	mvprintw(1,0,"  flyby Tracking:                                                               ");
	mvprintw(2,0,"                                                                                ");
	mvprintw(1,21,"Astronomical bodies");

	//rotctld status box
	struct tracking_info *tracking_info = tracking_info_create(ROTCTLD_STATUS_ROW, ROTCTLD_STATUS_COL);

	//astronomical body currently selected using the cursor
	int selected_astronomical_body = 0;

	//astronomical body currently being tracked when do_tracking is true
	int tracked_astronomical_body = 0;

	//whether an astronomical body is tracked
	bool do_tracking = false;

	//load settings from file
	track_astronomical_body_settings_from_file(&tracked_astronomical_body, &do_tracking, NUM_ASTRONOMICAL_BODIES);

	//main menu window
	WINDOW *main_menu_win = newwin(ASTRONOMICAL_BODY_MENU_HEIGHT, COLS, LINES-ASTRONOMICAL_BODY_MENU_HEIGHT, 0);
	track_astronomical_body_menu(main_menu_win);
	refresh();

	bool should_run = true;
	while (should_run) {
		time_t epoch = time(NULL);
		predict_julian_date_t daynum = predict_to_julian(epoch);

		//display current time in header
		char time_string[MAX_NUM_CHARS];
		attrset(HEADER_ATTRIBUTES);
		strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %j.%H:%M:%S", gmtime(&epoch));
		mvprintw(1,54,"%s",time_string);

		//refresh body coordinates
		for (int i=0; i < NUM_ASTRONOMICAL_BODIES; i++) {
			astronomical_body_form_update(astronomical_bodies[i], qth, daynum);
		}

		//mark window selected with cursor
		astronomical_body_mark_as_selected(astronomical_bodies[selected_astronomical_body]);

		//mark window selected for tracking
		if (do_tracking) {
			astronomical_body_mark_as_tracked(astronomical_bodies[tracked_astronomical_body]);
		}

		//display rotation information
		struct predict_observation obs = astronomical_bodies[tracked_astronomical_body]->observation;
		tracking_info_update(tracking_info, &obs, rotctld, do_tracking);

		//send data to rotctld
		if ((obs.elevation*180.0/M_PI >= rotctld->tracking_horizon) && rotctld->connected && do_tracking) {
			rotctld_fail_on_errors(rotctld_track(rotctld, obs.azimuth*180.0/M_PI, obs.elevation*180.0/M_PI));
		}

		//handle keyboard input
		int input_key=getch();
		switch (tolower(input_key)) {
			//navigation
			case KEY_UP:
				selected_astronomical_body--;
				break;
			case KEY_DOWN:
				selected_astronomical_body++;
				break;

			//quit
			case 'q':
			case 'Q':
			case 27:
				should_run = false;
				break;

			//select object for tracking
			case 10:
			case ' ':
				if ((tracked_astronomical_body == selected_astronomical_body) && do_tracking) {
					do_tracking = false;
				} else {
					do_tracking = true;
					tracked_astronomical_body = selected_astronomical_body;
				}
				break;

		}
		if (selected_astronomical_body >= NUM_ASTRONOMICAL_BODIES) selected_astronomical_body = NUM_ASTRONOMICAL_BODIES-1;
		if (selected_astronomical_body < 0) selected_astronomical_body = 0;
	}

	//cleanup
	tracking_info_free(&tracking_info);
	for (int i=0; i < NUM_ASTRONOMICAL_BODIES; i++) {
		astronomical_body_form_free(&astronomical_bodies[i]);
	}
	delwin(main_menu_win);

	//save current state to file
	track_astronomical_body_settings_to_file(tracked_astronomical_body, do_tracking);
}
