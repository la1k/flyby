#ifndef TRANSPONDER_DB_H_DEFINED
#define TRANSPONDER_DB_H_DEFINED

#include "flyby_defines.h"
#include "tle_db.h"

/**
 * Entry in transponder database.
 **/
struct sat_db_entry {
	///satellite number, for relating to TLE database
	long satellite_number;
	///whether squint angle can be calculated
	bool squintflag;
	///attitude latitude for squint angle calculation
	double alat;
	//attitude longitude for squint angle calculation
	double alon;
	///number of transponders
	int num_transponders;
	///name of each transponder
	char transponder_name[MAX_NUM_TRANSPONDERS][MAX_NUM_CHARS];
	///uplink frequencies
	double uplink_start[MAX_NUM_TRANSPONDERS];
	double uplink_end[MAX_NUM_TRANSPONDERS];
	///downlink frequencies
	double downlink_start[MAX_NUM_TRANSPONDERS];
	double downlink_end[MAX_NUM_TRANSPONDERS];
	///at which day of week the transponder is turned on?
	unsigned char dayofweek[MAX_NUM_TRANSPONDERS];
	///phase something
	int phase_start[MAX_NUM_TRANSPONDERS];
	int phase_end[MAX_NUM_TRANSPONDERS];
};

/**
 * Transponder database, each entry index corresponding to the same TLE index in the TLE database.
 **/
struct transponder_db {
	///number of contained satellites. Corresponds to the number of TLEs in the TLE database
	int num_sats;
	///transponder database entries
	struct sat_db_entry sats[MAX_NUM_SATS];
	///whether the transponder database is loaded, or empty
	bool loaded;
};

/**
 * Read transponder database from file. Only fields matching the TLE database fields are modified.
 *
 * \param db_file .db file
 * \param tle_db Previously read TLE database, for which fields from transponder database are matched
 * \param ret_db Returned transponder database
 * \return 0 on success, -1 otherwise
 **/
int transponder_db_from_file(const char *db_file, const struct tle_db *tle_db, struct transponder_db *ret_db);

#endif
