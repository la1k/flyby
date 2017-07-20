#include "prediction_schedules.h"
#include "singletrack.h"
#include "xdg_basedirs.h"
#include "config.h"
#include <math.h>
#include <time.h>
#include <menu.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "filtered_menu.h"
#include "ui.h"
#include "qth_config.h"
#include "transponder_editor.h"
#include "multitrack.h"
#include "locator.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Leftovers from old predict.c-function not sorted elsewhere. Mainly contains run_flyby_curses_ui(), which           //
// handles the multitrack listing and user input choices and calls the correct functions. The file                    //
// also contains functionality like the whitelist editor, the QTH editor, TLE updater and orbital elements displayer. //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void bailout(const char *string)
{
	beep();
	curs_set(1);
	bkgdset(COLOR_PAIR(1));
	clear();
	refresh();
	endwin();
	fprintf(stderr,"*** flyby: %s\n",string);
}

void flyby_banner()
{
	curs_set(0);
	bkgdset(COLOR_PAIR(3));
	clear();
	refresh();

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(2,18,"                                                     ");
	mvprintw(3,18,"                --== flyby v%s ==--                 ",FLYBY_VERSION);
	mvprintw(4,18,"                                                     ");
	mvprintw(5,18,"   based on PREDICT, by John A. Magliacane (KD2BD)   ");
	mvprintw(6,18,"         with mods by John Heaton (G1YYH)            ");
	mvprintw(7,18,"                                                     ");
}

void any_key()
{
	mvprintw(LINES - 2,57,"[Any Key To Continue]");
	refresh();
	getch();
}

void update_tle_database(const char *string, struct tle_db *tle_db)
{
	bool interactive_mode = (string[0] == '\0');
	char filename[MAX_NUM_CHARS] = {0};

	if (interactive_mode) {
		//get filename from user
		curs_set(1);
		bkgdset(COLOR_PAIR(3));
		refresh();
		clear();
		echo();

		attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
		mvprintw(0,0,"                                                                                ");
		mvprintw(1,0,"  flyby Keplerian Database Auto Update                                          ");
		mvprintw(2,0,"                                                                                ");

		attrset(COLOR_PAIR(4)|A_BOLD);
		bkgdset(COLOR_PAIR(2));
		mvprintw(19,18,"Enter NASA Two-Line Element Source File Name");
		mvprintw(13,18,"-=> ");
		refresh();
		wgetnstr(stdscr,filename,49);
		clear();
		curs_set(0);
	} else {
		strncpy(filename, string, MAX_NUM_CHARS);
	}

	//update TLE database with file
	int *update_status = (int*)calloc(tle_db->num_tles, sizeof(int));
	tle_db_update(filename, tle_db, update_status);

	if (interactive_mode) {
		move(12, 0);
	}

	int num_updated = 0;
	bool in_new_file = false;
	bool not_written = false;
	char new_file[MAX_NUM_CHARS] = {0};
	for (int i=0; i < tle_db->num_tles; i++) {
		if (update_status[i] & TLE_DB_UPDATED) {
			//print updated entries
			if (interactive_mode) {
				printw("Updated %s (%ld)", tle_db->tles[i].name, tle_db->tles[i].satellite_number);
			} else {
				printf("Updated %s (%ld)", tle_db->tles[i].name, tle_db->tles[i].satellite_number);
			}
			if (update_status[i] & TLE_IN_NEW_FILE) {
				if (!in_new_file) {
					strncpy(new_file, tle_db->tles[i].filename, MAX_NUM_CHARS);
				}

				in_new_file = true;
				if (interactive_mode) {
					printw(" (*)");
				} else {
					printf(" (*)");
				}
			}
			if (!(update_status[i] & TLE_IN_NEW_FILE) && !(update_status[i] & TLE_FILE_UPDATED)) {
				not_written = true;
				if (interactive_mode) {
					printw(" (X)");
				} else {
					printf(" (X)");
				}
			}
			if (interactive_mode) {
				printw("\n");
			} else {
				printf("\n");
			}
			num_updated++;
		}
	}
	free(update_status);

	//print file information
	if (interactive_mode) {
		if (in_new_file) {
			printw("\nSatellites marked with (*) were put in a new file (%s).\n", new_file);
		}
		if (not_written) {
			printw("\nSatellites marked with (X) were not written to file.");
		}
	} else {
		if (in_new_file) {
			printf("\nSatellites marked with (*) were put in a new file (%s).\n", new_file);
		}
		if (not_written) {
			printf("\nSatellites marked with (X) were not written to file.");
		}
	}

	if (num_updated == 0) {
		if (interactive_mode) {
			printw("No TLE updates/file not found.\n");
		} else {
			printf("No TLE updates/file not found.\n");
		}
	}

	if (interactive_mode) {
		refresh();
		any_key();
	}
}

long date_to_daynumber(int m, int d, int y);

