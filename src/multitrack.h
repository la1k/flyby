#ifndef MULTITRACK_H_DEFINED
#define MULTITRACK_H_DEFINED

#include "ncurses.h"
#include "form.h"
#include "menu.h"

//Width of multitrack window
#define MULTITRACK_WINDOW_WIDTH 67

//Height of multitrack header window
#define MULTITRACK_HEADER_HEIGHT 2

//Start row for multitrack window printing
#define MULTITRACK_WINDOW_ROW (MULTITRACK_HEADER_HEIGHT+1)

enum sort_options{SORT_BY_AOS, SORT_BY_MAX_ELEVATION};

/**
 * Structs and functions used for showing a navigateable real-time satellite listing.
 **/

/**
 * Entry in satellite listing.
 **/
typedef struct {
	///Satellite name
	char *name;
	///Orbital elements for satellite
	predict_orbital_elements_t *orbital_elements;
	///Time for next AOS
	double next_aos;
	///Time for next LOS
	double next_los;
	///Maximum elevation of the next or current pass in degrees
	double max_elevation;
	///Whether satellite currently is above horizon
	bool above_horizon;
	///Whether satellite is geostationary
	bool geostationary;
	///Whether satellite is never visible
	bool never_visible;
	///Whether satellite is above maximum elevation threshold
	bool above_max_elevation_threshold;
	///Whether satellite has decayed
	bool decayed;
	///String used for information displaying in the satellite listing
	char display_string[MAX_NUM_CHARS];
	///Formatting attributes (input to wattrset())
	int display_attributes;
} multitrack_entry_t;

/**
 * Submenu shown when pressing -> or ENTER on selected satellite in multitrack listing.
 **/
typedef struct {
	///Menu
	MENU *menu;
	///Window height
	int window_height;
	///Display window, shifted around to selected satellite when shown
	WINDOW *window;
	///Sub window used for menu
	WINDOW *sub_window;
	///Whether window is visible
	bool visible;
	///List of menu items
	ITEM **items;
	///Annotation of menu items: Contains members of `enum sub_menu_options` for what to do on selection
	int *item_types;
	///Whether an option has been selected. Submenu is hidden, and this field is set to `true` when an option has been selected
	bool option_selected;
} multitrack_option_selector_t;

/**
 * Search field shown when pressing '/'. (display format inspired by htop)
 **/
typedef struct {
	///Location of search field counted from the bottom of the terminal and up
	int row_offset;
	///Column location of search field
	int col;
	///Display window
	WINDOW *window;
	///Form window
	WINDOW *form_window;
	///Number of columns in form window
	int form_window_cols;
	///Search form
	FORM *form;
	///Search field in form
	FIELD **field;
	///Whether visible
	bool visible;
	///Display attributes of search string (set to red when no matches found)
	int attributes;
	///Current match number state (used for jumping through the matches)
	int match_num;
	///List of match indices in the sorted index array
	int *matches;
	///Number of matches
	int num_matches;
	///Available size in the `matches` array, reallocated at need
	int available_match_size;
} multitrack_search_field_t;

/**
 * Satellite listing, used for providing a live overview of the current status of the satellites, and for
 * providing a selection menu for these satellites.
 **/
typedef struct {
	///Whether multitrack listing has been displayed for the first time yet
	bool not_displayed;
	///Number of displayed satellites
	int num_entries;
	///Displayed satellites
	multitrack_entry_t **entries;
	///Index mapping from index corresponding to displayed entry in menu to index in `entries`-array
	int *sorted_index;
	///Currently selected index in menu
	int selected_entry_index;
	///Satellite displayed on top of scrolled view
	int top_index;
	///Satellite displayed on bottom of scrolled view
	int bottom_index;
	///Number of satellites per scrolled view
	int displayed_entries_per_page;
	int window_row;
	///Height of display window
	int window_height;
	///Width of display window
	int window_width;
	///Display window
	WINDOW *window;
	///Header window (Satellite Azi Lat ...)
	WINDOW *header_window;
	///QTH coordinates
	predict_observer_t *qth;
	///Current number of satellites above horizon (displayed on top, first part of sorted mapping)
	int num_above_horizon;
	///Current number of satellites below the horizon (but will eventually rise) (displayed next, next part of sorted mapping)
	int num_below_horizon;
	///Number of satellites below max elevation threshold
	int num_below_threshold;
	///Number of satellites that will never be visible from the current QTH
	int num_nevervisible;
	///Number of decayed satellites (last part of menu listing, last part of sorted mapping)
	int num_decayed;
	///Mapping from indices in multitrack_entry_t array to indices in the TLE database array
	int *tle_db_mapping;
	///Submenu for selecting options on selected satellite
	multitrack_option_selector_t *option_selector;
	///Search field
	multitrack_search_field_t *search_field;
	///Current terminal height
	int terminal_height;
	///Current terminal width
	int terminal_width;
	///Sorting options
	int sort_option;
	///Max elevation threshold in degrees
	double max_elevation_threshold;
} multitrack_listing_t;

