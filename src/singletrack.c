#include "singletrack.h"
#include <curses.h>

#include "defines.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/**
 * Get next enabled entry within the TLE database. Used for navigating between enabled satellites within singletrack().
 *
 * \param curr_index Current index
 * \param step Step used for finding next enabled entry (-1 or +1, preferably)
 * \param tle_db TLE database
 * \return Next entry in step direction which is enabled, or curr_index if none was found
 **/
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

//Help window width
#define SINGLETRACK_HELP_WIDTH 80

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

/**
 * Display singletrack help window.
 **/
void singletrack_help()
{
	//prepare help window
	WINDOW *help_window = newwin(LINES, SINGLETRACK_HELP_WIDTH, SINGLETRACK_HELP_ROW, SINGLETRACK_HELP_COL);

	//print help information
	int row = 1;
	singletrack_help_print_keyhint(help_window, &row, "q/ESC", "Escape single track mode");
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
	singletrack_help_print_keyhint(help_window, &row, "f/F", "Overwrite current uplink and downlink frequencies with thecurrent frequency in the rig (inversely doppler-corrected)");
	singletrack_help_print_keyhint(help_window, &row, "m/M", "Turns on/off a continuous version of the above");
	singletrack_help_print_keyhint(help_window, &row, "x", "Reverse downlink and uplink VFO names");
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

enum dopp_shift_frequency_type {
	DOPP_UPLINK,
	DOPP_DOWNLINK
};

/**
 * Calculate what would be the original frequency when given a doppler shifted frequency.
 *
 * \param type Whether it is a downlink or uplink frequency (determines sign of doppler shift)
 * \param observer Observer
 * \param orbit Predicted orbit
 * \param doppler_shifted_frequency Input frequency
 * \return Original frequency
 **/
double inverse_doppler_shift(enum dopp_shift_frequency_type type, const predict_observer_t *observer, const struct predict_orbit *orbit, double doppler_shifted_frequency)
{
	int sign = 1;
	if (type == DOPP_UPLINK) {
		sign = -1;
	}
	return doppler_shifted_frequency/(1.0 + sign*predict_doppler_shift(observer, orbit, 1));
}

//defines at which rows transponder information will be displayed
#define TRANSPONDER_START_ROW 10

#define RAD2DEG (180.0/M_PI)

#define AOSLOS_INFORMATION_ROW 20

void singletrack(int orbit_ind, predict_observer_t *qth, struct transponder_db *sat_db, struct tle_db *tle_db, rotctld_info_t *rotctld, rigctld_info_t *downlink_info, rigctld_info_t *uplink_info)
{
	double horizon = rotctld->tracking_horizon;

	struct sat_db_entry *sat_db_entries = sat_db->sats;
	struct tle_db_entry *tle_db_entries = tle_db->tles;

	int     ans;
	bool	downlink_update=true, uplink_update=true, readfreq=false;

	do {
		int     length, xponder=0,
			polarity=0;
		bool	aos_alarm=0;
		double	downlink=0.0, uplink=0.0, downlink_start=0.0,
			downlink_end=0.0, uplink_start=0.0, uplink_end=0.0;

		struct predict_observation aos = {0};
		struct predict_observation los = {0};
		struct predict_observation max_elevation = {0};

		double delay;
		double loss;

		//elevation and azimuth at previous timestep, for checking when to send messages to rotctld
		int prev_elevation = 0;
		int prev_azimuth = 0;
		time_t prev_time = 0;

		char ephemeris_string[MAX_NUM_CHARS];

		char time_string[MAX_NUM_CHARS];

		predict_orbital_elements_t *orbital_elements = tle_db_entry_to_orbital_elements(tle_db, orbit_ind);
		struct predict_orbit orbit;
		struct sat_db_entry sat_db = sat_db_entries[orbit_ind];

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

		bool comsat = sat_db.num_transponders > 0;

		if (comsat) {
			downlink_start=sat_db.transponders[xponder].downlink_start;
			downlink_end=sat_db.transponders[xponder].downlink_end;
			uplink_start=sat_db.transponders[xponder].uplink_start;
			uplink_end=sat_db.transponders[xponder].uplink_end;

			if (downlink_start>downlink_end)
				polarity=-1;

			if (downlink_start<downlink_end)
				polarity=1;

			if (downlink_start==downlink_end)
				polarity=0;

			downlink=0.5*(downlink_start+downlink_end);
			uplink=0.5*(uplink_start+uplink_end);
		} else {
			downlink_start=0.0;
			downlink_end=0.0;
			uplink_start=0.0;
			uplink_end=0.0;
			polarity=0;
			downlink=0.0;
			uplink=0.0;
		}

		bool aos_happens = predict_aos_happens(orbital_elements, qth->latitude);
		bool geostationary = predict_is_geostationary(orbital_elements);

		predict_julian_date_t daynum = predict_to_julian(time(NULL));
		predict_orbit(orbital_elements, &orbit, daynum);
		bool decayed = orbit.decayed;

		halfdelay(HALF_DELAY_TIME);
		curs_set(0);
		bkgdset(COLOR_PAIR(3));
		clear();

		attrset(COLOR_PAIR(6)|A_REVERSE|A_BOLD);
		mvprintw(0,0,"                                                                                ");
		mvprintw(1,0,"  flyby Tracking:                                                               ");
		mvprintw(2,0,"                                                                                ");
		mvprintw(1,21,"%-24s (%d)", tle_db_entries[orbit_ind].name, orbital_elements->satellite_number);

		attrset(COLOR_PAIR(4)|A_BOLD);

		mvprintw(4,1,"Satellite     Direction     Velocity     Footprint    Altitude     Slant Range");

		mvprintw(5,1,"        .            Az           mi            mi          mi              mi");
		mvprintw(6,1,"        .            El           km            km          km              km");

		mvprintw(17,1,"Eclipse Depth   Orbital Phase   Orbital Model   Squint Angle      AutoTracking");

		if (comsat) {
			mvprintw(TRANSPONDER_START_ROW+1,1,"Uplink   :");
			mvprintw(TRANSPONDER_START_ROW+2,1,"Downlink :");
			mvprintw(TRANSPONDER_START_ROW+3,1,"Delay    :");
			mvprintw(TRANSPONDER_START_ROW+3,55,"Echo      :");
			mvprintw(TRANSPONDER_START_ROW+2,29,"RX:");
			mvprintw(TRANSPONDER_START_ROW+2,55,"Path loss :");
			mvprintw(TRANSPONDER_START_ROW+1,29,"TX:");
			mvprintw(TRANSPONDER_START_ROW+1,55,"Path loss :");
		}

		do {
			if (downlink_info->connected && readfreq) {
				downlink = inverse_doppler_shift(DOPP_DOWNLINK, qth, &orbit, rigctld_read_frequency(downlink_info));
			}
			if (uplink_info->connected && readfreq) {
				uplink = inverse_doppler_shift(DOPP_UPLINK, qth, &orbit, rigctld_read_frequency(uplink_info));
			}


			//predict and observe satellite orbit
			time_t epoch = time(NULL);
			daynum = predict_to_julian(epoch);
			predict_orbit(orbital_elements, &orbit, daynum);
			struct predict_observation obs;
			predict_observe_orbit(qth, &orbit, &obs);
			double sat_vel = sqrt(pow(orbit.velocity[0], 2.0) + pow(orbit.velocity[1], 2.0) + pow(orbit.velocity[2], 2.0));
			double squint = predict_squint_angle(qth, &orbit, sat_db.alon, sat_db.alat);

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

			attrset(COLOR_PAIR(4)|A_BOLD);
			mvprintw(5,8,"N");
			mvprintw(6,8,"E");

			//display satellite data
			attrset(COLOR_PAIR(2)|A_BOLD);
			mvprintw(5,1,"%-6.2f",orbit.latitude*180.0/M_PI);

			attrset(COLOR_PAIR(2)|A_BOLD);
			mvprintw(5,55,"%0.f ",orbit.altitude*KM_TO_MI);
			mvprintw(6,55,"%0.f ",orbit.altitude);
			mvprintw(5,68,"%-5.0f",obs.range*KM_TO_MI);
			mvprintw(6,68,"%-5.0f",obs.range);
			mvprintw(6,1,"%-7.2f",orbit.longitude*180.0/M_PI);
			mvprintw(5,15,"%-7.2f",obs.azimuth*180.0/M_PI);
			mvprintw(6,14,"%+-6.2f",obs.elevation*180.0/M_PI);
			mvprintw(5,29,"%0.f ",(3600.0*sat_vel)*KM_TO_MI);
			mvprintw(6,29,"%0.f ",3600.0*sat_vel);
			mvprintw(18,3,"%+6.2f deg",orbit.eclipse_depth*180.0/M_PI);
			mvprintw(18,20,"%5.1f",256.0*(orbit.phase/(2*M_PI)));
			mvprintw(18,37,"%s",ephemeris_string);
			if (sat_db.squintflag) {
				mvprintw(18,52,"%+6.2f",squint);
			} else {
				mvprintw(18,52,"N/A");
			}
			mvprintw(5,42,"%0.f ",orbit.footprint*KM_TO_MI);
			mvprintw(6,42,"%0.f ",orbit.footprint);

			attrset(COLOR_PAIR(1)|A_BOLD);
			mvprintw(22,1,"Spacecraft is currently ");
			if (obs.visible) {
				mvprintw(22,25,"visible    ");
			} else if (!(orbit.eclipsed)) {
				mvprintw(22,25,"in sunlight");
			} else {
				mvprintw(22,25,"in eclipse ");
			}

			//display AOS/LOS times
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

			//display pass information
			if (!geostationary && !decayed && aos_happens) {
				//max elevation time
				time_t epoch = predict_from_julian(max_elevation.time);
				char time_string[MAX_NUM_CHARS];
				strftime(time_string, MAX_NUM_CHARS, "%H:%M:%S UTC", gmtime(&epoch));

				//pass properties
				mvprintw(AOSLOS_INFORMATION_ROW+1, 1, "Max ele   %s (%0.f Az, %2.f El)", time_string, max_elevation.azimuth*RAD2DEG, max_elevation.elevation*RAD2DEG);
			}

			//predict and observe sun and moon
			struct predict_observation sun;
			predict_observe_sun(qth, daynum, &sun);

			struct predict_observation moon;
			predict_observe_moon(qth, daynum, &moon);

			//display sun and moon
			attrset(COLOR_PAIR(4)|A_REVERSE|A_BOLD);
			mvprintw(20,55,"   Sun   ");
			mvprintw(20,70,"   Moon  ");
			if (sun.elevation > 0.0)
				attrset(COLOR_PAIR(3)|A_BOLD);
			else
				attrset(COLOR_PAIR(2));
			mvprintw(21,55,"%-7.2fAz",sun.azimuth*180.0/M_PI);
			mvprintw(22,55,"%+-6.2f El",sun.elevation*180.0/M_PI);

			attrset(COLOR_PAIR(3)|A_BOLD);
			if (moon.elevation > 0.0)
				attrset(COLOR_PAIR(1)|A_BOLD);
			else
				attrset(COLOR_PAIR(1));
			mvprintw(21,70,"%-7.2fAz",moon.azimuth*180.0/M_PI);
			mvprintw(22,70,"%+-6.2f El",moon.elevation*180.0/M_PI);

			attrset(COLOR_PAIR(2)|A_BOLD);

			//display downlink/uplink information
			if (comsat) {

				length=strlen(sat_db.transponders[xponder].name)/2;
	      mvprintw(TRANSPONDER_START_ROW,0,"                                                                                ");
				mvprintw(TRANSPONDER_START_ROW,40-length,"%s",sat_db.transponders[xponder].name);

				if (downlink!=0.0)
					mvprintw(TRANSPONDER_START_ROW+2,11,"%11.5f MHz%c%c%c",downlink,
					readfreq ? '<' : ' ',
					(readfreq || downlink_update) ? '=' : ' ',
					downlink_update ? '>' : ' ');

				else
					mvprintw(TRANSPONDER_START_ROW+2,11,"               ");

				if (uplink!=0.0)
					mvprintw(TRANSPONDER_START_ROW+1,11,"%11.5f MHz%c%c%c",uplink,
					readfreq ? '<' : ' ',
					(readfreq || uplink_update) ? '=' : ' ',
					uplink_update ? '>' : ' ');

				else
					mvprintw(TRANSPONDER_START_ROW+1,11,"               ");
			}

			//calculate and display downlink/uplink information during pass, and control rig if available
			delay=1000.0*((1000.0*obs.range)/299792458.0);
			if (obs.elevation>=horizon) {
				if (obs.elevation>=0 && aos_alarm==0) {
					beep();
					aos_alarm=1;
				}

				if (comsat) {
					attrset(COLOR_PAIR(4)|A_BOLD);

					if (fabs(obs.range_rate)<0.1)
						mvprintw(TRANSPONDER_START_ROW+3,34,"    TCA    ");
					else {
						if (obs.range_rate<0.0)
							mvprintw(TRANSPONDER_START_ROW+3,34,"Approaching");

						if (obs.range_rate>0.0)
							mvprintw(TRANSPONDER_START_ROW+3,34,"  Receding ");
					}

					attrset(COLOR_PAIR(2)|A_BOLD);

					if (downlink!=0.0) {
						double downlink_doppler = downlink + predict_doppler_shift(qth, &orbit, downlink);
						mvprintw(TRANSPONDER_START_ROW+2,32,"%11.5f MHz",downlink_doppler);
						loss=32.4+(20.0*log10(downlink))+(20.0*log10(obs.range));
						mvprintw(TRANSPONDER_START_ROW+2,67,"%7.3f dB",loss);
						mvprintw(TRANSPONDER_START_ROW+3,13,"%7.3f   ms",delay);
						if (downlink_info->connected && downlink_update)
							rigctld_set_frequency(downlink_info, downlink_doppler);
					}

					else
					{
						mvprintw(TRANSPONDER_START_ROW+2,32,"                ");
						mvprintw(TRANSPONDER_START_ROW+2,67,"          ");
						mvprintw(TRANSPONDER_START_ROW+3,13,"            ");
					}
					if (uplink!=0.0) {
						double uplink_doppler = uplink - predict_doppler_shift(qth, &orbit, uplink);
						mvprintw(TRANSPONDER_START_ROW+1,32,"%11.5f MHz",uplink_doppler);
						loss=32.4+(20.0*log10(uplink))+(20.0*log10(obs.range));
						mvprintw(TRANSPONDER_START_ROW+1,67,"%7.3f dB",loss);
						if (uplink_info->connected && uplink_update)
							rigctld_set_frequency(uplink_info, uplink_doppler);
					}
					else
					{
						mvprintw(TRANSPONDER_START_ROW+1,32,"                ");
						mvprintw(TRANSPONDER_START_ROW+1,67,"          ");
					}

					if (uplink!=0.0 && downlink!=0.0)
						mvprintw(TRANSPONDER_START_ROW+3,67,"%7.3f ms",2.0*delay);
					else
						mvprintw(TRANSPONDER_START_ROW+3,67,"              ");
				}

			} else {
				aos_alarm=0;

				if (comsat) {
					mvprintw(TRANSPONDER_START_ROW+1,32,"                ");
					mvprintw(TRANSPONDER_START_ROW+1,67,"          ");
					mvprintw(TRANSPONDER_START_ROW+2,32,"                ");
					mvprintw(TRANSPONDER_START_ROW+2,67,"          ");
					mvprintw(TRANSPONDER_START_ROW+3,13,"            ");
					mvprintw(TRANSPONDER_START_ROW+3,34,"           ");
					mvprintw(TRANSPONDER_START_ROW+3,67,"          ");
				}
			}

			//display rotation information
			if (rotctld->connected) {
				if (obs.elevation>=horizon)
					mvprintw(18,67,"   Active   ");
				else
					mvprintw(18,67,"Standing  By");
			} else
				mvprintw(18,67,"Not  Enabled");


			//send data to rotctld
			if (obs.elevation*180.0/M_PI >= horizon) {
				time_t curr_time = time(NULL);
				int elevation = (int)round(obs.elevation*180.0/M_PI);
				int azimuth = (int)round(obs.azimuth*180.0/M_PI);
				bool coordinates_differ = (elevation != prev_elevation) || (azimuth != prev_azimuth);
				bool use_update_interval = (rotctld->update_time_interval > 0);

				//send when coordinates differ or when a update interval has been specified
				if ((coordinates_differ && !use_update_interval) || (use_update_interval && ((curr_time - rotctld->update_time_interval) >= prev_time))) {
					if (rotctld->connected) rotctld_track(rotctld, obs.azimuth*180.0/M_PI, obs.elevation*180.0/M_PI);
					prev_elevation = elevation;
					prev_azimuth = azimuth;
					prev_time = curr_time;
				}
			}

			/* Get input from keyboard */

			ans=getch();

			if (comsat) {
				if (ans==' ' && sat_db.num_transponders>1) {
					xponder++;

					if (xponder>=sat_db.num_transponders)
						xponder=0;

					move(9,1);
					clrtoeol();

					downlink_start=sat_db.transponders[xponder].downlink_start;
					downlink_end=sat_db.transponders[xponder].downlink_end;
					uplink_start=sat_db.transponders[xponder].uplink_start;
					uplink_end=sat_db.transponders[xponder].uplink_end;

					if (downlink_start>downlink_end)
						polarity=-1;

					if (downlink_start<downlink_end)
						polarity=1;

					if (downlink_start==downlink_end)
						polarity=0;

					downlink=0.5*(downlink_start+downlink_end);
					uplink=0.5*(uplink_start+uplink_end);
				}

				double shift = 0;

				/* Raise uplink frequency */
				if (ans==KEY_UP || ans=='>' || ans=='.') {
					if (ans==KEY_UP || ans=='>')
						shift=0.001;  /* 1 kHz */
					else
						shift=0.0001; /* 100 Hz */
				}

				/* Lower uplink frequency */
				if (ans==KEY_DOWN || ans=='<' || ans== ',') {
					if (ans==KEY_DOWN || ans=='<')
						shift=-0.001;  /* 1 kHz */
					else
						shift=-0.0001; /* 100 Hz */
				}

				uplink+=shift*(double)abs(polarity);
				downlink=downlink+(shift*(double)polarity);

				if (uplink < uplink_start) {
					uplink=uplink_end;
					downlink=downlink_end;
				}
				if (uplink > uplink_end) {
					uplink=uplink_start;
					downlink=downlink_start;
				}

				if (ans=='d')
					downlink_update=true;
				if (ans=='D')
					downlink_update=false;
				if (ans=='u')
					uplink_update=true;
				if (ans=='U')
					uplink_update=false;
				if (ans=='f' || ans=='F')
				{
					if (downlink_info->connected)
						downlink = inverse_doppler_shift(DOPP_DOWNLINK, qth, &orbit, rigctld_read_frequency(downlink_info));
					if (uplink_info->connected)
						uplink = inverse_doppler_shift(DOPP_UPLINK, qth, &orbit, rigctld_read_frequency(uplink_info));
					if (ans=='f')
					{
						downlink_update=true;
						uplink_update=true;
					}
				}
				if (ans=='m')
					readfreq=true;
				if (ans=='M')
					readfreq=false;
				if (ans=='x') // Reverse VFO uplink and downlink names
				{
					if (downlink_info->connected && uplink_info->connected)
					{
						char tmp_vfo[MAX_NUM_CHARS];
						strncpy(tmp_vfo, downlink_info->vfo_name, MAX_NUM_CHARS);
						strncpy(downlink_info->vfo_name, uplink_info->vfo_name, MAX_NUM_CHARS);
						strncpy(uplink_info->vfo_name, tmp_vfo, MAX_NUM_CHARS);
					}
				}
			}

			refresh();

			if ((ans == KEY_LEFT) || (ans == '-')) {
				orbit_ind = singletrack_get_next_enabled_satellite(orbit_ind, -1, tle_db);
			}

			if ((ans == KEY_RIGHT) || (ans == '+')) {
				orbit_ind = singletrack_get_next_enabled_satellite(orbit_ind, +1, tle_db);
			}

			if (ans == SINGLETRACK_HELP_KEY) {
				singletrack_help();
			}

			halfdelay(HALF_DELAY_TIME);

		} while (ans!='q' && ans!='Q' && ans!=27 &&
		 	ans!='+' && ans!='-' &&
			ans!=KEY_LEFT && ans!=KEY_RIGHT && ans!=SINGLETRACK_HELP_KEY);

		predict_destroy_orbital_elements(orbital_elements);
	} while (ans!='q' && ans!=17 && ans!=27);

	cbreak();
}

