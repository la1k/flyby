/**
 * Functions related to singletrack/real-time tracking of a single satellite.
 * Entry point is the function singletrack(), while
 * singletrack_track_satellite() does the bulk of the work. The rest of the
 * functions are mainly for separating tedious parts/separatable details into
 * separate and probably more readable units.
 *
 * Evolved from PREDICT's SingleTrack().
 **/

#include "singletrack.h"
#include <curses.h>
#include <ctype.h>

#include "defines.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"

/**
 * Get next enabled entry within the TLE database. Used for navigating between enabled satellites within singletrack().
 *
 * \param curr_index Current index
 * \param step Step used for finding next enabled entry (-1 or +1, preferably)
 * \param tle_db TLE database
 * \return Next entry in step direction which is enabled, or curr_index if none was found
 **/
int singletrack_get_next_enabled_satellite(int curr_index, int step, struct tle_db *tle_db);

/**
 * Display singletrack help window.
 **/
void singletrack_help();

///Height of main menu shown on bottom
#define SINGLETRACK_MAIN_MENU_HEIGHT 1

/**
 * Print major keybindings at the bottom of the window, htop style.
 *
 * \param window Window for printing
 **/
void singletrack_print_main_menu(WINDOW *window);

/**
 * Print general singletrack headers.
 *
 * \param satellite_name Satellite name
 * \param satellite_number Satellite number
 **/
void singletrack_print_headers(const char *satellite_name, long satellite_number);

/**
 * Print current observed satellite properties.
 *
 * \param orbit Orbit
 * \param obs Observation
 **/
void singletrack_print_satellite_properties(const struct predict_orbit *orbit, const struct predict_observation *obs);

/**
 * For specifying current satellite status without passing struct predict_observation all over the place.
 **/
enum satellite_status {
	SAT_STATUS_TCA, SAT_STATUS_APPROACHING, SAT_STATUS_RECEDING
};

/**
 * Link information, containing current link properties and user choices regarding rigctld control.
 **/
struct singletrack_link {
	///Chosen transponder
	struct transponder transponder;
	///Chosen uplink frequency from transponder
	double uplink;
	///Chosen downlink frequency to transponder
	double downlink;
	//Current doppler shifted downlink frequency
	double downlink_doppler;
	///Current doppler shifted uplink frequency
	double uplink_doppler;
	///Current delay
	double delay;
	///Current downlink loss
	double downlink_loss;
	///Current uplink loss
	double uplink_loss;
	///Status of satellite: receding, TCA, approaching
	enum satellite_status satellite_status;
	///Whether satellite is in range
	bool in_range;
	///Whether frequency should be read from rigctld
	bool readfreq;
	///Whether flyby should send downlink frequencies to rigctld
	bool downlink_update;
	///Whether flyby should send uplink frequencies to rigctld
	bool uplink_update;
};

/**
 * Set current transponder.
 *
 * \param transponder_entry Transponder database entry for tracked satellite
 * \param transponder_index Transponder to be chosen within the database entry
 * \param ret_transponder Returned link information
 **/
void singletrack_set_transponder(const struct sat_db_entry *transponder_entry, int transponder_index, struct singletrack_link *ret_transponder);

/**
 * Update link information based on observed satellite properties.
 *
 * \param observation Current satellite observation
 * \param link_status Link information to be updated
 **/
void singletrack_update_link_information(const struct predict_observation *observation, struct singletrack_link *link_status);

/**
 * Print transponder headers for link information printing.
 **/
void singletrack_print_transponder_headers();

/**
 * Print current uplink/downlink information.
 *
 * \param link_status Link information
 **/
void singletrack_print_link_information(const struct singletrack_link *link_status);

/**
 * Transponder keyboard input handling.
 *
 * \param link_status Link information
 * \param input_key Input key
 **/
void singletrack_handle_transponder_key(struct singletrack_link *link_status, int input_key);

/**
 * Calculates satellite properties and displays in real-time to screen. Controls rig and rotor controller through rotctld/rigctld interfaces when connected. Handles keyboard input. Handled and run from singletrack(...).
 *
 * \param satellite_name Satellite name
 * \param qth Ground station
 * \param orbital_elements Orbital elements for tracked satellite
 * \param satellite_transponders Satellite transponders
 * \param rotctld Rotctld connection
 * \param downlink_info Downlink rigctld connection
 * \param uplink_info Uplink rigctld connection
 **/
int singletrack_track_satellite(const char *satellite_name, predict_observer_t *qth, const predict_orbital_elements_t *orbital_elements, struct sat_db_entry satellite_transponders, rotctld_info_t *rotctld, rigctld_info_t *downlink_info, rigctld_info_t *uplink_info);