/* This function permits displays a satellite's orbital
 * data.  The age of the satellite data is also provided.
 *
 * \param name Satellite name
 * \param orbital_elements Orbital elements
 **/
void orbital_elements_display(const char *name, predict_orbital_elements_t *orbital_elements)
{
	int c, namelength, age;
	double an_period, no_period, sma, c1, e2, satepoch;
	char days[5];

	if (orbital_elements->mean_motion!=0.0) {
		bkgdset(COLOR_PAIR(2)|A_BOLD);
		clear();
		sma=331.25*exp(log(1440.0/orbital_elements->mean_motion)*(2.0/3.0));
		an_period=1440.0/orbital_elements->mean_motion;
		c1=cos(orbital_elements->inclination*M_PI/180.0);
		e2=1.0-(orbital_elements->eccentricity*orbital_elements->eccentricity);
		no_period=(an_period*360.0)/(360.0+(4.97*pow((EARTH_RADIUS_KM/sma),3.5)*((5.0*c1*c1)-1.0)/(e2*e2))/orbital_elements->mean_motion);
		satepoch=date_to_daynumber(1,0,orbital_elements->epoch_year)+orbital_elements->epoch_day;
		age=(int)rint(predict_to_julian(time(NULL))-satepoch);

		if (age==1)
			strcpy(days,"day");
		else
			strcpy(days,"days");

		namelength=strlen(name);

		printw("\n");

		for (c=41; c>namelength; c-=2)
			printw(" ");

		bkgdset(COLOR_PAIR(3)|A_BOLD);
		attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
		clear();

		mvprintw(0,0,"                                                                                ");
		mvprintw(1,0,"  flyby Orbital Data                                                            ");
		mvprintw(2,0,"                                                                                ");

		mvprintw(1,25,"(%ld) %s", orbital_elements->satellite_number, name);

		attrset(COLOR_PAIR(4)|A_BOLD);
		mvprintw( 4, 4,"Data Issued        : ");
		mvprintw( 5, 4,"Reference Epoch    : ");
		mvprintw( 6, 4,"Inclination        : ");
		mvprintw( 7, 4,"RAAN               : ");
		mvprintw( 8, 4,"Eccentricity       : ");
		mvprintw( 9, 4,"Arg of Perigee     : ");
		mvprintw(10, 4,"Mean Anomaly       : ");
		mvprintw(11, 4,"Mean Motion        : ");
		mvprintw(12, 4,"Decay Rate         : ");
		mvprintw(13, 4,"Nddot/6 Drag       : ");
		mvprintw(14, 4,"Bstar Drag Factor  : ");
		mvprintw(15, 4,"Semi-Major Axis    : ");
		mvprintw(16, 4,"Apogee Altitude    : ");
		mvprintw(17, 4,"Perigee Altitude   : ");
		mvprintw(18, 4,"Anomalistic Period : ");
		mvprintw(19, 4,"Nodal Period       : ");
		mvprintw(20, 4,"Orbit Number       : ");
		mvprintw(21, 4,"Element Set Number : ");

		attrset(COLOR_PAIR(2)|A_BOLD);
		mvprintw( 4,25,"%d %s ago",age,days);
		mvprintw( 5,25,"%02d %.8f",orbital_elements->epoch_year,orbital_elements->epoch_day);
		mvprintw( 6,25,"%.4f deg",orbital_elements->inclination);
		mvprintw( 7,25,"%.4f deg",orbital_elements->right_ascension);
		mvprintw( 8,25,"%g",orbital_elements->eccentricity);
		mvprintw( 9,25,"%.4f deg",orbital_elements->argument_of_perigee);
		mvprintw(10,25,"%.4f deg",orbital_elements->mean_anomaly);
		mvprintw(11,25,"%.8f rev/day",orbital_elements->mean_motion);
		mvprintw(12,25,"%g rev/day/day",orbital_elements->derivative_mean_motion);
		mvprintw(13,25,"%g rev/day/day/day",orbital_elements->second_derivative_mean_motion);
		mvprintw(14,25,"%g 1/earth radii",orbital_elements->bstar_drag_term);
		mvprintw(15,25,"%.4f km",sma);
		mvprintw(16,25,"%.4f km",sma*(1.0+orbital_elements->eccentricity)-EARTH_RADIUS_KM);
		mvprintw(17,25,"%.4f km",sma*(1.0-orbital_elements->eccentricity)-EARTH_RADIUS_KM);
		mvprintw(18,25,"%.4f mins",an_period);
		mvprintw(19,25,"%.4f mins",no_period);
		mvprintw(20,25,"%ld",orbital_elements->revolutions_at_epoch);
		mvprintw(21,25,"%ld",orbital_elements->element_number);

		attrset(COLOR_PAIR(3)|A_BOLD);
		refresh();
		any_key();
	}
}

