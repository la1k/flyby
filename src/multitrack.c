#include <predict/predict.h>
#include "defines.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <curses.h>
#include <stdlib.h>
#include "tle_db.h"
#include "multitrack.h"
#include "ui.h"

//header (Satellite Azim Elev ...) color style
#define HEADER_STYLE COLOR_PAIR(2)|A_REVERSE

//color attributes of selected entry in satellite listing
#define MULTITRACK_SELECTED_ATTRIBUTE (COLOR_PAIR(6)|A_REVERSE)

//marker of menu item
#define MULTITRACK_SELECTED_MARKER '-'

/** Private multitrack satellite listing prototypes. **/

/**
 * Create entry in multitrack satellite listing.
 *
 * \param name Satellite name
 * \param orbital_elements Orbital elements of satellite, created from TLE
 * \return Multitrack entry
 **/
multitrack_entry_t *multitrack_create_entry(const char *name, predict_orbital_elements_t *orbital_elements);

/**
 * Print scrollbar for satellite listing.
 *
 * \param listing Satellite listing
 **/
void multitrack_print_scrollbar(multitrack_listing_t *listing);

/**
 * Display entry in satellite listing.
 *
 * \param window Window to display entry in
 * \param row Row
 * \param col Column
 * \param entry Satellite entry
 **/
void multitrack_display_entry(WINDOW *window, int row, int col, multitrack_entry_t *entry);

/**
 * Update display strings and status in satellite entry.
 *
 * \param qth QTH coordinates
 * \param entry Multitrack entry
 * \param time Time at which satellite status should be calculated
 **/
void multitrack_update_entry(predict_observer_t *qth, multitrack_entry_t *entry, predict_julian_date_t time);

/**
 * Sort satellite listing in different categories: Currently above horizon, below horizon but will rise, will never rise above horizon, decayed satellites. The satellites below the horizon are sorted internally according to AOS times.
 *
 * \param listing Satellite listing
 **/
void multitrack_sort_listing(multitrack_listing_t *listing);

/**
 * Apply search information in the search field, and construct match array for matches found in the satellite list.
 * Match state is saved to listing->search_field.
 *
 * \param listing Satellite list
 **/
void multitrack_search_listing(multitrack_listing_t *listing);

/**
 * Jump to next search match (obtained from listing->search_field).
 *
 * \param listing Satellite list
 **/
void multitrack_listing_next_match(multitrack_listing_t *listing);

/** Private option submenu selector prototypes. **/

/**
 * Create option selector submenu.
 *
 * \return Option selector
 **/
multitrack_option_selector_t* multitrack_option_selector_create();

/**
 * Destroy option selector.
 *
 * \param option_selector Option selector to free
 **/
void multitrack_option_selector_destroy(multitrack_option_selector_t **option_selector);

/**
 * Hide option selector.
 *
 * \param option_selector Option selector
 **/
void multitrack_option_selector_hide(multitrack_option_selector_t *option_selector);

/**
 * Show option selector.
 *
 * \param option_selector Option selector
 **/
void multitrack_option_selector_show(multitrack_option_selector_t *option_selector);

/**
 * Check whether option selector is visible.
 *
 * \return True if option selector is visible, false otherwise
 **/
bool multitrack_option_selector_visible(multitrack_option_selector_t *option_selector);

/**
 * Resize option selector windows in order to retain sizes during terminal resize.
 *
 * \param option_selector Option selector
 **/
void multitrack_option_selector_resize(multitrack_option_selector_t *option_selector);

/**
 * Display option selector on specified row number in the standard screen.
 *
 * \param row Row to place submenu window
 * \param option_selector Option selector
 **/
void multitrack_option_selector_display(int row, multitrack_option_selector_t *option_selector);

/**
 * Jump to specified option and signal that an option has been selected, regardless of whether
 * the option selector is visible or not. Used for shortcuts like 'T', 'P', ... from the main menu.
 *
 * \param option_selector Option selector
 * \param option Option to select
 **/
void multitrack_option_selector_jump(multitrack_option_selector_t *option_selector, enum sub_menu_options option);

/**
 * Handle input characters to option selector. KEY_UP/KEY_DOWN scrolls menu, KEY_LEFT/q/ESC hides the menu,
 * while ENTER/KEY_RIGHT also sets the `option_selected` flag in addition to hiding the menu.
 *
 * \param option_selector Option selector
 * \param input_key Input key
 * \return True if input key was handled, false otherwise
 **/
bool multitrack_option_selector_handle(multitrack_option_selector_t *option_selector, int input_key);

/**
 * Get string corresponding to each submenu option.
 *
 * \param option Submenu option
 * \return String corresponding to submenu option
 **/
const char *multitrack_option_selector_name(enum sub_menu_options option);

/** Private search field prototypes. **/

/**
 * Create search field.
 *
 * \param row_offset_from_bottom Offset from bottom of terminal
 * \param col Column
 * \return Search field
 **/
multitrack_search_field_t *multitrack_search_field_create(int row_offset_from_bottom, int col);

/**
 * Make search field visible.
 *
 * \param search_field Search field
 **/
void multitrack_search_field_show(multitrack_search_field_t *search_field);

/**
 * Refresh search field.
 *
 * \param search_field Search field
 **/
void multitrack_search_field_display(multitrack_search_field_t *search_field);