void singletrack(int orbit_ind, predict_observer_t *qth, struct transponder_db *sat_db, struct tle_db *tle_db, rotctld_info_t *rotctld, rigctld_info_t *downlink_info, rigctld_info_t *uplink_info)
{
	struct sat_db_entry *sat_db_entries = sat_db->sats;
	struct tle_db_entry *tle_db_entries = tle_db->tles;

	int     input_key;

	while (true) {
		predict_orbital_elements_t *orbital_elements = tle_db_entry_to_orbital_elements(tle_db, orbit_ind);
		const char *satellite_name = tle_db_entries[orbit_ind].name;
		struct sat_db_entry satellite_transponders = sat_db_entries[orbit_ind];

		//track satellite until keyboard input breaks the loop
		input_key = singletrack_track_satellite(satellite_name, qth, orbital_elements, satellite_transponders, rotctld, downlink_info, uplink_info);
		predict_destroy_orbital_elements(orbital_elements);

		//handle keyboard input not handled by singletrack_track_satellite(...):
		//track next satellite
		if ((input_key == KEY_LEFT) || (input_key == '-')) {
			orbit_ind = singletrack_get_next_enabled_satellite(orbit_ind, -1, tle_db);
		}

		//track previous satellite
		if ((input_key == KEY_RIGHT) || (input_key == '+')) {
			orbit_ind = singletrack_get_next_enabled_satellite(orbit_ind, +1, tle_db);
		}

		//quit
		if ((input_key=='q')
			|| (input_key == 'Q')
			|| (input_key == 27)) {
			break;
		}
	}
	cbreak();
}

int singletrack_get_next_enabled_satellite(int curr_index, int step, struct tle_db *tle_db)
{
	int index = curr_index;
	index += step;
	while ((index >= 0) && (index < tle_db->num_tles)) {
		if (tle_db_entry_enabled(tle_db, index)) {
			return index;
		}
		index += step;
	}
	return curr_index;
}

void singletrack_print_main_menu(WINDOW *window)
{
	int row = 0;
	int column = 0;

	column = print_main_menu_option(window, row, column, 'A', "Rotate to AOS position   ");
	column = print_main_menu_option(window, row, column, 'H', "Other keybindings        ");
	column = print_main_menu_option(window, row, column, 'Q', "Return                    ");

	wrefresh(window);
}

//Help window width
#define SINGLETRACK_HELP_WIDTH 79

//Key used for displaying help window
#define SINGLETRACK_HELP_KEY 'h'

//Row position of help window
#define SINGLETRACK_HELP_ROW 4

//Column position of help window
#define SINGLETRACK_HELP_COL 1

//Column for printing key string
#define SINGLETRACK_HELP_KEY_COL 1

//Column for printing key description
#define SINGLETRACK_HELP_DESC_COL 21

/**
 * Print singletrack keyhint using uniform formatting.
 *
 * \param window Print window
 * \param row Row number, is incremented by the function
 * \param key_str String specifying the key
 * \param desc_str String specifying the keybinding description
 **/
void singletrack_help_print_keyhint(WINDOW *window, int *row, const char *key_str, const char *desc_str)
{
	wattrset(window, COLOR_PAIR(3)|A_BOLD);
	mvwprintw(window, *row, SINGLETRACK_HELP_KEY_COL, key_str);
	mvwprintw(window, *row, SINGLETRACK_HELP_DESC_COL-2, ":");

	wattrset(window, COLOR_PAIR(1));

	//ensure nice linebreaks on long descriptions
	int desc_length = SINGLETRACK_HELP_WIDTH - SINGLETRACK_HELP_DESC_COL - 1;
	int print_length = strlen(desc_str);
	int start_pos = 0;
	while (print_length > 0) {
		char *temp_str = strdup(desc_str + start_pos);
		if (strlen(temp_str) > desc_length) {
			temp_str[desc_length] = '\0';
		}
		mvwprintw(window, *row, SINGLETRACK_HELP_DESC_COL, temp_str);
		free(temp_str);
		start_pos += desc_length;
		print_length -= desc_length;
		(*row)++;
	}
}

void singletrack_help()
{
	//prepare help window
	WINDOW *help_window = newwin(LINES, SINGLETRACK_HELP_WIDTH, SINGLETRACK_HELP_ROW, SINGLETRACK_HELP_COL);

	//print help information
	int row = 1;
	singletrack_help_print_keyhint(help_window, &row, "+/-", "Next/previous satellite");
	singletrack_help_print_keyhint(help_window, &row, "Key left/Key right", "---------- \"\" ---------");
	singletrack_help_print_keyhint(help_window, &row, "SPACE", "Next transponder");
	row++;
	singletrack_help_print_keyhint(help_window, &row, "Key down/key up", "Step through defined frequency range in 1 KHz steps");
	singletrack_help_print_keyhint(help_window, &row, "</>", "------------------------ \"\" -----------------------");
	singletrack_help_print_keyhint(help_window, &row, ",/.", "Step through defined frequency range in 100 Hz steps");
	row++;
	singletrack_help_print_keyhint(help_window, &row, "d/D", "Turn on/off downlink frequency updates to rigctld");
	singletrack_help_print_keyhint(help_window, &row, "u/U", "Turn on/off uplink frequency updates to rigctld");
	singletrack_help_print_keyhint(help_window, &row, "f", "Turn on downlink and uplink frequency updates to rigctld ");
	singletrack_help_print_keyhint(help_window, &row, "f/F", "Overwrite current uplink and downlink frequencies with   the current frequency in the rig (inversely doppler-     corrected)");
	singletrack_help_print_keyhint(help_window, &row, "m/M", "Turns on/off a continuous version of the above");
	singletrack_help_print_keyhint(help_window, &row, "x", "Reverse downlink and uplink VFO names");
	row++;
	row++;
	mvwprintw(help_window, row++, 1, "Press any key to continue");
	wresize(help_window, row+1, SINGLETRACK_HELP_WIDTH);
	wattrset(help_window, COLOR_PAIR(4));
	box(help_window, 0, 0);
	wrefresh(help_window);

	cbreak(); //turn off halfdelay mode so that getch blocks
	getch();

	delwin(help_window);
}