void qth_editor_update_latlon_fields_from_locator(FIELD *locator, FIELD *longitude, FIELD *latitude)
{
	double longitude_new = 0, latitude_new = 0;
	char *locator_str = strdup(field_buffer(locator, 0));
	trim_whitespaces_from_end(locator_str);
	maidenhead_to_latlon(locator_str, &longitude_new, &latitude_new);

	char temp[MAX_NUM_CHARS];
	snprintf(temp, MAX_NUM_CHARS, "%f", longitude_new);
	set_field_buffer(longitude, 0, temp);
	snprintf(temp, MAX_NUM_CHARS, "%f", latitude_new);
	set_field_buffer(latitude, 0, temp);

	free(locator_str);
}

char *qth_editor_locator_from_latlon(FIELD *longitude, FIELD *latitude)
{
	double curr_longitude = strtod(field_buffer(longitude, 0), NULL);
	double curr_latitude = strtod(field_buffer(latitude, 0), NULL);
	char locator_str[MAX_NUM_CHARS];
	latlon_to_maidenhead(curr_latitude, curr_longitude, locator_str);
	return strdup(locator_str);
}

void qth_editor_update_locator_field_from_latlon(FIELD *longitude, FIELD *latitude, FIELD *locator)
{
	char *locator_str = qth_editor_locator_from_latlon(longitude, latitude);
	set_field_buffer(locator, 0, locator_str);
	free(locator_str);
}

void qth_editor_update_locator_message(FIELD *locator, FIELD *longitude, FIELD *latitude, FIELD *locator_message)
{
	char *locator_str_in_field = strdup(field_buffer(locator, 0));
	trim_whitespaces_from_end(locator_str_in_field);
	char *locator_str_assumed = qth_editor_locator_from_latlon(longitude, latitude);

	if (strcmp(locator_str_in_field, locator_str_assumed) != 0) {
		char locator_message_str[MAX_NUM_CHARS];
		snprintf(locator_message_str, MAX_NUM_CHARS, "(%s assumed)", locator_str_assumed);
		set_field_buffer(locator_message, 0, locator_message_str);
	} else {
		set_field_buffer(locator_message, 0, "");
	}
	free(locator_str_in_field);
	free(locator_str_assumed);
	refresh();
}

#define QTH_ENTRY_FIELD_LENGTH 10
#define QTH_DESCRIPTION_FIELD_LENGTH 20
#define NUM_QTH_FIELDS 9
#define INPUT_NUM_CHARS 128

/**
 * Edit QTH information and save to file.
 *
 * \param qthfile File at which qth information is saved
 * \param qth Returned QTH information
 **/