/**
 * Retain size of search field during terminal resize.
 *
 * \param search_field Search field
 **/
void multitrack_search_field_resize(multitrack_search_field_t *search_field);

/**
 * Hide search field from view.
 *
 * \param search_field Search field
 **/
void multitrack_search_field_hide(multitrack_search_field_t *search_field);

/**
 * Destroy search field and associated memory.
 *
 * \param search_field Search field
 **/
void multitrack_search_field_destroy(multitrack_search_field_t **search_field);

/**
 * Handle input characters to search field.
 *
 * \param search_field Search field
 * \param input_key Input key
 * \return True if an input character was handles, false otherwise
 **/
bool multitrack_search_field_handle(multitrack_search_field_t *search_field, int input_key);

/**
 * Get (trimmed) input string from search field.
 *
 * \param search_field Search field
 **/
char *multitrack_search_field_string(multitrack_search_field_t *search_field);

/**
 * Add match to match array.
 *
 * \param search_field Search field
 * \param index Menu index to add to list of matches
 **/
void multitrack_search_field_add_match(multitrack_search_field_t *search_field, int index);

/**
 * Clear match array.
 *
 * \param search_field Search field
 **/
void multitrack_search_field_clear_matches(multitrack_search_field_t *search_field);

/**
 * Update window size according to current terminal height.
 *
 * \param listing Multitrack listing
 **/
void multitrack_resize(multitrack_listing_t *listing);

/** Multitrack satellite listing function implementations. **/

multitrack_entry_t *multitrack_create_entry(const char *name, predict_orbital_elements_t *orbital_elements)
{
	multitrack_entry_t *entry = (multitrack_entry_t*)malloc(sizeof(multitrack_entry_t));
	entry->orbital_elements = orbital_elements;
	entry->name = strdup(name);
	entry->next_aos = 0;
	entry->next_los = 0;
	entry->above_horizon = 0;
	entry->geostationary = 0;
	entry->never_visible = 0;
	entry->decayed = 0;
	return entry;
}

void multitrack_resize(multitrack_listing_t *listing)
{
	//resize main window
	int sat_list_win_height = LINES-MAIN_MENU_OPTS_WIN_HEIGHT-MULTITRACK_WINDOW_ROW-1;

	if (sat_list_win_height > 0) {
		//move and resize listing window to correct location and size
		wresize(listing->window, sat_list_win_height, MULTITRACK_WINDOW_WIDTH);
		mvwin(listing->window, MULTITRACK_WINDOW_ROW, 0);

		//update internal variables
		int window_height, window_width;
		getmaxyx(listing->window, window_height, window_width);
		listing->window_height = window_height;
		listing->window_width = window_width;
		listing->displayed_entries_per_page = window_height;
		int window_row = getbegy(listing->window);
		listing->window_row = window_row;
		listing->bottom_index = listing->top_index + listing->displayed_entries_per_page - 1;
		wrefresh(listing->window);
	}

	//make header line behave correctly
	wresize(listing->header_window, 1, COLS);
	wrefresh(listing->header_window);
}

multitrack_listing_t* multitrack_create_listing(predict_observer_t *observer, struct tle_db *tle_db)
{
	multitrack_listing_t *listing = (multitrack_listing_t*)malloc(sizeof(multitrack_listing_t));

	//prepare main window. Proper size and position is set in multitrack_resize().
	listing->window = newwin(1, 1, 1, 0);

	//prepare window for header printing. Proper size is set in multitrack_resize().
	listing->header_window = newwin(1, COLS, 0, 0);

	multitrack_resize(listing);

	listing->num_entries = 0;
	listing->entries = NULL;
	listing->tle_db_mapping = NULL;
	listing->sorted_index = NULL;

	listing->qth = observer;

	multitrack_refresh_tles(listing, tle_db);

	listing->option_selector = multitrack_option_selector_create();

	int search_row = 3;
	int search_col = 0;
	listing->search_field = multitrack_search_field_create(search_row, search_col);

	listing->terminal_height = LINES;
	listing->terminal_width = LINES;
	return listing;
}

void multitrack_search_listing(multitrack_listing_t *listing)
{
	multitrack_search_field_clear_matches(listing->search_field);
	char *expression = multitrack_search_field_string(listing->search_field);
	if (strlen(expression) == 0) {
		free(expression);
		return;
	}
	for (int i=0; i < listing->num_entries; i++) {
		if (strstr(listing->entries[listing->sorted_index[i]]->name, expression) != NULL) {
			multitrack_search_field_add_match(listing->search_field, i);
		}
	}
	multitrack_listing_next_match(listing);
	free(expression);
}

void multitrack_listing_next_match(multitrack_listing_t *listing)
{
	multitrack_search_field_t *search_field = listing->search_field;
	if (search_field->num_matches > 0) {
		search_field->match_num = (search_field->match_num + 1) % (search_field->num_matches);
		listing->selected_entry_index = search_field->matches[search_field->match_num];
	}
}

void multitrack_free_entry(multitrack_entry_t **entry)
{
	predict_destroy_orbital_elements((*entry)->orbital_elements);
	free((*entry)->name);
	free(*entry);
	*entry = NULL;
}