#define SATELLITE_GENERAL_DESC_ROW 17
#define SATELLITE_GENERAL_PROPS_ROW 18

void singletrack_print_headers(const char *satellite_name, long satellite_number)
{
	curs_set(0);
	bkgdset(COLOR_PAIR(3));
	clear();

	attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
	mvprintw(0,0,"                                                                                ");
	mvprintw(1,0,"  flyby Tracking:                                                               ");
	mvprintw(2,0,"                                                                                ");
	mvprintw(1,21,"%-24s (%d)", satellite_name, satellite_number);

	attrset(COLOR_PAIR(4)|A_BOLD);

	mvprintw(4,1,"Satellite     Direction     Velocity     Footprint    Altitude     Slant Range");

	mvprintw(5,1,"       N.            Az           mi            mi          mi              mi");
	mvprintw(6,1,"       E.            El           km            km          km              km");

	mvprintw(SATELLITE_GENERAL_DESC_ROW,1,"Eclipse Depth   Orbital Phase   Orbital Model   Squint Angle      AutoTracking");
}

#define SUNLIGHT_BASESTRING "Spacecraft is currently "

/**
 * Get string describing current satellite sunlight status.
 **/
const char *sunlight_status_string(const struct predict_orbit *orbit, const struct predict_observation *obs)
{
	if (obs->visible) {
		return SUNLIGHT_BASESTRING "visible    ";
	} else if (!(orbit->eclipsed)) {
		return SUNLIGHT_BASESTRING "in sunlight";
	} else {
		return SUNLIGHT_BASESTRING "in eclipse ";
	}
}

//row for printing satellite sunlight status
#define SUNLIGHT_STATUS_ROW 22

void singletrack_print_satellite_properties(const struct predict_orbit *orbit, const struct predict_observation *obs)
{
	double sat_vel = sqrt(pow(orbit->velocity[0], 2.0) + pow(orbit->velocity[1], 2.0) + pow(orbit->velocity[2], 2.0));
	attrset(COLOR_PAIR(2)|A_BOLD);
	mvprintw(5,1,"%-6.2f",orbit->latitude*180.0/M_PI);

	attrset(COLOR_PAIR(2)|A_BOLD);
	mvprintw(5,55,"%0.f ",orbit->altitude*KM_TO_MI);
	mvprintw(6,55,"%0.f ",orbit->altitude);
	mvprintw(5,68,"%-5.0f",obs->range*KM_TO_MI);
	mvprintw(6,68,"%-5.0f",obs->range);
	mvprintw(6,1,"%-7.2f",orbit->longitude*180.0/M_PI);
	mvprintw(5,15,"%-7.2f",obs->azimuth*180.0/M_PI);
	mvprintw(6,14,"%+-6.2f",obs->elevation*180.0/M_PI);
	mvprintw(5,29,"%0.f ",(3600.0*sat_vel)*KM_TO_MI);
	mvprintw(6,29,"%0.f ",3600.0*sat_vel);
	mvprintw(SATELLITE_GENERAL_PROPS_ROW,3,"%+6.2f deg",orbit->eclipse_depth*180.0/M_PI);
	mvprintw(SATELLITE_GENERAL_PROPS_ROW,20,"%5.1f",256.0*(orbit->phase/(2*M_PI)));
	mvprintw(5,42,"%0.f ",orbit->footprint*KM_TO_MI);
	mvprintw(6,42,"%0.f ",orbit->footprint);

	attrset(COLOR_PAIR(1)|A_BOLD);
	mvprintw(SUNLIGHT_STATUS_ROW,1,sunlight_status_string(orbit, obs));
}

///Scissored from libpredict for predict_doppler_shift_(...) below. FIXME: Remove after libpredict#75 merge.
#define SPEED_OF_LIGHT 299792458.0

/**
 * Modified version of predict_doppler_shift(...) until https://github.com/la1k/libpredict/issues/75 is
 * solved and merged. FIXME: Remove after merge.
 *
 * Calculates doppler shift.
 *
 * \param obs Satellite observation
 * \param frequency Frequency
 * \return Doppler shift
 **/
double predict_doppler_shift_(const struct predict_observation *obs, double frequency)
{
	double sat_range_rate = obs->range_rate*1000.0; //convert to m/s
	return -frequency*sat_range_rate/SPEED_OF_LIGHT; //assumes that sat_range <<<<< speed of light, which is very ok
}