void qth_editor(const char *qthfile, predict_observer_t *qth)
{
	//display current QTH information
	bkgdset(COLOR_PAIR(3)|A_BOLD);
	clear();

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(0,0,"                                                                                ");
	mvprintw(1,0,"  flyby Ground Station Location                                                 ");
	mvprintw(2,0,"                                                                                ");

	curs_set(1);

	//set up form for QTH editing
	FIELD *fields[NUM_QTH_FIELDS+1] = {
		new_field(1, QTH_ENTRY_FIELD_LENGTH, 0, 0, 0, 0),
		new_field(1, QTH_ENTRY_FIELD_LENGTH, 1, 0, 0, 0),
		new_field(1, QTH_DESCRIPTION_FIELD_LENGTH, 1, QTH_ENTRY_FIELD_LENGTH+1, 0, 0),
		new_field(1, QTH_ENTRY_FIELD_LENGTH, 2, 0, 0, 0),
		new_field(1, QTH_DESCRIPTION_FIELD_LENGTH, 2, QTH_ENTRY_FIELD_LENGTH+1, 0, 0),
		new_field(1, QTH_ENTRY_FIELD_LENGTH, 3, 0, 0, 0),
		new_field(1, QTH_DESCRIPTION_FIELD_LENGTH, 3, QTH_ENTRY_FIELD_LENGTH+1, 0, 0),
		new_field(1, QTH_ENTRY_FIELD_LENGTH, 4, 0, 0, 0),
		new_field(1, QTH_DESCRIPTION_FIELD_LENGTH, 4, QTH_ENTRY_FIELD_LENGTH+1, 0, 0),
		NULL};
	FIELD *name = fields[0];
	FIELD *locator = fields[1];
	FIELD *locator_message = fields[2];
	FIELD *latitude = fields[3];
	FIELD *latitude_description = fields[4];
	FIELD *longitude = fields[5];
	FIELD *longitude_description = fields[6];
	FIELD *altitude = fields[7];
	FIELD *altitude_description = fields[8];
	FORM *form = new_form(fields);
	for (int i=0; i < NUM_QTH_FIELDS; i++) {
		set_field_fore(fields[i], COLOR_PAIR(2)|A_BOLD);
		field_opts_off(fields[i], O_STATIC);
		set_max_field(fields[i], INPUT_NUM_CHARS);
	}

	field_opts_off(locator_message, O_ACTIVE);
	field_opts_off(latitude_description, O_ACTIVE);
	field_opts_off(longitude_description, O_ACTIVE);
	field_opts_off(altitude_description, O_ACTIVE);

	//set up windows
	int win_row = 8;
	int win_col = 40;
	int win_height, win_width;
	scale_form(form, &win_height, &win_width);
	WINDOW *form_win = newwin(win_height, win_width, win_row, win_col);
	set_form_win(form, form_win);
	set_form_sub(form, derwin(form_win, win_height, win_width, 0, 0));
	keypad(form_win, TRUE);
	post_form(form);

	//display headers
	attrset(COLOR_PAIR(4)|A_BOLD);
	int info_col = 20;
	mvprintw(win_row,info_col,"Station Callsign  : ");
	mvprintw(win_row+1,info_col,"Station Locator   : ");
	mvprintw(win_row+2,info_col,"Station Latitude  : ");
	mvprintw(win_row+3,info_col,"Station Longitude : ");
	mvprintw(win_row+4,info_col,"Station Altitude  : ");
	mvprintw(win_row+6,info_col-5," Only decimal notation (e.g. 74.2467) allowed");
	mvprintw(win_row+7,info_col-5," for longitude and latitude.");
	mvprintw(win_row+9,info_col-5," Navigate using keypad or ENTER. Press ENTER");
	mvprintw(win_row+10,info_col-5," on last field or ESC to save and exit.");

	//print units
	attrset(COLOR_PAIR(2)|A_BOLD);
	set_field_buffer(latitude_description, 0, "[DegN]");
	set_field_buffer(longitude_description, 0, "[DegE]");
	set_field_buffer(altitude_description, 0, "[m]");

	//fill form with QTH contents
	set_field_buffer(name, 0, qth->name);
	char temp[MAX_NUM_CHARS] = {0};
	snprintf(temp, MAX_NUM_CHARS, "%f", qth->latitude*180.0/M_PI);
	set_field_buffer(latitude, 0, temp);
	snprintf(temp, MAX_NUM_CHARS, "%f", qth->longitude*180.0/M_PI);
	set_field_buffer(longitude, 0, temp);
	snprintf(temp, MAX_NUM_CHARS, "%f", qth->altitude);
	set_field_buffer(altitude, 0, temp);
	qth_editor_update_locator_field_from_latlon(longitude, latitude, locator);

	refresh();
	wrefresh(form_win);

	//handle input characters to QTH form
	bool run_form = true;
	FIELD *prev_field = current_field(form);
	while (run_form) {
		int key = wgetch(form_win);

		switch (key) {
			case KEY_UP:
				form_driver(form, REQ_UP_FIELD);
				break;
			case KEY_DOWN:
				form_driver(form, REQ_DOWN_FIELD);
				break;
			case KEY_LEFT:
				form_driver(form, REQ_PREV_CHAR);
				break;
			case KEY_RIGHT:
				form_driver(form, REQ_NEXT_CHAR);
				break;
			case 10:
				if (current_field(form) == altitude) {
					run_form = false;
				} else {
					form_driver(form, REQ_NEXT_FIELD);
				}
				break;
			case KEY_BACKSPACE:
				form_driver(form, REQ_DEL_PREV);
				form_driver(form, REQ_VALIDATION);
				if (current_field(form) == locator) {
					qth_editor_update_latlon_fields_from_locator(locator, longitude, latitude);
					qth_editor_update_locator_message(locator, longitude, latitude, locator_message);
				} else {
					qth_editor_update_locator_field_from_latlon(longitude, latitude, locator);
				}
				break;
			case KEY_DC:
				form_driver(form, REQ_DEL_CHAR);
				break;
			case 27:
				run_form = false;
				break;
			default:
				form_driver(form, key);
				form_driver(form, REQ_VALIDATION); //update buffer with field contents
				if (current_field(form) == locator) {
					qth_editor_update_latlon_fields_from_locator(locator, longitude, latitude);
					qth_editor_update_locator_message(locator, longitude, latitude, locator_message);
				} else {
					qth_editor_update_locator_field_from_latlon(longitude, latitude, locator);
				}

				break;
		}
	}

	//copy field contents to predict_observer
	strncpy(qth->name, field_buffer(name, 0), INPUT_NUM_CHARS);
	trim_whitespaces_from_end(qth->name);
	qth->latitude = strtod(field_buffer(latitude, 0), NULL)*M_PI/180.0;
	qth->longitude = strtod(field_buffer(longitude, 0), NULL)*M_PI/180.0;
	qth->altitude = strtod(field_buffer(altitude, 0), NULL);

	//write to file
	qth_to_file(qthfile, qth);
	curs_set(0);

	//free form
	unpost_form(form);
	free_field(name);
	free_field(latitude);
	free_field(longitude);
	free_field(altitude);
	free_field(locator);
	free_form(form);
	delwin(form_win);
}