void multitrack_free_entries(multitrack_listing_t *listing)
{
	if (listing->entries != NULL) {
		for (int i=0; i < listing->num_entries; i++) {
			multitrack_free_entry(&(listing->entries[i]));
		}
		free(listing->entries);
		listing->entries = NULL;
	}
	if (listing->tle_db_mapping != NULL) {
		free(listing->tle_db_mapping);
		listing->tle_db_mapping = NULL;
	}
	if (listing->sorted_index != NULL) {
		free(listing->sorted_index);
		listing->sorted_index = NULL;
	}
	listing->num_entries = 0;
}

void multitrack_refresh_tles(multitrack_listing_t *listing, struct tle_db *tle_db)
{
	werase(listing->window);
	listing->not_displayed = true;
	multitrack_free_entries(listing);

	int num_enabled_tles = 0;
	for (int i=0; i < tle_db->num_tles; i++) {
		if (tle_db_entry_enabled(tle_db, i)) {
			num_enabled_tles++;
		}
	}

	listing->num_entries = num_enabled_tles;

	if (listing->num_entries > 0) {
		listing->entries = (multitrack_entry_t**)malloc(sizeof(multitrack_entry_t*)*num_enabled_tles);
		listing->tle_db_mapping = (int*)calloc(num_enabled_tles, sizeof(int));
		listing->sorted_index = (int*)calloc(tle_db->num_tles, sizeof(int));

		int j=0;
		for (int i=0; i < tle_db->num_tles; i++) {
			if (tle_db_entry_enabled(tle_db, i)) {
				predict_orbital_elements_t *orbital_elements = tle_db_entry_to_orbital_elements(tle_db, i);
				listing->entries[j] = multitrack_create_entry(tle_db_entry_name(tle_db, i), orbital_elements);
				listing->tle_db_mapping[j] = i;
				listing->sorted_index[j] = j;
				j++;
			}
		}
	}

	listing->selected_entry_index = 0;
	listing->top_index = 0;

	listing->num_above_horizon = 0;
	listing->num_below_horizon = 0;
	listing->num_decayed = 0;
	listing->num_nevervisible = 0;
	multitrack_resize(listing);
}

NCURSES_ATTR_T multitrack_colors(double range, double elevation)
{
	if (range < 8000)
		if (range < 4000)
			if (range < 2000)
				if (range < 1000)
					if (elevation > 10)
						return (COLOR_PAIR(6)|A_REVERSE); /* red */
					else
						return (COLOR_PAIR(3)|A_REVERSE); /* yellow */
				else
					if (elevation > 20)
						return (COLOR_PAIR(3)|A_REVERSE); /* yellow */
					else
						return (COLOR_PAIR(4)|A_REVERSE); /* cyan */
			else
				if (elevation > 40)
					return (COLOR_PAIR(4)|A_REVERSE); /* cyan */
				else
					return (COLOR_PAIR(1)|A_REVERSE); /* white */
		else
			return (COLOR_PAIR(1)|A_REVERSE); /* white */
	else
		return (COLOR_PAIR(2)|A_REVERSE); /* reverse */
}