/**
 * For specifying whether inverse_doppler_shift(...) should assume uplink or downlink frequency for inverse calculation.
 **/
enum dopp_shift_frequency_type {
	///Assume uplink frequency
	DOPP_UPLINK,
	///Assume downlink frequency
	DOPP_DOWNLINK
};

/**
 * Calculate what would be the original frequency when given a doppler shifted frequency.
 *
 * \param type Whether it is a downlink or uplink frequency (determines sign of doppler shift)
 * \param observation Observed orbit
 * \param doppler_shifted_frequency Input frequency
 * \return Original frequency
 **/
double inverse_doppler_shift(enum dopp_shift_frequency_type type, const struct predict_observation *observation, double doppler_shifted_frequency)
{
	int sign = 1;
	if (type == DOPP_UPLINK) {
		sign = -1;
	}
	return doppler_shifted_frequency/(1.0 + sign*predict_doppler_shift_(observation, 1));
}

void singletrack_set_transponder(const struct sat_db_entry *transponder_entry, int transponder_index, struct singletrack_link *ret_transponder)
{
	struct transponder transponder = transponder_entry->transponders[transponder_index];
	double downlink=0.5*(transponder.downlink_start+transponder.downlink_end);
	double uplink=0.5*(transponder.uplink_start+transponder.uplink_end);

	ret_transponder->transponder = transponder;
	ret_transponder->downlink = downlink;
	ret_transponder->uplink = uplink;
}

void singletrack_update_link_information(const struct predict_observation *observation, struct singletrack_link *link_status)
{
	link_status->delay=1000.0*((1000.0*observation->range)/299792458.0);
	link_status->downlink_loss=32.4+(20.0*log10(link_status->downlink))+(20.0*log10(observation->range));
	link_status->uplink_loss=32.4+(20.0*log10(link_status->uplink))+(20.0*log10(observation->range));
	link_status->downlink_doppler = link_status->downlink + predict_doppler_shift_(observation, link_status->downlink);
	link_status->uplink_doppler = link_status->uplink - predict_doppler_shift_(observation, link_status->uplink);
	link_status->in_range = observation->elevation >= 0;

	if (fabs(observation->range_rate) < 0.1) {
		link_status->satellite_status = SAT_STATUS_TCA;
	} else if (observation->range_rate < 0.0) {
		link_status->satellite_status = SAT_STATUS_APPROACHING;
	} else if (observation->range_rate > 0.0) {
		link_status->satellite_status = SAT_STATUS_RECEDING;
	}
}


/**
 * Get string corresponding to the satellite status (approaching, TCA, ...)
 *
 * \param satellite_status Satellite status
 * \return String containing "TCA", ...
 **/
const char *sat_status_string(enum satellite_status satellite_status)
{
	switch (satellite_status) {
		case SAT_STATUS_TCA:
			return "    TCA    ";
		case SAT_STATUS_APPROACHING:
			return "Approaching";
		case SAT_STATUS_RECEDING:
			return "  Receding ";
	}
	return NULL;
}

//defines at which rows transponder information will be displayed
#define TRANSPONDER_START_ROW 10

//rows and columns for transponder properties
#define TRANSPONDER_NAME_ROW TRANSPONDER_START_ROW
#define TRANSPONDER_UPLINK_ROW (TRANSPONDER_START_ROW+1)
#define TRANSPONDER_DOWNLINK_ROW (TRANSPONDER_UPLINK_ROW+1)
#define TRANSPONDER_GENERAL_ROW (TRANSPONDER_DOWNLINK_ROW+1)

#define TRANSPONDER_TXRX_DESC_COL 29
#define TRANSPONDER_PATHLOSS_DESC_COL 57
#define TRANSPONDER_START_COL 1
#define TRANSPONDER_VFO_COL (TRANSPONDER_PATHLOSS_DESC_COL - 8)

#define TRANSPONDER_FREQ_COL (TRANSPONDER_START_COL+10)
#define TRANSPONDER_DELAY_COL TRANSPONDER_FREQ_COL
#define TRANSPONDER_DOPP_COL (TRANSPONDER_TXRX_DESC_COL + 3)
#define TRANSPONDER_LOSS_COL (TRANSPONDER_PATHLOSS_DESC_COL + 12)
#define TRANSPONDER_ECHO_COL TRANSPONDER_LOSS_COL
#define TRANSPONDER_STATUS_COL 34

void singletrack_print_transponder_headers()
{
	attrset(COLOR_PAIR(4)|A_BOLD);
	mvprintw(TRANSPONDER_UPLINK_ROW,TRANSPONDER_START_COL,"Uplink   :");
	mvprintw(TRANSPONDER_DOWNLINK_ROW,TRANSPONDER_START_COL,"Downlink :");
	mvprintw(TRANSPONDER_GENERAL_ROW,TRANSPONDER_START_COL,"Delay    :");
	mvprintw(TRANSPONDER_GENERAL_ROW,TRANSPONDER_PATHLOSS_DESC_COL,"Echo      :");
	mvprintw(TRANSPONDER_DOWNLINK_ROW,TRANSPONDER_TXRX_DESC_COL,"RX:");
	mvprintw(TRANSPONDER_DOWNLINK_ROW,TRANSPONDER_PATHLOSS_DESC_COL,"Path loss :");
	mvprintw(TRANSPONDER_UPLINK_ROW,TRANSPONDER_TXRX_DESC_COL,"TX:");
	mvprintw(TRANSPONDER_UPLINK_ROW,TRANSPONDER_PATHLOSS_DESC_COL,"Path loss :");
}


