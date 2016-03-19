typedef struct {
	char *name;
	predict_orbital_elements_t *orbital_elements;

	double next_aos;
	double next_los;

	bool above_horizon;
	bool geostationary;
	bool never_visible;
	bool decayed;

	char display_string[MAX_NUM_CHARS];
	int display_attributes;
} multitrack_entry_t;

typedef struct {
	int selected_entry_index;
	int window_height;
	int window_width;
	WINDOW *window;

	int num_entries;

	int num_displayed_entries;
	int top_index;
	int bottom_index;

	predict_observer_t *qth;
	multitrack_entry_t **entries;
	int *sorted_index;
	int num_above_horizon;
	int num_below_horizon;
	int num_decayed;
	int num_nevervisible;

	int *tle_db_mapping;
} multitrack_listing_t;

#define SELECTED_ATTRIBUTE (COLOR_PAIR(6)|A_REVERSE)
#define SELECTED_MARKER '-'

multitrack_entry_t *multitrack_create_entry(const char *name, predict_orbital_elements_t *orbital_elements);

multitrack_listing_t* multitrack_create_listing(WINDOW *window, predict_observer_t *observer, predict_orbital_elements_t **orbital_elements, struct tle_db *tle_db);

void multitrack_refresh_tles(multitrack_listing_t *listing, predict_orbital_elements_t **orbital_elements, struct tle_db *tle_db);

void multitrack_update_entry(predict_observer_t *qth, multitrack_entry_t *entry, predict_julian_date_t time);

void multitrack_update_listing(multitrack_listing_t *listing, predict_julian_date_t time);

void multitrack_sort_listing(multitrack_listing_t *listing);

void multitrack_display_listing(multitrack_listing_t *listing);

bool multitrack_handle_listing(multitrack_listing_t *listing, int input_key);

int multitrack_selected_entry(multitrack_listing_t *listing);

int multitrack_selected_window_row(multitrack_listing_t *listing);