void multitrack_update_entry(predict_observer_t *qth, multitrack_entry_t *entry, predict_julian_date_t time)
{
	entry->geostationary = false;

	struct predict_observation obs;
	struct predict_orbit orbit;
	predict_orbit(entry->orbital_elements, &orbit, time);
	predict_observe_orbit(qth, &orbit, &obs);

	//sun status
	char sunstat;
	if (!orbit.eclipsed) {
		if (obs.visible) {
			sunstat='V';
		} else {
			sunstat='D';
		}
	} else {
		sunstat='N';
	}

	//satellite approaching status
	char rangestat;
	if (fabs(obs.range_rate) < 0.1) {
		rangestat = '=';
	} else if (obs.range_rate < 0.0) {
		rangestat = '/';
	} else if (obs.range_rate > 0.0) {
		rangestat = '\\';
	}

	//set text formatting attributes according to satellite state, set AOS/LOS string
	bool can_predict = !predict_is_geostationary(entry->orbital_elements) && predict_aos_happens(entry->orbital_elements, qth->latitude) && !(orbit.decayed);
	char aos_los[MAX_NUM_CHARS] = {0};
	if (obs.elevation >= 0) {
		//different colours according to range and elevation
		entry->display_attributes = multitrack_colors(obs.range, obs.elevation*180/M_PI);

		if (predict_is_geostationary(entry->orbital_elements)){
			sprintf(aos_los, "*GeoS*");
			entry->geostationary = true;
		} else {
			time_t epoch = predict_from_julian(entry->next_los - time);
			struct tm timeval;
			gmtime_r(&epoch, &timeval);
			if ((entry->next_los - time) > 1.0) {
				int num_days = (entry->next_los - time);
				snprintf(aos_los, MAX_NUM_CHARS, "%d days", num_days);
			} else if (timeval.tm_hour > 0) {
				strftime(aos_los, MAX_NUM_CHARS, "%H:%M:%S", &timeval);
			} else {
				strftime(aos_los, MAX_NUM_CHARS, "%M:%S", &timeval);
			}

		}
	} else if ((obs.elevation < 0) && can_predict) {
		if ((entry->next_aos-time) < 0.00694) {
			//satellite is close, set bold
			entry->display_attributes = COLOR_PAIR(2);
			time_t epoch = predict_from_julian(entry->next_aos - time);
			strftime(aos_los, MAX_NUM_CHARS, "%M:%S", gmtime(&epoch)); //minutes and seconds left until AOS
		} else {
			//satellite is far, set normal coloring
			entry->display_attributes = COLOR_PAIR(4);
			time_t aoslos_epoch = predict_from_julian(entry->next_aos);
			time_t curr_epoch = predict_from_julian(time);
			struct tm aostime, currtime;
			gmtime_r(&aoslos_epoch, &aostime);
			gmtime_r(&curr_epoch, &currtime);
			aostime.tm_yday = aostime.tm_yday - currtime.tm_yday;
			char temp[MAX_NUM_CHARS];
			strftime(temp, MAX_NUM_CHARS, "%H:%MZ", &aostime);
			if (aostime.tm_yday == 0) {
				strncpy(aos_los, temp, MAX_NUM_CHARS);
			} else {
				snprintf(aos_los, MAX_NUM_CHARS, "+%dd %s", aostime.tm_yday, temp);
			}
		}
	} else if (!can_predict) {
		entry->display_attributes = COLOR_PAIR(3);
		sprintf(aos_los, "*GeoS-NoAOS*");
	}

	char abs_pos_string[MAX_NUM_CHARS] = {0};
	sprintf(abs_pos_string, "%3.0f  %3.0f", orbit.latitude*180.0/M_PI, orbit.longitude*180.0/M_PI);

	/* Calculate Next Event (AOS/LOS) Times */
	if (can_predict && (time > entry->next_los) && (obs.elevation > 0)) {
		entry->next_los= predict_next_los(qth, entry->orbital_elements, time);
		predict_julian_date_t max_ele_time = predict_max_elevation(qth, entry->orbital_elements, time);

		struct predict_orbit orbit;
		struct predict_observation observation;
		predict_orbit(entry->orbital_elements, &orbit, max_ele_time);
		predict_observe_orbit(qth, &orbit, &observation);
		entry->max_elevation = observation.elevation;
	}

	if (can_predict && (time > entry->next_aos)) {
		if (obs.elevation < 0) {
			entry->next_aos = predict_next_aos(qth, entry->orbital_elements, time);
		}
	}

	//altitude and range in km/miles
	double disp_altitude = orbit.altitude;
	double disp_range = obs.range;

	//set string to display
	char disp_string[MAX_NUM_CHARS];
	sprintf(disp_string, " %-10.8s%5.1f  %5.1f %8s%6.0f %6.0f %c %c %12s ", entry->name, obs.azimuth*180.0/M_PI, obs.elevation*180.0/M_PI, abs_pos_string, disp_altitude, disp_range, sunstat, rangestat, aos_los);

	//overwrite everything if orbit was decayed
	if (orbit.decayed) {
		entry->display_attributes = COLOR_PAIR(2);
		sprintf(disp_string, " %-10.8s ----------------     Decayed       --------------- ", entry->name);
	}

	memcpy(entry->display_string, disp_string, sizeof(char)*MAX_NUM_CHARS);

	entry->above_horizon = obs.elevation > 0;
	entry->decayed = orbit.decayed;

	entry->never_visible = !predict_aos_happens(entry->orbital_elements, qth->latitude) || (predict_is_geostationary(entry->orbital_elements) && (obs.elevation <= 0.0));
}

void multitrack_update_listing_data(multitrack_listing_t *listing, predict_julian_date_t time)
{
	for (int i=0; i < listing->num_entries; i++) {
		if (listing->not_displayed) {
			//display progress information when this is the first time entries are displayed
			wattrset(listing->window, COLOR_PAIR(1));
			mvwprintw(listing->window, 0, 1, "Preparing entry %d of %d\n", i, listing->num_entries);
			wrefresh(listing->window);
		}
		multitrack_entry_t *entry = listing->entries[i];
		multitrack_update_entry(listing->qth, entry, time);
	}

	if (!multitrack_option_selector_visible(listing->option_selector) && !multitrack_search_field_visible(listing->search_field)) {
		multitrack_sort_listing(listing); //freeze sorting when option selector is hovering over a satellite
	}

	listing->not_displayed = false;
}

void multitrack_sort_listing(multitrack_listing_t *listing)
{
	int num_orbits = listing->num_entries;

	//those with elevation > 0 at the top
	int above_horizon_counter = 0;
	for (int i=0; i < num_orbits; i++){
		if (listing->entries[i]->above_horizon && !(listing->entries[i]->decayed)) {
			listing->sorted_index[above_horizon_counter] = i;
			above_horizon_counter++;
		}
	}
	listing->num_above_horizon = above_horizon_counter;

	//satellites that will eventually rise above the horizon
	int below_horizon_counter = 0;
	for (int i=0; i < num_orbits; i++){
		if (!(listing->entries[i]->above_horizon) && !(listing->entries[i]->never_visible) && !(listing->entries[i]->decayed)) {
			listing->sorted_index[below_horizon_counter + above_horizon_counter] = i;
			below_horizon_counter++;
		}
	}
	listing->num_below_horizon = below_horizon_counter;

	//satellites that will never be visible, with decayed orbits last
	int nevervisible_counter = 0;
	int decayed_counter = 0;
	for (int i=0; i < num_orbits; i++){
		if (listing->entries[i]->never_visible && !(listing->entries[i]->decayed)) {
			listing->sorted_index[below_horizon_counter + above_horizon_counter + nevervisible_counter] = i;
			nevervisible_counter++;
		} else if (listing->entries[i]->decayed) {
			listing->sorted_index[num_orbits - 1 - decayed_counter] = i;
			decayed_counter++;
		}
	}
	listing->num_nevervisible = nevervisible_counter;
	listing->num_decayed = decayed_counter;

	//sort internally according to AOS/LOS
	for (int i=0; i < above_horizon_counter + below_horizon_counter; i++) {
		for (int j=0; j < above_horizon_counter + below_horizon_counter - 1; j++){
			if (listing->entries[listing->sorted_index[j]]->next_aos > listing->entries[listing->sorted_index[j+1]]->next_aos) {
				int x = listing->sorted_index[j];
				listing->sorted_index[j] = listing->sorted_index[j+1];
				listing->sorted_index[j+1] = x;
			}
		}
	}
}