/**
 * Create multitrack satellite listing. Only satellites enabled within the TLE database are displayed.
 *
 * \param observer QTH coordinates
 * \param tle_db TLE database
 * \return Multitrack satellite listing
 **/
multitrack_listing_t* multitrack_create_listing(predict_observer_t *observer, struct tle_db *tle_db);

/**
 * Update satellite listing according to the `enabled`-flag within the TLE database (i.e. hide satellites that are disabled, show satellites that are enabled).
 *
 * \param listing Multitrack satellite listing
 * \param tle_db TLE database
 **/
void multitrack_refresh_tles(multitrack_listing_t *listing, struct tle_db *tle_db);

/**
 * Update satellite listing data.
 *
 * \param listing Multitrack satellite listing
 * \param time Time at which satellite listing should be calculated
 **/
void multitrack_update_listing_data(multitrack_listing_t *listing, predict_julian_date_t time);

/**
 * Print satellite listing and refresh associated windows.
 *
 * \param listing Satellite listing
 **/
void multitrack_display_listing(multitrack_listing_t *listing);

/**
 * Handle input keys to satellite listing. KEY_UP, KEY_DOWN, KEY_PPAGE and KEY_NPAGE are handled, and will move the menu
 * marker and scroll the satellite listing as neccessary.
 *
 * \param listing Satellite listing
 * \param input_key Input key
 * \return True if input key was handled, false otherwise
 **/
bool multitrack_handle_listing(multitrack_listing_t *listing, int input_key);

/**
 * Edit multitrack options. Will save to config file on exit.
 *
 * \param listing Multitrack listing
 **/
void multitrack_edit_settings(multitrack_listing_t *listing);

/**
 * Show help window.
 **/
void multitrack_show_help();

/**
 * Return currently selected entry, in terms of index in the TLE database.
 *
 * \param listing Satellite listing
 * \return TLE db index
 **/
int multitrack_selected_entry(multitrack_listing_t *listing);

/**
 * Destroy multitrack listing and free all associated memory.
 *
 * \param listing Satellite listing
 **/
void multitrack_destroy_listing(multitrack_listing_t **listing);

/**
 * Return currently selected entry, in terms of window row within the display window.
 *
 * \param listing Satellite index
 * \return Window row
 **/
int multitrack_selected_window_row(multitrack_listing_t *listing);

/**
 * Used for annotation of satellite submenu.
 **/
enum sub_menu_options {OPTION_SINGLETRACK, //run in single track mode
	OPTION_PREDICT, //predict passes
	OPTION_PREDICT_VISIBLE, //predict visible passes
	OPTION_DISPLAY_ORBITAL_DATA, //display orbital data
	OPTION_SOLAR_ILLUMINATION, //predict solar illumination
	OPTION_EDIT_TRANSPONDER}; //edit transponder database entry

/**
 * Get selected submenu option.
 *
 * \param option_selector Option selector
 * \return Option from `enum sub_menu_options`
 **/
int multitrack_option_selector_get_option(multitrack_option_selector_t *option_selector);

/**
 * Check if an option has been selected and clear it.
 *
 * \param option_selector Option selector
 * \return True if an option has been selected by the user, false otherwise
 **/
bool multitrack_option_selector_pop(multitrack_option_selector_t *option_selector);

/**
 * Check whether search field is visible.
 *
 * \param search_field Search field
 * \return True if search field is visible, false otherwise
 **/
bool multitrack_search_field_visible(multitrack_search_field_t *search_field);

#endif