/**
 * Functions for clearing transponder fields.
 **/

void singletrack_clear_downlink_dynamic_fields()
{
	mvprintw(TRANSPONDER_DOWNLINK_ROW,TRANSPONDER_DOPP_COL,"                ");
	mvprintw(TRANSPONDER_DOWNLINK_ROW,TRANSPONDER_LOSS_COL,"          ");
	mvprintw(TRANSPONDER_DOWNLINK_ROW,TRANSPONDER_VFO_COL,"       ");
}

void singletrack_clear_downlink_fields()
{
	singletrack_clear_downlink_dynamic_fields();
	mvprintw(TRANSPONDER_DOWNLINK_ROW,TRANSPONDER_FREQ_COL,"                  ");
}

void singletrack_clear_uplink_dynamic_fields()
{
	mvprintw(TRANSPONDER_UPLINK_ROW,TRANSPONDER_DOPP_COL,"                ");
	mvprintw(TRANSPONDER_UPLINK_ROW,TRANSPONDER_LOSS_COL,"          ");
	mvprintw(TRANSPONDER_UPLINK_ROW,TRANSPONDER_VFO_COL,"       ");
}

void singletrack_clear_uplink_fields()
{
	singletrack_clear_uplink_dynamic_fields();
	mvprintw(TRANSPONDER_UPLINK_ROW,TRANSPONDER_FREQ_COL,"                  ");
}

void singletrack_clear_delay_field()
{
	mvprintw(TRANSPONDER_GENERAL_ROW,TRANSPONDER_DELAY_COL,"              ");
}

void singletrack_clear_echo_field()
{
	mvprintw(TRANSPONDER_GENERAL_ROW,TRANSPONDER_ECHO_COL,"              ");
}

void singletrack_clear_status_field()
{
	mvprintw(TRANSPONDER_GENERAL_ROW,TRANSPONDER_STATUS_COL,"            ");
}

void singletrack_clear_name_field()
{
	mvprintw(TRANSPONDER_START_ROW,0,"                                                                                ");
}

void singletrack_print_link_information(const struct singletrack_link *link_status)
{
	attrset(COLOR_PAIR(2)|A_BOLD);
	double downlink = link_status->downlink;
	double uplink = link_status->uplink;
	bool readfreq = link_status->readfreq;
	bool downlink_update = link_status->downlink_update;
	bool uplink_update = link_status->uplink_update;

	//print general transponder properties
	int length=strlen(link_status->transponder.name)/2;
	singletrack_clear_name_field();
	mvprintw(TRANSPONDER_NAME_ROW,40-length,"%s",link_status->transponder.name);

	//current downlink user choices
	if (downlink!=0.0)
		mvprintw(TRANSPONDER_DOWNLINK_ROW,TRANSPONDER_FREQ_COL,"%11.5f MHz%c%c%c",downlink,
		readfreq ? '<' : ' ',
		(readfreq || downlink_update) ? '=' : ' ',
		downlink_update ? '>' : ' ');

	else {
		singletrack_clear_downlink_fields();
	}

	//current uplink user choices
	if (uplink!=0.0)
		mvprintw(TRANSPONDER_UPLINK_ROW,TRANSPONDER_FREQ_COL,"%11.5f MHz%c%c%c",uplink,
		readfreq ? '<' : ' ',
		(readfreq || uplink_update) ? '=' : ' ',
		uplink_update ? '>' : ' ');

	else {
		singletrack_clear_uplink_fields();
	}

	//calculate and display downlink/uplink information during pass
	if (link_status->in_range) {
		//current satellite status: approaching, receding, ...
		attrset(COLOR_PAIR(4)|A_BOLD);
		mvprintw(TRANSPONDER_GENERAL_ROW, TRANSPONDER_STATUS_COL, sat_status_string(link_status->satellite_status));

		//downlink information
		attrset(COLOR_PAIR(2)|A_BOLD);
		if (downlink!=0.0) {
			mvprintw(TRANSPONDER_DOWNLINK_ROW,TRANSPONDER_DOPP_COL,"%11.5f MHz",link_status->downlink_doppler);
			mvprintw(TRANSPONDER_DOWNLINK_ROW,TRANSPONDER_LOSS_COL,"%7.3f dB",link_status->downlink_loss);
			mvprintw(TRANSPONDER_GENERAL_ROW,TRANSPONDER_DELAY_COL,"%11.5f ms",link_status->delay);
		} else {
			singletrack_clear_delay_field();
			singletrack_clear_downlink_fields();
		}

		//uplink information
		if (uplink!=0.0) {
			mvprintw(TRANSPONDER_UPLINK_ROW,TRANSPONDER_DOPP_COL,"%11.5f MHz",link_status->uplink_doppler);
			mvprintw(TRANSPONDER_UPLINK_ROW,TRANSPONDER_LOSS_COL,"%7.3f dB",link_status->uplink_loss);
		} else {
			singletrack_clear_uplink_fields();
		}

		//echo if both uplink and downlink defined
		if (uplink!=0.0 && downlink!=0.0) {
			double echo = 2.0*link_status->delay;
			mvprintw(TRANSPONDER_GENERAL_ROW,TRANSPONDER_ECHO_COL,"%7.3f ms",echo);
		} else {
			singletrack_clear_echo_field();
		}
	} else {
		singletrack_clear_echo_field();
		singletrack_clear_downlink_dynamic_fields();
		singletrack_clear_uplink_dynamic_fields();
		singletrack_clear_delay_field();
		singletrack_clear_status_field();
	}
}