void multitrack_display_entry(WINDOW *window, int row, int col, multitrack_entry_t *entry)
{
	wattrset(window, entry->display_attributes);
	mvwprintw(window, row, col, "%s", entry->display_string);
}

void multitrack_print_scrollbar(multitrack_listing_t *listing)
{
	int scrollarea_offset = 0;
	int scrollarea_height = listing->window_height;
	int scrollbar_height = ((listing->displayed_entries_per_page*1.0)/(listing->num_entries*1.0))*scrollarea_height;

	//print scrollarea
	for (int i=0; i < scrollarea_height; i++) {
		int row = i+scrollarea_offset;
		wattrset(listing->window, COLOR_PAIR(8));
		mvwprintw(listing->window, row, listing->window_width-2, "  ");
	}

	//print scrollbar
	int scrollbar_placement = (listing->top_index*1.0/(listing->num_entries - listing->displayed_entries_per_page))*(scrollarea_height - scrollbar_height);
	for (int i=scrollbar_placement; i < scrollbar_height+scrollbar_placement; i++) {
		int row = i+scrollarea_offset;
		wattrset(listing->window, COLOR_PAIR(8)|A_REVERSE);
		mvwprintw(listing->window, row, listing->window_width-2, "  ");
	}
}

void multitrack_display_listing(multitrack_listing_t *listing)
{
	if ((listing->terminal_height != LINES) || (listing->terminal_width != COLS)) {
		multitrack_resize(listing);
		multitrack_search_field_resize(listing->search_field);
		multitrack_option_selector_resize(listing->option_selector);

		listing->terminal_height = LINES;
		listing->terminal_width = COLS;
	}

	//show header
	wbkgd(listing->header_window, HEADER_STYLE);
	wattrset(listing->header_window, HEADER_STYLE);
	char header_text[] = "  Satellite  Azim   Elev Lat Long   Alt   Range     Next AOS/LOS    ";
	mvwprintw(listing->header_window, 0, 0, header_text);

	//show UTC clock in header
	time_t epoch = time(NULL);
	char time_string[MAX_NUM_CHARS] = {0};
	strftime(time_string, MAX_NUM_CHARS, "%H:%M:%SZ", gmtime(&epoch));
	mvwprintw(listing->header_window, 0, strlen(header_text), "%s", time_string);

	//show entries
	if (listing->num_entries > 0) {
		int selected_index = listing->sorted_index[listing->selected_entry_index];
		listing->entries[selected_index]->display_attributes = MULTITRACK_SELECTED_ATTRIBUTE;
		listing->entries[selected_index]->display_string[0] = MULTITRACK_SELECTED_MARKER;

		int line = 0;
		int col = 1;

		for (int i=listing->top_index; ((i <= listing->bottom_index) && (i < listing->num_entries)); i++) {
			multitrack_display_entry(listing->window, line++, col, listing->entries[listing->sorted_index[i]]);
		}

		if (listing->num_entries > listing->displayed_entries_per_page) {
			multitrack_print_scrollbar(listing);
		}
	} else {
		wattrset(listing->window, COLOR_PAIR(1));
		mvwprintw(listing->window, 5, 2, "Satellite list is empty. Are any satellites enabled?");
		mvwprintw(listing->window, 6, 2, "(Press 'W' to enable satellites)");
	}
	wrefresh(listing->window);
	wrefresh(listing->header_window);

	//refresh search field
	multitrack_search_field_display(listing->search_field);

	//refresh option selector
	int option_selector_row = multitrack_selected_window_row(listing) + listing->window_row + 1;
	if (option_selector_row + listing->option_selector->window_height > LINES) {
		//flip menu since it otherwise would try to be outside the terminal
		option_selector_row = option_selector_row - listing->option_selector->window_height - 1;
	}
	multitrack_option_selector_display(option_selector_row, listing->option_selector);
}