void trim_whitespaces_from_end(char *string)
{
	//trim whitespaces from end
	for (int i=strlen(string)-1; i >= 0; i--) {
		if (string[i] == ' ') {
			string[i] = '\0';
		} else if (isdigit(string[i]) || isalpha(string[i])) {
			break;
		}
	}
}

void prepare_pattern(char *string)
{
	trim_whitespaces_from_end(string);

	//lowercase to uppercase
	for (int i=0; i < strlen(string); i++) {
		string[i] = toupper(string[i]);
	}
}

//column at which to print keyhints in whitelist
#define WHITELIST_KEYHINT_COL 42

//row at which to print info whether only entries with transponders are displayed
#define WHITELIST_TRANSPONDER_TOGGLE_INFO_ROW 24

void whitelist_editor(struct tle_db *tle_db, const struct transponder_db *transponder_db)
{
	/* Print header */
	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	clear();

	int row = 0;
	mvprintw(row++,0,"                                                                                ");
	mvprintw(row++,0,"  flyby Enable/Disable Satellites                                               ");
	mvprintw(row++,0,"                                                                                ");

	int c;

	WINDOW *my_menu_win;

	int *tle_index = (int*)calloc(tle_db->num_tles, sizeof(int));

	if (tle_db->num_tles > 0) {
		attrset(COLOR_PAIR(3)|A_BOLD);
		mvprintw(LINES-3,46,"%d satellites",tle_db->num_tles);
	}

	/* Create form for query input */
	FIELD *field[2];
	FORM  *form;

	field[0] = new_field(1, 24, 1, 1, 0, 0);
	field[1] = NULL;

	set_field_back(field[0], A_UNDERLINE);
	field_opts_off(field[0], O_AUTOSKIP);

	form = new_form(field);
	int rows, cols;
	scale_form(form, &rows, &cols);

	int form_win_height = rows + 4;
	WINDOW *form_win = newwin(rows + 4, cols + 4, row+1, 3);
	row += form_win_height;
	keypad(form_win, TRUE);
	wattrset(form_win, COLOR_PAIR(4));
	box(form_win, 0, 0);

	/* Set main window and sub window */
	set_form_win(form, form_win);
	WINDOW *subwin = derwin(form_win, rows, cols, 2, 2);
	set_form_sub(form, subwin);

	post_form(form);
	wrefresh(form_win);

	/* Create the window to be associated with the menu */
	int window_width = 35;
	int window_ypos = row;
	my_menu_win = newwin(LINES-window_ypos-1, window_width, window_ypos, 5);

	keypad(my_menu_win, TRUE);
	wattrset(my_menu_win, COLOR_PAIR(4));

	if (tle_db->num_tles > 0) {
		box(my_menu_win, 0, 0);
	}

	if (tle_db->num_tles > 0) {
		/* Print description */
		attrset(COLOR_PAIR(3)|A_BOLD);
		int col = WHITELIST_KEYHINT_COL;
		row = 5;
		mvprintw( 6,col,"Use upper-case characters to ");
		mvprintw( 7,col,"filter satellites by name,");
		mvprintw( 8,col,"satellite number or TLE filename.");


		mvprintw( 10,col,"Use cursor keys to move up/down");
		mvprintw( 11,col,"the list and then disable/enable");
		mvprintw( 12,col,"with        .");

		mvprintw( 14,col,"Press  q  to return to menu or");
		mvprintw( 15,col,"wipe query field if filled.");
		mvprintw( 17,col,"Press  a  to toggle visible entries.");
		mvprintw( 19,col,"Press  w  to wipe query field.");
		mvprintw( 21,col,"Press  t  to enable/disable");
		mvprintw( 22,col,"transponder filter.");
		mvprintw(5, 6, "Filter TLEs by string:");
		row = 18;

		/* Print keyboard bindings in special format */
		attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
		mvprintw( 12,col+5," SPACE ");
		mvprintw( 14,col+6," q ");
		mvprintw( 17,col+6," a ");
		mvprintw( 19,col+6," w ");
		mvprintw( 21,col+6," t ");
	}

	refresh();

	struct filtered_menu menu = {0};
	filtered_menu_from_tle_db(&menu, tle_db, my_menu_win);

	char field_contents[MAX_NUM_CHARS] = {0};

	refresh();
	wrefresh(my_menu_win);
	form_driver(form, REQ_VALIDATION);
	if (tle_db->num_tles > 0) {
		wrefresh(form_win);
	}
	bool run_menu = true;

	while (run_menu) {
		if (tle_db->num_tles == 0) {
			char *data_home = xdg_data_home();
			char *data_dirs = xdg_data_dirs();
			string_array_t data_dirs_list = {0};
			stringsplit(data_dirs, &data_dirs_list);

			int row = 5;
			int col = 10;
			attrset(COLOR_PAIR(1));
			mvprintw(row++, col, "No TLEs found.");
			row++;
			mvprintw(row++, col, "TLE files can be placed in ");
			mvprintw(row++, col, "the following locations:");
			row++;
			mvprintw(row++, col, "* %s%s", data_home, TLE_RELATIVE_DIR_PATH);

			for (int i=0; i < string_array_size(&data_dirs_list); i++) {
				mvprintw(row++, col, "* %s%s", string_array_get(&data_dirs_list, i), TLE_RELATIVE_DIR_PATH);
			}
			row++;
			mvprintw(row++, col, "Files will be loaded on program restart.");

			free(data_home);
			free(data_dirs);
			string_array_free(&data_dirs_list);
			refresh();
		}

		//handle keyboard
		c = wgetch(my_menu_win);
		bool handled = false;

		handled = filtered_menu_handle(&menu, c);

		wrefresh(my_menu_win);

		if (!handled) {
			switch (c) {
				case 't':
					filtered_menu_only_comsats(&menu, !menu.display_only_entries_with_transponders);
					filtered_menu_pattern_match(&menu, tle_db, transponder_db, field_contents);
					if (menu.display_only_entries_with_transponders) {
						attrset(COLOR_PAIR(1));
						int row = WHITELIST_TRANSPONDER_TOGGLE_INFO_ROW;
						mvprintw(row++, WHITELIST_KEYHINT_COL, "Only entries with transponders");
						mvprintw(row, WHITELIST_KEYHINT_COL, "are shown.");
						refresh();
					} else {
						int row = WHITELIST_TRANSPONDER_TOGGLE_INFO_ROW;
						move(row++, WHITELIST_KEYHINT_COL);
						clrtoeol();
						move(row, WHITELIST_KEYHINT_COL);
						clrtoeol();
						refresh();
					}

					break;
				case 'q':
					strncpy(field_contents, field_buffer(field[0], 0), MAX_NUM_CHARS);
					prepare_pattern(field_contents);

					if (strlen(field_contents) > 0) {
						//wipe field if field is non-empty
						c = 'w';
					} else {
						//exit whitelister otherwise
						run_menu = false;
						break;
					}
				case KEY_BACKSPACE:
					form_driver(form, REQ_DEL_PREV);
				default:
					if (isupper(c) || isdigit(c)) {
						form_driver(form, c);
					}
					if (c == 'w') {
						form_driver(form, REQ_CLR_FIELD);
					}

					form_driver(form, REQ_VALIDATION); //update buffer with field contents

					strncpy(field_contents, field_buffer(field[0], 0), MAX_NUM_CHARS);
					prepare_pattern(field_contents);

					filtered_menu_pattern_match(&menu, tle_db, transponder_db, field_contents);

					wrefresh(form_win);
					break;
			}
		}
	}

	filtered_menu_to_tle_db(&menu, tle_db);
	filtered_menu_free(&menu);

	whitelist_write_to_default(tle_db);

	unpost_form(form);

	free(tle_index);
	free_form(form);

	delwin(subwin);
	delwin(my_menu_win);
	delwin(form_win);

	free_field(field[0]);
}

