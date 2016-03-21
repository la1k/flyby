#ifndef MULTITRACK_H_DEFINED
#define MULTITRACK_H_DEFINED

#include "ncurses.h"
#include "form.h"
#include "menu.h"

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
	///Whether satellite currently is above horizon
	bool above_horizon;
	///Whether satellite is geostationary
	bool geostationary;
	///Whether satellite is never visible
	bool never_visible;
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
	WINDOW *window;
	FORM *form;
	FIELD **field;
	bool visible;
	int attributes;
	int match_num;
	int *matches;
	int num_matches;
	int available_match_size;
} multitrack_searcher_t;

/**
 * Satellite listing, used for providing a live overview of the current status of the satellites, and for
 * providing a selection menu for these satellites.
 **/
typedef struct {
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
	///Number of satellites that will never be visible from the current QTH
	int num_nevervisible;
	///Number of decayed satellites (last part of menu listing, last part of sorted mapping)
	int num_decayed;
	///Mapping from indices in multitrack_entry_t array to indices in the TLE database array
	int *tle_db_mapping;
	///Submenu for selecting options on selected satellite
	multitrack_option_selector_t *option_selector;
	///Search field
	multitrack_searcher_t *search_field;
} multitrack_listing_t;

///Row offset from window start at which to start printing
#define MULTITRACK_PRINT_OFFSET 0

/**
 * Create multitrack satellite listing. Only satellites enabled within the TLE database are displayed.
 *
 * \param window Display window
 * \param observer QTH coordinates
 * \param tle_db TLE database
 * \return Multitrack satellite listing
 **/
multitrack_listing_t* multitrack_create_listing(WINDOW *window, predict_observer_t *observer, struct tle_db *tle_db);

/**
 * Update satellite listing according to the `enabled`-flag within the TLE database (i.e. hide satellites that are disabled, show satellites that are enabled).
 *
 * \param listing Multitrack satellite listing
 * \param tle_db TLE database
 **/
void multitrack_refresh_tles(multitrack_listing_t *listing, struct tle_db *tle_db);

/**
 * Update satellite listing.
 *
 * \param listing Multitrack satellite listing
 * \param time Time at which satellite listing should be calculated
 **/
void multitrack_update_listing(multitrack_listing_t *listing, predict_julian_date_t time);

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

#endif