bool multitrack_handle_listing(multitrack_listing_t *listing, int input_key)
{
	if (multitrack_option_selector_visible(listing->option_selector)) {
		return multitrack_option_selector_handle(listing->option_selector, input_key);
	} else if (listing->num_entries > 0) {
		bool handled = false;
		switch (input_key) {
			case KEY_UP:
				listing->selected_entry_index--;
				handled = true;
				break;
			case KEY_DOWN:
				listing->selected_entry_index++;
				handled = true;
				break;
			case KEY_PPAGE:
				listing->selected_entry_index -= listing->displayed_entries_per_page;
				handled = true;
				break;
			case KEY_NPAGE:
				listing->selected_entry_index += listing->displayed_entries_per_page;
				handled = true;
				break;
			case 10:
			case KEY_RIGHT:
				if (listing->num_entries > 0) {
					multitrack_option_selector_show(listing->option_selector);
				}
				handled = true;
				break;
			case 27:
				if (multitrack_search_field_visible(listing->search_field)) {
					multitrack_search_field_hide(listing->search_field);
				}
				handled = true;
				break;
			case '/':
			case KEY_F(3):
				if (multitrack_search_field_visible(listing->search_field)) {
					multitrack_listing_next_match(listing);
				} else {
					multitrack_search_field_show(listing->search_field);
				}
				handled = true;
				break;
			//quick jump to each option
			case 't':
			case 'T':
				if (!multitrack_search_field_visible(listing->search_field)) {
					multitrack_option_selector_jump(listing->option_selector, OPTION_SINGLETRACK);
					handled = true;
					break;
				}
			case 'p':
			case 'P':
				if (!multitrack_search_field_visible(listing->search_field)) {
					multitrack_option_selector_jump(listing->option_selector, OPTION_PREDICT);
					handled = true;
					break;
				}
			case 'v':
			case 'V':
				if (!multitrack_search_field_visible(listing->search_field)) {
					multitrack_option_selector_jump(listing->option_selector, OPTION_PREDICT_VISIBLE);
					handled = true;
					break;
				}
			default:
				if (multitrack_search_field_visible(listing->search_field)) {
					if (multitrack_search_field_handle(listing->search_field, input_key)) {
						multitrack_search_listing(listing);
					}
					handled = true;
				}
			break;
		}

		//adjust index according to limits
		if (listing->selected_entry_index < 0) {
			listing->selected_entry_index = 0;
		}

		if (listing->selected_entry_index >= listing->num_entries) {
			listing->selected_entry_index = listing->num_entries-1;
		}

		//check for scroll event
		if (listing->selected_entry_index > listing->bottom_index) {
			int diff = listing->selected_entry_index - listing->bottom_index;
			listing->bottom_index += diff;
			listing->top_index += diff;
		}
		if (listing->selected_entry_index < listing->top_index) {
			int diff = listing->top_index - listing->selected_entry_index;
			listing->bottom_index -= diff;
			listing->top_index -= diff;
		}
		return handled;
	}
	return false;
}

int multitrack_selected_entry(multitrack_listing_t *listing)
{
	int index = listing->sorted_index[listing->selected_entry_index];
	return listing->tle_db_mapping[index];
}

int multitrack_selected_window_row(multitrack_listing_t *listing)
{
	return listing->selected_entry_index - listing->top_index;
}

void multitrack_destroy_listing(multitrack_listing_t **listing)
{
	multitrack_free_entries(*listing);
	multitrack_option_selector_destroy(&((*listing)->option_selector));
	multitrack_search_field_destroy(&((*listing)->search_field));
	delwin((*listing)->header_window);
	delwin((*listing)->window);
	free(*listing);
	*listing = NULL;
}

/** Option selector submenu function implementations. **/

//number of options in the option selector submenu
#define NUM_OPTIONS 6

void multitrack_option_selector_destroy(multitrack_option_selector_t **option_selector)
{
	unpost_menu((*option_selector)->menu);
	free_menu((*option_selector)->menu);
	delwin((*option_selector)->sub_window);
	delwin((*option_selector)->window);
	for (int i=0; i < NUM_OPTIONS; i++) {
		free_item((*option_selector)->items[i]);
	}
	free((*option_selector)->items);
	free((*option_selector)->item_types);
	free((*option_selector));
	*option_selector = NULL;
}

const char *multitrack_option_selector_name(enum sub_menu_options option)
{
	switch (option) {
		case OPTION_SINGLETRACK:
			return "Track satellite";
		case OPTION_PREDICT:
			return "Predict passes";
		case OPTION_PREDICT_VISIBLE:
			return "Predict visible passes";
		case OPTION_DISPLAY_ORBITAL_DATA:
			return "Display orbital data";
		case OPTION_SOLAR_ILLUMINATION:
			return "Solar illumination prediction";
		case OPTION_EDIT_TRANSPONDER:
			return "Show transponders";
	}
	return "";
}

//Width of option selector window
#define OPTION_SELECTOR_WINDOW_WIDTH 30

//Column offset of option selector window
#define OPTION_SELECTOR_WINDOW_OFFSET 2