#define MAIN_MENU_BACKGROUND_STYLE COLOR_PAIR(4)|A_REVERSE
int print_main_menu_option(WINDOW *window, int row, int col, char key, const char *description)
{
	wattrset(window, COLOR_PAIR(1));
	mvwprintw(window, row,col,"%c", key);
	wattrset(window, MAIN_MENU_BACKGROUND_STYLE);
	mvwprintw(window, row,col+1,"%s", description);
	return col + 1 + strlen(description);
}

/**
 * Print global main menu options to specified window. Display format is inspired by htop. :-)
 *
 * \param window Window for printing
 **/
void print_main_menu(WINDOW *window)
{
	int row = 0;
	int column = 0;

	column = print_main_menu_option(window, row, column, 'W', "Enable/Disable Satellites");
	column = print_main_menu_option(window, row, column, 'G', "Edit Ground Station      ");
	column = print_main_menu_option(window, row, column, 'E', "Edit Transponder Database");
	column = 0;
	row++;
	column = print_main_menu_option(window, row, column, 'I', "Program Information      ");
	column = print_main_menu_option(window, row, column, 'O', "Solar Pass Predictions   ");
	column = print_main_menu_option(window, row, column, 'N', "Lunar Pass Predictions   ");
	column = 0;
	row++;
	column = print_main_menu_option(window, row, column, 'U', "Update Sat Elements      ");
	column = print_main_menu_option(window, row, column, 'M', "Multitrack settings      ");
	column = print_main_menu_option(window, row, column, 'Q', "Exit flyby               ");

	wrefresh(window);
}

/**
 * Print rigctld info to terminal.
 *
 * \param name Name used for printing
 * \param rigctld Rigctl info.
 **/
void print_rigctld_info(const char *name, rigctld_info_t *rigctld)
{
	if (rigctld->connected) {
		printw("\n");
		printw("\t\t%s VFO\t: Enabled\n", name);
		printw("\t\t - Connected to rigctld: %s:%s\n", rigctld->host, rigctld->port);

		printw("\t\t - VFO name: ");
		if (strlen(rigctld->vfo_name) > 0) {
			printw("%s\n", rigctld->vfo_name);
		} else {
			printw("Undefined\n");
		}
	} else {
		printw("\t\t%s VFO\t: Not enabled\n", name);
	}
}