void singletrack_handle_transponder_key(struct singletrack_link *link_status, int input_key)
{
	double shift = 0;

	//raise frequency
	if (input_key==KEY_UP || input_key=='>' || input_key=='.') {
		if (input_key==KEY_UP || input_key=='>')
			shift=0.001;  /* 1 kHz */
		else
			shift=0.0001; /* 100 Hz */
	}

	//lower frequency
	if (input_key==KEY_DOWN || input_key=='<' || input_key== ',') {
		if (input_key==KEY_DOWN || input_key=='<')
			shift=-0.001;  /* 1 kHz */
		else
			shift=-0.0001; /* 100 Hz */
	}

	//calculate which direction uplink and downlink frequencies should be raised/lowered
	int polarity = 0;
	struct transponder transponder = link_status->transponder;
	if (transponder.downlink_start>transponder.downlink_end) {
		polarity=-1;
	} else if (transponder.downlink_start<transponder.downlink_end) {
		polarity=1;
	}

	//adjust uplink/downlink frequencies
	link_status->uplink+=shift*(double)abs(polarity);
	link_status->downlink+=(shift*(double)polarity);
	if (link_status->uplink < transponder.uplink_start) {
		link_status->uplink=transponder.uplink_end;
		link_status->downlink=transponder.downlink_end;
	}
	if (link_status->uplink > transponder.uplink_end) {
		link_status->uplink=transponder.uplink_start;
		link_status->downlink=transponder.downlink_start;
	}

	//turn on downlink updates
	if (input_key=='d')
		link_status->downlink_update=true;

	//turn off downlink updates
	if (input_key=='D')
		link_status->downlink_update=false;

	//turn on uplink update
	if (input_key=='u')
		link_status->uplink_update=true;

	//turn off uplink update
	if (input_key=='U')
		link_status->uplink_update=false;

	//turn on both uplink and downlink update
	if (input_key=='f')
	{
		link_status->downlink_update=true;
		link_status->uplink_update=true;
	}

	//read frequency from rig
	if (input_key=='m')
		link_status->readfreq=true;

	//turn off frequency reading from rig
	if (input_key=='M')
		link_status->readfreq=false;
}

///Radians to degrees conversion factor
#define RAD2DEG (180.0/M_PI)

///Row at which aoslos information is printed
#define AOSLOS_INFORMATION_ROW 20

//Row at which max elevation information is printed
#define MAXELE_INFORMATION_ROW (AOSLOS_INFORMATION_ROW+1)

//distance between moon, sun and qth boxes
#define SUN_MOON_COLUMN_DIFF 12

//row at which to print sun and moon boxes
#define SUN_MOON_ROW 20

//column for sun
#define SUN_COLUMN 46

//column for moon
#define MOON_COLUMN (SUN_COLUMN + SUN_MOON_COLUMN_DIFF)

//row for QTH box
#define QTH_ROW SUN_MOON_ROW

//column for QTH box
#define QTH_COLUMN (MOON_COLUMN + SUN_MOON_COLUMN_DIFF)

