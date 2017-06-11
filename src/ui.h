#ifndef FLYBY_UI_H_DEFINED
#define FLYBY_UI_H_DEFINED

#include "hamlib.h"
#include <predict/predict.h>
#include "tle_db.h"
#include "transponder_db.h"
#include <curses.h>

/**
 * Print sun azimuth/elevation to infobox on the standard screen.
 *
 * \param row Start row for printing
 * \param col Start column for printing
 * \param qth QTH coordinates
 * \param daynum Time for calculation
 **/
void print_sun_box(int row, int col, predict_observer_t *qth, predict_julian_date_t daynum);

/**
 * Print moon azimuth/elevation to infobox on the standard screen.
 *
 * \param row Start row for printing
 * \param col Start column for printing
 * \param qth QTH coordinates
 * \param daynum Time for calculation
 **/
void print_moon_box(int row, int col, predict_observer_t *qth, predict_julian_date_t daynum);

/**
 * Print QTH coordinates in infobox on standard screen. Uses 9 columns and 3 rows.
 *
 * \param row Start row for printing
 * \param col Start column for printing
 * \param qth QTH coordinates

 **/
void print_qth_box(int row, int col, predict_observer_t *qth);

/**
 * Trim whitespaces in string from end. Used for massaging output from FIELD/FORMs.
 *
 * \param string String to modify. Is modified in place
 **/
void trim_whitespaces_from_end(char *string);

/**
 * Quits ncurses, resets the terminal and displays an error message.
 *
 * \param string Error message
 **/
void bailout(const char *string);

/* This function updates PREDICT's orbital datafile from a NASA
 * 2-line element file either through a menu (interactive mode)
 * or via the command line.  string==filename of 2-line element
 * set if this function is invoked via the command line. Only
 * entries present within the TLE database are updated, rest
 * is ignored.
 *
 * \param string Filename, or 0 if interactive mode is to be used
 * \param tle_db Pre-loaded TLE database
 * \return 0 on success, -1 otherwise
 **/
void update_tle_database(const char *string, struct tle_db *tle_db);

/**
 * Run flyby UI.
 *
 * \param new_user Whether NewUser() should be run
 * \param qthfile Write path for QTH file
 * \param observer QTH coordinates
 * \param tle_db TLE database
 * \param sat_db Transponder database
 * \param rotctld Rotctld info
 * \param downlink Downlink info
 * \param uplink Uplink info
 **/
void run_flyby_curses_ui(bool new_user, const char *qthfile, predict_observer_t *observer, struct tle_db *tle_db, struct transponder_db *sat_db, rotctld_info_t *rotctld, rigctld_info_t *downlink, rigctld_info_t *uplink);

#endif
