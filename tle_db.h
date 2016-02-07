#ifndef TLE_DB_H_DEFINED
#define TLE_DB_H_DEFINED

#include <stdbool.h>
#include "flyby_defines.h"

/**
 * Entry in TLE database.
 **/
struct tle_db_entry {
	///satellite number, parsed from TLE line 1
	long satellite_number;
	///satellite name, defined in TLE file
	char name[MAX_NUM_CHARS];
	///line 1 in NORAD TLE
	char line1[MAX_NUM_CHARS];
	///line 2 in NORAD TLE
	char line2[MAX_NUM_CHARS];
	///Filename from which the TLE has been read
	char filename[MAX_NUM_CHARS];
};

/**
 * TLE database.
 **/
struct tle_db {
	///Number of contained TLEs
	int num_tles;
	///TLE entries
	struct tle_db_entry tles[MAX_NUM_SATS];
	///Whether TLE database was read from XDG standard paths or supplied on command line
	bool read_from_xdg;
};

/**
 * Defines whether to overwrite only older TLE entries or all existing TLE entries when merging two databases.
 **/
enum tle_merge_behavior {
	///Overwrite only old existing TLE entries
	TLE_OVERWRITE_OLD,
	///Overwrite none of the existing TLE entries
	TLE_OVERWRITE_NONE
};

/**
 * Merge two TLE databases.
 *
 * \param new_db New TLE database to merge into an existing one
 * \param main_db Existing TLE database into which new TLE database is to be merged
 * \param merge_opt Merge options
 **/
void tle_db_merge(struct tle_db *new_db, struct tle_db *main_db, enum tle_merge_behavior merge_opt);

/**
 * Check internal TLE lines within tle entries to see whether one is more recent than the other.
 *
 * \param tle_entry_1 TLE entry 1
 * \param tle_entry_2 TLE entry 2
 * \return True if TLE entry 1 is more recent than TLE entry 2
 **/
bool tle_db_entry_is_newer_than(struct tle_db_entry tle_entry_1, struct tle_db_entry tle_entry_2);

/**
 * Overwrite TLE database entry with supplied TLE entry.
 *
 * \param entry_index Index in TLE database
 * \param tle_db TLE database
 * \param new_entry TLE entry to overwrite on specified index
 **/
void tle_db_overwrite_entry(int entry_index, struct tle_db *tle_db, const struct tle_db_entry *new_entry);

/**
 * Add TLE entry to database.
 *
 * \param tle_db TLE database
 * \param entry TLE database entry to add
 **/
void tle_db_add_entry(struct tle_db *tle_db, const struct tle_db_entry *entry);

/**
 * Find TLE entry within TLE database. Searches with respect to the satellite number.
 *
 * \param tle_db TLE database
 * \param satellite_number Lookup satellite number
 * \return Index within TLE database if found, -1 otherwise
 **/
int tle_db_find_entry(struct tle_db *tle_db, long satellite_number);

/**
 * Read TLEs from files in specified directory. When TLE entries are multiply defined
 * across TLE files, the TLE entry with the most recent epoch is chosen.
 *
 * \param dirpath Directory from which files are to be read
 * \param ret_tle_db Returned TLE database
 **/
void tle_db_from_directory(const char *dirpath, struct tle_db *ret_tle_db);

/**
 * Read TLE database from file.
 *
 * \param tle_file TLE database file
 * \param ret_db Returned TLE database
 * \return 0 on success, -1 otherwise
 **/
int tle_db_from_file(const char *tle_file, struct tle_db *ret_db);

/**
 * Write contents of TLE database to file.
 *
 * \param filename Filename
 * \param tle_db TLE database to write
 **/
void tle_db_to_file(const char *filename, struct tle_db *tle_db);


#endif