/**
 * Display program information.
 **/
void general_program_info(const char *qthfile, struct tle_db *tle_db, struct transponder_db *transponder_db, rotctld_info_t *rotctld, rigctld_info_t* downlink, rigctld_info_t *uplink)
{
	flyby_banner();
	attrset(COLOR_PAIR(3)|A_BOLD);

	printw("\n\n\n\n\n\t\tflyby version : %s\n",FLYBY_VERSION);
	printw("\t\tQTH file        : %s\n", qthfile);
	printw("\t\tTLE file        : ");
	if (tle_db->num_tles > 0) {
		string_array_t tle_db_files = tle_db_filenames(tle_db);
		printw("%d TLEs loaded from %d files\n", tle_db->num_tles, string_array_size(&tle_db_files));
		string_array_free(&tle_db_files);
	} else {
		printw("Not loaded\n");
	}
	printw("\t\tDatabase file   : ");
	if (transponder_db->loaded) {
		printw("Loaded\n");
	} else {
		printw("Not loaded\n");
	}

	if (rotctld->connected) {
		printw("\n");
		printw("\t\tAutoTracking    : Enabled\n");
		printw("\t\t - Connected to rotctld: %s:%s\n", rotctld->host, rotctld->port);

		printw("\t\t - Tracking horizon: %.2f degrees. ", rotctld->tracking_horizon);

		if (rotctld->update_time_interval > 0)
			printw("Update every %d seconds", rotctld->update_time_interval);

		printw("\n");
	} else {
		printw("\t\tAutoTracking    : Not enabled\n");
	}

	print_rigctld_info("Uplink", uplink);
	print_rigctld_info("Downlink", downlink);

	refresh();
	attrset(COLOR_PAIR(4)|A_BOLD);
	any_key();
}

void print_sun_box(int row, int col, predict_observer_t *qth, predict_julian_date_t daynum)
{
	struct predict_observation sun;
	predict_observe_sun(qth, daynum, &sun);

	attrset(COLOR_PAIR(4)|A_REVERSE|A_BOLD);
	mvprintw(row++,col,"   Sun   ");
	if (sun.elevation > 0.0)
		attrset(COLOR_PAIR(3)|A_BOLD);
	else
		attrset(COLOR_PAIR(2));
	mvprintw(row++,col,"%-7.2fAz",sun.azimuth*180.0/M_PI);
	mvprintw(row++,col,"%+-6.2f El",sun.elevation*180.0/M_PI);
}

void print_moon_box(int row, int col, predict_observer_t *qth, predict_julian_date_t daynum)
{
	struct predict_observation moon;
	predict_observe_moon(qth, daynum, &moon);

	attrset(COLOR_PAIR(4)|A_REVERSE|A_BOLD);
	mvprintw(row++,col,"   Moon  ");
	attrset(COLOR_PAIR(3)|A_BOLD);
	if (moon.elevation > 0.0)
		attrset(COLOR_PAIR(1)|A_BOLD);
	else
		attrset(COLOR_PAIR(1));
	mvprintw(row++,col,"%-7.2fAz",moon.azimuth*180.0/M_PI);
	mvprintw(row++,col,"%+-6.2f El",moon.elevation*180.0/M_PI);
}

void print_qth_box(int row, int col, predict_observer_t *qth)
{
	attrset(COLOR_PAIR(4)|A_REVERSE|A_BOLD);
	mvprintw(row++,col,"   QTH   ");
	attrset(COLOR_PAIR(2));
	mvprintw(row++,col,"%9s",qth->name);
	char maidenstr[9];
	latlon_to_maidenhead(qth->latitude*180.0/M_PI, qth->longitude*180.0/M_PI, maidenstr);
	mvprintw(row++,col,"%9s",maidenstr);
}

