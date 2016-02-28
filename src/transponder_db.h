#ifndef TRANSPONDER_DB_H_DEFINED
#define TRANSPONDER_DB_H_DEFINED

#include "defines.h"
#include "tle_db.h"

/**
 * Entry in transponder database.
 **/
struct sat_db_entry {
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
 * Read transponder database from folders defined using the XDG file specification.
 * Database file is assumed to be located in {XDG_DATA_DIRS}/flyby/flyby.db and XDG_DATA_HOME/flyby/flyby.db.
 * A union over the files is used.
 *
 * Transponder entries defined in XDG_DATA_HOME take precedence over XDG_DATA_DIRS. XDG_DATA_DIRS
 * ordering decides precedence of entries defined across XDG_DATA_DIRS directories.
 *
 * \param tle_db Full TLE database for which transponder database entries are matched
 * \param transponder_db Returned transponder database
 **/
void transponder_db_from_search_paths(const struct tle_db *tle_db, struct transponder_db *transponder_db);

/**
 * Read transponder database from file. Only fields matching the TLE database fields are modified.
 *
 * \param db_file .db file
 * \param tle_db Previously read TLE database, for which fields from transponder database are matched
 * \param ret_db Returned transponder database
 * \return 0 on success, -1 otherwise
 **/
int transponder_db_from_file(const char *db_file, const struct tle_db *tle_db, struct transponder_db *ret_db);

/**
 * Write transponder database to file.
 *
 * \param filename Filename
 * \param tle_db TLE database, used for obtaining name and satellite number of satellite
 * \param transponder_db Transponder database to write to file
 **/
void transponder_db_to_file(const char *filename, struct tle_db *tle_db, struct transponder_db *transponder_db);

#endif