int singletrack_track_satellite(const char *satellite_name, predict_observer_t *qth, const predict_orbital_elements_t *orbital_elements, struct sat_db_entry satellite_transponders, rotctld_info_t *rotctld, rigctld_info_t *downlink_info, rigctld_info_t *uplink_info)
{
	int input_key;
	int    transponder_index=0;
	struct singletrack_link link_status;
	link_status.downlink_update = true;
	link_status.uplink_update = true;
	link_status.readfreq = false;

	struct predict_observation aos = {0};
	struct predict_observation los = {0};
	struct predict_observation max_elevation = {0};

	char ephemeris_string[MAX_NUM_CHARS];

	char time_string[MAX_NUM_CHARS];

	struct predict_orbit orbit;

	switch (orbital_elements->ephemeris) {
		case EPHEMERIS_SGP4:
			strcpy(ephemeris_string, "SGP4");
		break;
		case EPHEMERIS_SDP4:
			strcpy(ephemeris_string, "SDP4");
		break;
		case EPHEMERIS_SGP8:
			strcpy(ephemeris_string, "SGP8");
		break;
		case EPHEMERIS_SDP8:
			strcpy(ephemeris_string, "SDP8");
		break;
	}

	bool comsat = satellite_transponders.num_transponders > 0;

	if (comsat) {
		singletrack_set_transponder(&satellite_transponders, transponder_index, &link_status);
	}

	bool aos_happens = predict_aos_happens(orbital_elements, qth->latitude);
	bool geostationary = predict_is_geostationary(orbital_elements);

	predict_julian_date_t daynum = predict_to_julian(time(NULL));
	predict_orbit(orbital_elements, &orbit, daynum);
	bool decayed = orbit.decayed;

	halfdelay(HALF_DELAY_TIME);

	//print static description fields
	singletrack_print_headers(satellite_name, orbital_elements->satellite_number);

	//print QTH box
	print_qth_box(QTH_ROW, QTH_COLUMN, qth);

	if (comsat) {
		singletrack_print_transponder_headers();
	}

	//window for printing main menu options
	WINDOW *main_menu_win = newwin(SINGLETRACK_MAIN_MENU_HEIGHT, COLS, LINES-SINGLETRACK_MAIN_MENU_HEIGHT, 0);
	singletrack_print_main_menu(main_menu_win);
	refresh();

	while (true) {
		//predict and observe satellite orbit
		time_t epoch = time(NULL);
		daynum = predict_to_julian(epoch);
		predict_orbit(orbital_elements, &orbit, daynum);
		struct predict_observation obs;
		predict_observe_orbit(qth, &orbit, &obs);
		double squint = predict_squint_angle(qth, &orbit, satellite_transponders.alon, satellite_transponders.alat);

		//update pass information
		if (!decayed && aos_happens && !geostationary && (daynum > los.time)) {
			struct predict_orbit temp_orbit;

			//aos of next pass
			predict_orbit(orbital_elements, &temp_orbit, predict_next_aos(qth, orbital_elements, daynum));
			predict_observe_orbit(qth, &temp_orbit, &aos);

			//los of current or next pass
			predict_orbit(orbital_elements, &temp_orbit, predict_next_los(qth, orbital_elements, daynum));
			predict_observe_orbit(qth, &temp_orbit, &los);

			//max elevation of current or next pass
			max_elevation = predict_at_max_elevation(qth, orbital_elements, daynum);
		}

		//display current time
		attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
		strftime(time_string, MAX_NUM_CHARS, "%a %d%b%y %j.%H:%M:%S", gmtime(&epoch));
		mvprintw(1,54,"%s",time_string);

		//display satellite data
		singletrack_print_satellite_properties(&orbit, &obs);
		attrset(COLOR_PAIR(2)|A_BOLD);
		mvprintw(SATELLITE_GENERAL_PROPS_ROW,37,"%s",ephemeris_string);
		if (satellite_transponders.squintflag) {
			mvprintw(SATELLITE_GENERAL_PROPS_ROW,52,"%+6.2f",squint);
		} else {
			mvprintw(SATELLITE_GENERAL_PROPS_ROW,52,"N/A");
		}

		//display AOS/LOS times
		attrset(COLOR_PAIR(1)|A_BOLD);
		if (geostationary && (obs.elevation>=0.0)) {
			mvprintw(AOSLOS_INFORMATION_ROW,1,"Satellite orbit is geostationary");
		} else if (decayed || !aos_happens || (geostationary && (obs.elevation<0.0))){
			mvprintw(AOSLOS_INFORMATION_ROW,1,"This satellite never reaches AOS");
		} else if (obs.elevation >= 0.0) {
			time_t epoch = predict_from_julian(los.time);
			strftime(time_string, MAX_NUM_CHARS, "%H:%M:%S", gmtime(&epoch));
			mvprintw(AOSLOS_INFORMATION_ROW,1,"LOS at:   %s UTC (%0.f Az)   ",time_string,los.azimuth*RAD2DEG);
		} else if (obs.elevation < 0.0) {
			time_t epoch = predict_from_julian(aos.time);
			strftime(time_string, MAX_NUM_CHARS, "%H:%M:%S", gmtime(&epoch));
			mvprintw(AOSLOS_INFORMATION_ROW,1,"Next AOS: %s UTC (%0.f Az)   ",time_string, aos.azimuth*RAD2DEG);
		}

		//display max elevation information
		if (!geostationary && !decayed && aos_happens) {
			//max elevation time
			time_t epoch = predict_from_julian(max_elevation.time);
			char time_string[MAX_NUM_CHARS];
			strftime(time_string, MAX_NUM_CHARS, "%H:%M:%S UTC", gmtime(&epoch));

			//pass properties
			mvprintw(MAXELE_INFORMATION_ROW, 1, "Max ele   %s (%0.f Az, %2.f El)", time_string, max_elevation.azimuth*RAD2DEG, max_elevation.elevation*RAD2DEG);
		}

		//predict and observe sun and moon
		print_sun_box(SUN_MOON_ROW, SUN_COLUMN, qth, daynum);
		print_moon_box(SUN_MOON_ROW, MOON_COLUMN, qth, daynum);

		//display downlink/uplink information
		if (comsat) {
			//set downlink/uplink from rig on readfreq option
			if (downlink_info->connected && link_status.readfreq) {
				double frequency;
				rigctld_fail_on_errors(rigctld_read_frequency(downlink_info, &frequency));
				link_status.downlink = inverse_doppler_shift(DOPP_DOWNLINK, &obs, frequency);
			}
			if (uplink_info->connected && link_status.readfreq) {
				double frequency;
				rigctld_fail_on_errors(rigctld_read_frequency(uplink_info, &frequency));
				link_status.uplink = inverse_doppler_shift(DOPP_UPLINK, &obs, frequency);
			}

			//update link information from current satellite data
			singletrack_update_link_information(&obs, &link_status);

			//print link information to screen
			singletrack_print_link_information(&link_status);

			//print VFO names
			if (downlink_info->connected && (link_status.downlink != 0.0) && (link_status.in_range) && (strlen(downlink_info->vfo_name) > 0)) {
				mvprintw(TRANSPONDER_DOWNLINK_ROW, TRANSPONDER_VFO_COL, "(%s)", downlink_info->vfo_name);
			}
			if (uplink_info->connected && (link_status.uplink != 0.0) && (link_status.in_range) && (strlen(uplink_info->vfo_name) > 0)) {
				mvprintw(TRANSPONDER_UPLINK_ROW, TRANSPONDER_VFO_COL, "(%s)", uplink_info->vfo_name);
			}

			//set doppler-shifted downlink/uplink to rig
			if (link_status.in_range && downlink_info->connected && link_status.downlink_update && (link_status.downlink != 0.0)) {
				rigctld_fail_on_errors(rigctld_set_frequency(downlink_info, link_status.downlink_doppler));
			}
			if (link_status.in_range && uplink_info->connected && link_status.uplink_update && (link_status.uplink != 0.0)) {
				rigctld_fail_on_errors(rigctld_set_frequency(uplink_info, link_status.uplink_doppler));
			}
		}

		//display rotation information
		if (rotctld->connected) {
			if (obs.elevation>=rotctld->tracking_horizon)
				mvprintw(SATELLITE_GENERAL_PROPS_ROW,67,"   Active   ");
			else
				mvprintw(SATELLITE_GENERAL_PROPS_ROW,67,"Standing  By");
		} else
			mvprintw(SATELLITE_GENERAL_PROPS_ROW,67,"Not  Enabled");


		//send data to rotctld
		if ((obs.elevation*180.0/M_PI >= rotctld->tracking_horizon) && rotctld->connected) {
			rotctld_fail_on_errors(rotctld_track(rotctld, obs.azimuth*180.0/M_PI, obs.elevation*180.0/M_PI));
		}

		singletrack_print_main_menu(main_menu_win);

		//handle keyboard input
		input_key=getch();

		//move antenna towards AOS position
		if ((input_key == 'A') && (obs.elevation*180.0/M_PI < rotctld->tracking_horizon) && rotctld->connected) {
			rotctld_fail_on_errors(rotctld_track(rotctld, aos.azimuth*180.0/M_PI, 0));
		}

		if (comsat && (input_key != ERR)) {
			//get next transponder
			if (input_key==' ' && satellite_transponders.num_transponders>1) {
				transponder_index++;

				if (transponder_index>=satellite_transponders.num_transponders)
					transponder_index=0;

				singletrack_set_transponder(&satellite_transponders, transponder_index, &link_status);
			}

			//handle transponder key input
			singletrack_handle_transponder_key(&link_status, input_key);
		}

		//read frequency once from rig
		if (input_key=='f' || input_key=='F')
		{
			if (downlink_info->connected) {
				double frequency;
				rigctld_fail_on_errors(rigctld_read_frequency(downlink_info, &frequency));
				link_status.downlink = inverse_doppler_shift(DOPP_DOWNLINK, &obs, frequency);
			}
			if (uplink_info->connected) {
				double frequency;
				rigctld_fail_on_errors(rigctld_read_frequency(uplink_info, &frequency));
				link_status.uplink = inverse_doppler_shift(DOPP_UPLINK, &obs, frequency);
			}
		}

		//reverse VFO uplink and downlink names
		if ((input_key=='x') && (downlink_info->connected) && (uplink_info->connected)) {
			char tmp_vfo[MAX_NUM_CHARS];
			strncpy(tmp_vfo, downlink_info->vfo_name, MAX_NUM_CHARS);
			strncpy(downlink_info->vfo_name, uplink_info->vfo_name, MAX_NUM_CHARS);
			strncpy(uplink_info->vfo_name, tmp_vfo, MAX_NUM_CHARS);
		}

		refresh();

		//display help
		if (tolower(input_key) == SINGLETRACK_HELP_KEY) {
			singletrack_help();
		}

		halfdelay(HALF_DELAY_TIME);

		//quit function and return input key
		if ((input_key=='q')
			|| (input_key == 'Q')
			|| (input_key == 27) //escape
			|| (input_key == '+')
			|| (input_key == '-')
			|| (input_key == KEY_LEFT)
			|| (input_key == KEY_RIGHT)
			|| (tolower(input_key) == SINGLETRACK_HELP_KEY)) {
			break;
		}
	}
	return input_key;

}