void run_flyby_curses_ui(bool new_user, const char *qthfile, predict_observer_t *observer, struct tle_db *tle_db, struct transponder_db *sat_db, rotctld_info_t *rotctld, rigctld_info_t *downlink, rigctld_info_t *uplink)
{
	/* Start ncurses */
	initscr();
	keypad(stdscr, TRUE);
	start_color();
	cbreak();
	noecho();
	scrollok(stdscr,TRUE);
	curs_set(0);

	init_pair(1,COLOR_WHITE,COLOR_BLACK);
	init_pair(2,COLOR_YELLOW,COLOR_BLACK);
	init_pair(3,COLOR_GREEN,COLOR_BLACK);
	init_pair(4,COLOR_CYAN,COLOR_BLACK);
	init_pair(5,COLOR_WHITE,COLOR_RED);
	init_pair(6,COLOR_RED,COLOR_WHITE);
	init_pair(7,COLOR_CYAN,COLOR_RED);
	init_pair(8,COLOR_RED,COLOR_YELLOW);

	if (new_user) {
		qth_editor(qthfile, observer);
		clear();
	}

	predict_julian_date_t curr_time = predict_to_julian(time(NULL));

	//prepare multitrack window
	multitrack_listing_t *listing = multitrack_create_listing(observer, tle_db);

	//window for printing main menu options
	WINDOW *main_menu_win = newwin(MAIN_MENU_OPTS_WIN_HEIGHT, COLS, LINES-MAIN_MENU_OPTS_WIN_HEIGHT, 0);

	refresh();

	/* Display main menu and handle keyboard input */
	int key = 0;
	bool should_run = true;
	int terminal_lines = LINES;
	int terminal_columns = COLS;
	while (should_run) {
		if ((terminal_lines != LINES) || (terminal_columns != COLS)) {
			//force full redraw
			clear();

			//update main menu option window
			mvwin(main_menu_win, LINES-MAIN_MENU_OPTS_WIN_HEIGHT, 0);
			wresize(main_menu_win, MAIN_MENU_OPTS_WIN_HEIGHT, COLS);
			wrefresh(main_menu_win);

			terminal_lines = LINES;
			terminal_columns = COLS;
		}

		curr_time = predict_to_julian(time(NULL));

		//refresh satellite list
		multitrack_update_listing_data(listing, curr_time);
		multitrack_display_listing(listing);

		if (!multitrack_search_field_visible(listing->search_field)) {
			print_main_menu(main_menu_win);
		}
		print_sun_box(listing->window_height + listing->window_row - 7, listing->window_width+1, observer, curr_time);
		print_moon_box(listing->window_height + listing->window_row - 7 + 4, listing->window_width+1, observer, curr_time);
		print_qth_box(listing->window_row, listing->window_width+1, observer);

		//get input character
		refresh();
		halfdelay(HALF_DELAY_TIME);  // Increase if CPU load is too high
		key = getch();
		if (key != -1) {
			cbreak(); //turn off halfdelay

			//handle input to satellite list
			bool handled = multitrack_handle_listing(listing, key);

			//option in submenu has been selected, run satellite specific options
			if (multitrack_option_selector_pop(listing->option_selector)) {
				int option = multitrack_option_selector_get_option(listing->option_selector);
				int satellite_index = multitrack_selected_entry(listing);
				predict_orbital_elements_t *orbital_elements = tle_db_entry_to_orbital_elements(tle_db, satellite_index);
				const char *sat_name = tle_db->tles[satellite_index].name;
				switch (option) {
					case OPTION_SINGLETRACK:
						singletrack(satellite_index, observer, sat_db, tle_db, rotctld, downlink, uplink);
						break;
					case OPTION_PREDICT_VISIBLE:
						satellite_pass_display_schedule(sat_name, orbital_elements, observer, 'v');
						break;
					case OPTION_PREDICT:
						satellite_pass_display_schedule(sat_name, orbital_elements, observer, 'p');
						break;
					case OPTION_DISPLAY_ORBITAL_DATA:
						orbital_elements_display(sat_name, orbital_elements);
						break;
					case OPTION_EDIT_TRANSPONDER:
						transponder_database_editor(satellite_index, tle_db, sat_db);
						break;
					case OPTION_SOLAR_ILLUMINATION:
						solar_illumination_display_predictions(sat_name, orbital_elements);
						break;
				}
				predict_destroy_orbital_elements(orbital_elements);
				clear();
				refresh();
			}

			//handle all other, global options
			if (!handled) {
				if (!handled) {
					switch (key) {
						case 'N':
						case 'n':
							sun_moon_pass_display_schedule(PREDICT_MOON, observer);
							break;

						case 'O':
						case 'o':
							sun_moon_pass_display_schedule(PREDICT_SUN, observer);
							break;

						case 'U':
						case 'u':
							update_tle_database("", tle_db);
							break;

						case 'M':
						case 'm':
							multitrack_edit_settings(listing);
							break;

						case 'H':
						case 'h':
							multitrack_show_help();
							break;

						case 'G':
						case 'g':
							qth_editor(qthfile, observer);
							multitrack_refresh_tles(listing, tle_db);
							break;

						case 'I':
						case 'i':
							general_program_info(qthfile, tle_db, sat_db, rotctld, downlink, uplink);
							break;

						case 'w':
						case 'W':
							whitelist_editor(tle_db, sat_db);
							multitrack_refresh_tles(listing, tle_db);
							break;
						case 'E':
						case 'e':
							transponder_database_editor(0, tle_db, sat_db);
							break;
						case 27:
						case 'q':
							should_run = false;
							break;
					}
					clear();
					refresh();
				}
			}
		}
	}

	curs_set(1);
	bkgdset(COLOR_PAIR(1));
	clear();
	refresh();
	endwin();

	delwin(main_menu_win);
	multitrack_destroy_listing(&listing);
}