multitrack_option_selector_t* multitrack_option_selector_create()
{
	multitrack_option_selector_t *option_selector = (multitrack_option_selector_t*)malloc(sizeof(multitrack_option_selector_t));

	//prepare option selector window
	option_selector->window_height = NUM_OPTIONS;
	WINDOW *option_win = newwin(option_selector->window_height, OPTION_SELECTOR_WINDOW_WIDTH, 0, 0);
	wattrset(option_win, COLOR_PAIR(1)|A_REVERSE);
	werase(option_win);
	wrefresh(option_win);
	option_selector->window = option_win;

	//prepare ITEMs
	int item_types[NUM_OPTIONS] = {OPTION_SINGLETRACK,
				      OPTION_PREDICT,
				      OPTION_PREDICT_VISIBLE,
				      OPTION_DISPLAY_ORBITAL_DATA,
				      OPTION_EDIT_TRANSPONDER,
				      OPTION_SOLAR_ILLUMINATION};

	option_selector->items = (ITEM**)malloc(sizeof(ITEM*)*(NUM_OPTIONS+1));
	option_selector->item_types = (int*)malloc(sizeof(int)*(NUM_OPTIONS+1));
	for (int i=0; i < NUM_OPTIONS; i++) {
		option_selector->item_types[i] = item_types[i];
		option_selector->items[i] = new_item(multitrack_option_selector_name(item_types[i]), "");
		set_item_userptr(option_selector->items[i], &(option_selector->item_types[i]));
	}
	option_selector->items[NUM_OPTIONS] = NULL;

	//prepare MENU
	MENU *menu = new_menu(option_selector->items);
	set_menu_back(menu,COLOR_PAIR(1)|A_REVERSE);
	set_menu_fore(menu,COLOR_PAIR(4)|A_REVERSE);
	set_menu_win(menu, option_win);

	int max_width, max_height;
	getmaxyx(option_win, max_height, max_width);
	option_selector->sub_window = derwin(option_win, max_height, max_width-1, 0, 1);
	set_menu_sub(menu, option_selector->sub_window);
	set_menu_format(menu, max_height, 1);

	set_menu_mark(menu, "");
	post_menu(menu);
	option_selector->menu = menu;
	option_selector->visible = false;
	option_selector->option_selected = false;
	return option_selector;
}

void multitrack_option_selector_hide(multitrack_option_selector_t *option_selector)
{
	option_selector->visible = false;
	wbkgd(option_selector->window, COLOR_PAIR(1));
	werase(option_selector->window);
	wrefresh(option_selector->window);
}

void multitrack_option_selector_show(multitrack_option_selector_t *option_selector)
{
	option_selector->visible = true;
}

bool multitrack_option_selector_visible(multitrack_option_selector_t *option_selector)
{
	return option_selector->visible;
}

void multitrack_option_selector_resize(multitrack_option_selector_t *option_selector)
{
	//ensure correct size of windows
	wresize(option_selector->window, option_selector->window_height, OPTION_SELECTOR_WINDOW_WIDTH);

	int max_height, max_width;
	getmaxyx(option_selector->window, max_height, max_width);
	wresize(option_selector->sub_window, max_height, max_width-1);

}

void multitrack_option_selector_display(int row, multitrack_option_selector_t *option_selector)
{
	if (option_selector->visible) {
		//move and unhide windows
		mvwin(option_selector->window, row, OPTION_SELECTOR_WINDOW_OFFSET);
		wbkgd(option_selector->window, COLOR_PAIR(4)|A_REVERSE);
		unpost_menu(option_selector->menu);
		post_menu(option_selector->menu);
		wrefresh(option_selector->window);
	}
}

bool multitrack_option_selector_handle(multitrack_option_selector_t *option_selector, int input_key)
{
	bool handled = false;
	switch (input_key) {
		case KEY_RIGHT:
		case 10:
			option_selector->option_selected = true;
		case 'q':
		case 27:
		case KEY_LEFT:
			multitrack_option_selector_hide(option_selector);
			handled = true;
			break;
		case KEY_UP:
			menu_driver(option_selector->menu, REQ_UP_ITEM);
			handled = true;
			break;
		case KEY_DOWN:
			menu_driver(option_selector->menu, REQ_DOWN_ITEM);
			handled = true;
			break;
	}
	return handled;
}

int multitrack_option_selector_get_option(multitrack_option_selector_t *option_selector)
{
	int option = *((int*)item_userptr(current_item(option_selector->menu)));
	return option;
}

bool multitrack_option_selector_pop(multitrack_option_selector_t *option_selector)
{
	if (option_selector->option_selected) {
		option_selector->option_selected = false;
		return true;
	} else {
		return false;
	}
}

void multitrack_option_selector_jump(multitrack_option_selector_t *option_selector, enum sub_menu_options option)
{
	set_menu_pattern(option_selector->menu, multitrack_option_selector_name(option));
	option_selector->option_selected = true;
}

/** Search field function implementations. **/

//Height of search window
#define SEARCH_FIELD_HEIGHT 3

//Width of search window
#define SEARCH_FIELD_LENGTH 78

//Column at which to start the search field
#define FIELD_START_COL 29

//Length of search field
#define FIELD_LENGTH (SEARCH_FIELD_LENGTH - FIELD_START_COL)

multitrack_search_field_t *multitrack_search_field_create(int row, int col)
{
	multitrack_search_field_t *searcher = (multitrack_search_field_t*)malloc(sizeof(multitrack_search_field_t));
	searcher->row_offset = row;
	searcher->col = col;
	searcher->window = newwin(SEARCH_FIELD_HEIGHT, SEARCH_FIELD_LENGTH, LINES-row, col);
	searcher->visible = false;

	//prepare search match control
	searcher->matches = NULL;
	multitrack_search_field_clear_matches(searcher);

	return searcher;
}

