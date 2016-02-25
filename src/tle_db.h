#ifndef TLE_DB_H_DEFINED
#define TLE_DB_H_DEFINED

#include <stdbool.h>
#include "string_array.h"
#include "defines.h"

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
	///Whether TLE entry is enabled for display
	bool enabled;
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
 * Read TLE entries from folders defined using the XDG file specification. TLEs are read
 * from files located in {XDG_DATA_DIRS}/flyby/tles and XDG_DATA_HOME/flyby/tles.
 * A union over the files is used.
 *
 * When the same TLE is defined in multiple files, the following behavior is used:
 *  - TLEs from XDG_DATA_HOME take precedence over any other folder, regardless of TLE epoch
 *  - TLEs across XDG_DATA_DIRS take precedence in the order defined within XDG_DATA_DIRS, regardless of TLE epoch
 *  - For multiply defined TLEs within a single folder, the TLE with the most recent epoch is chosen
 *
 * \param ret_tle_db Returned TLE database
 **/
void tle_db_from_search_paths(struct tle_db *ret_tle_db);

/**
 * Update internal TLE database with newer TLE entries located within supplied file, and update the corresponding file databases.
 * Following rules are used:
 *
 * - If the original TLE file is at a writable location: Update that file. Each file will be updated once.
 * - If the original TLE file is at a non-writable location, and the TLE database was read from XDG dirs: Create a new file in XDG_DATA_HOME/flyby/tle/, according to the filename defined in get_update_filename(). All TLEs will be written to the same file.
 *
 *  Update file will not be created if TLE database was not read from XDG, as it will be assumed that TLE files have been specified using the command line options, and it will be meaningless to create new files in any location.
 *
 * \param filename TLE file database to read
 * \param tle_db TLE database
 * \param ret_was_updated Boolean array of at least size tle_db->num_tles. Will contain true at the entry indices that were updated. Set to NULL if this is not to be used
 * \param ret_in_new_file Boolean array of at least size tle_db->num_tles. Will contain true at the entry indices that were updated and put in a new update file within the TLE folder. Check against tle_db->read_from_xdg_dirs to see whether file actually was created or not
 **/
void tle_db_update(const char *filename, struct tle_db *tle_db, bool *ret_was_updated, bool *ret_in_new_file);

/**
 * Read TLEs from files in specified directory. When TLE entries are multiply defined
 * across TLE files, the TLE entry with the most recent epoch is chosen.
 *
 * \param dirpath Directory from which files are to be read
 * \param ret_tle_db Returned TLE database
 **/
void tle_db_from_directory(const char *dirpath, struct tle_db *ret_tle_db);

/**
 * Set entry in TLE database to enabled/disabled.
 *
 * \param db TLE database
 * \param tle_index Index in TLE database
 * \param enabled True for enabling, false for disabling
 **/
void tle_db_entry_set_enabled(struct tle_db *db, int tle_index, bool enabled);

/**
 * Check whether TLE entry is enabled/disabled.
 *
 * \param db TLE database
 * \param tle_index Index in TLE database
 * \return True if TLE entry is enabled, false otherwise
 **/
bool tle_db_entry_enabled(const struct tle_db *db, int tle_index);

/**
 * Set TLE database entries to enabled according to whitelist file in search paths. Default is to let
 * TLE entry be disabled.
 *
 * Whitelist file is assumed to be located in XDG_CONFIG_HOME/flyby/flyby.whitelist, containing a list of
 * satellite numbers corresponding to the TLEs that should be enabled.
 *
 * \param db TLE database, which will have its enabled/disabled flags modified according to the whitelist file
 **/
void whitelist_from_search_paths(struct tle_db *db);

/**
 * Write enabled/disabled flags for each TLE entry to default writepath (XDG_CONFIG_HOME/flyby/flyby.whitelist).
 *
 * \param db TLE database
 **/
void whitelist_write_to_default(struct tle_db *db);

/**
 * Get list of filenames from which TLE database is generated.
 *
 * \param tle_db TLE database
 * \return List of filenames
 **/
string_array_t tle_db_filenames(const struct tle_db *db);

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
int tle_db_find_entry(const struct tle_db *tle_db, long satellite_number);


#endif