void multitrack_search_field_show(multitrack_search_field_t *search_field)
{
	//create field
	if (!search_field->visible) {
		search_field->field = (FIELD**)malloc(sizeof(FIELD*)*2);
		search_field->field[0] = new_field(1, FIELD_LENGTH, 0, 0, 0, 0);
		search_field->field[1] = NULL;
		search_field->form = new_form(search_field->field);
		field_opts_off(search_field->field[0], O_AUTOSKIP);
		int rows;
		scale_form(search_field->form, &rows, &(search_field->form_window_cols));
		set_form_win(search_field->form, search_field->window);
		search_field->form_window = derwin(search_field->window, rows, search_field->form_window_cols, 0, FIELD_START_COL);
		set_form_sub(search_field->form, search_field->form_window);
		keypad(search_field->window, TRUE);
		post_form(search_field->form);
	}

	search_field->visible = true;
	multitrack_search_field_display(search_field);
}

bool multitrack_search_field_visible(multitrack_search_field_t *search_field)
{
	return search_field->visible;
}

void multitrack_search_field_resize(multitrack_search_field_t *search_field)
{
	wresize(search_field->window, SEARCH_FIELD_HEIGHT, SEARCH_FIELD_LENGTH);
	mvwin(search_field->window, LINES-search_field->row_offset, search_field->col);

	if (search_field->visible) {
		multitrack_search_field_hide(search_field);
		multitrack_search_field_show(search_field);
	}
}

void multitrack_search_field_display(multitrack_search_field_t *search_field)
{
	//set font color according to whether there is a match or not
	if (search_field->num_matches > 0) {
		search_field->attributes = COLOR_PAIR(4)|A_REVERSE;
	} else {
		search_field->attributes = COLOR_PAIR(7)|A_REVERSE;
	}

	if (search_field->visible) {
		//fill window without destroying the styling of the FORM :^)
		wattrset(search_field->window, COLOR_PAIR(1));
		mvwprintw(search_field->window, 1, 0, "%-78s", "");
		mvwprintw(search_field->window, 2, 0, "%-78s", "");

		//print keybindings in the same style as htop
		int col = 0;
		wattrset(search_field->window, COLOR_PAIR(1));
		mvwprintw(search_field->window, 0, col, "F3");
		col+=2;
		wattrset(search_field->window, COLOR_PAIR(4)|A_REVERSE);
		mvwprintw(search_field->window, 0, col, "Next   ");
		col+=6;
		wattrset(search_field->window, COLOR_PAIR(1));
		mvwprintw(search_field->window, 0, col, "Esc");
		col+=3;
		wattrset(search_field->window, COLOR_PAIR(4)|A_REVERSE);
		mvwprintw(search_field->window, 0, col, "Cancel ");
		col+=8;
		wattrset(search_field->window, COLOR_PAIR(1));
		mvwprintw(search_field->window, 0, col, " ");
		col+=1;
		wattrset(search_field->window, COLOR_PAIR(4)|A_REVERSE);
		mvwprintw(search_field->window, 0, col, " Search: ");
		col+=9;

		//update form colors
		set_field_back(search_field->field[0], search_field->attributes);
		form_driver(search_field->form, REQ_VALIDATION);
		wrefresh(search_field->window);
	}
}

void multitrack_search_field_hide(multitrack_search_field_t *search_field)
{
	form_driver(search_field->form, REQ_CLR_FIELD);
	search_field->visible = false;
	multitrack_search_field_clear_matches(search_field);

	//remove field and form
	unpost_form(search_field->form);
	free_form(search_field->form);
	free_field(search_field->field[0]);
	free(search_field->field);
	delwin(search_field->form_window);
}

void multitrack_search_field_destroy(multitrack_search_field_t **search_field)
{
	multitrack_search_field_clear_matches(*search_field);
	delwin((*search_field)->window);
	free(*search_field);
	*search_field = NULL;
}

bool multitrack_search_field_handle(multitrack_search_field_t *search_field, int input_key)
{
	bool character_handled = false;
	char *expression = NULL;
	switch (input_key) {
		case KEY_BACKSPACE:
			expression = multitrack_search_field_string(search_field);
			if (strlen(expression) == 0) {
				//hide search field if it already is empty
				multitrack_search_field_hide(search_field);
			} else {
				//delete characters
				form_driver(search_field->form, REQ_DEL_PREV);
				form_driver(search_field->form, REQ_VALIDATION);
				character_handled = true;
			}
			free(expression);
			break;
		default:
			form_driver(search_field->form, input_key);
			form_driver(search_field->form, REQ_VALIDATION);
			character_handled = true;
			break;
	}
	return character_handled;
}

char *multitrack_search_field_string(multitrack_search_field_t *search_field)
{
	char *ret_str = strdup(field_buffer(search_field->field[0], 0));
	trim_whitespaces_from_end(ret_str);
	return ret_str;
}

void multitrack_search_field_clear_matches(multitrack_search_field_t *search_field)
{
	search_field->match_num = -1;

	if (search_field->matches != NULL) {
		free(search_field->matches);
		search_field->matches = NULL;
	}
	search_field->num_matches = 0;
	search_field->available_match_size = 0;
}

void multitrack_search_field_add_match(multitrack_search_field_t *search_field, int index)
{
	if (search_field->matches == NULL) {
		search_field->available_match_size = 2;
		search_field->matches = (int*)malloc(sizeof(int)*search_field->available_match_size);
	}

	if (search_field->num_matches == search_field->available_match_size) {
		search_field->available_match_size = search_field->available_match_size*2;
		search_field->matches = (int*)realloc(search_field->matches, sizeof(int)*search_field->available_match_size);
	}

	search_field->matches[search_field->num_matches++] = index;
}
