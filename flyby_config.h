#ifndef FLYBY_CONFIG_H_DEFINED
#define FLYBY_CONFIG_H_DEFINED

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
 * Read TLE database from file.
 *
 * \param tle_file TLE database file
 * \param ret_db Returned TLE database
 * \return 0 on success, -1 otherwise
 **/
int tle_db_from_file(const char *tle_file, struct tle_db *ret_db);

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

void tle_db_entry_set_enabled(struct tle_db *db, int tle_index, bool enabled);
bool tle_db_entry_enabled(struct tle_db *db, int tle_index);

void whitelist_from_search_paths(struct tle_db *db);
void whitelist_write_to_default(struct tle_db *db);

/**
 * Used for determining from where the QTH file was read.
 **/
enum qth_file_state {
	QTH_FILE_HOME, //read from XDG_CONFIG_HOME
	QTH_FILE_SYSTEMWIDE, //read from XDG_CONFIG_DIRS
	QTH_FILE_NOTFOUND //not found
};

/**
 * Read flyby.qth from XDG filepaths. Try XDG_CONFIG_HOME/flyby/flyby.qth first, then the paths in XDG_CONFIG_DIRS/flyby/flyby.qth.
 *
 * \param ret_observer Returned QTH information
 * \return Where the QTH file was read from: user home, system dir or not found at all
 **/
enum qth_file_state qth_from_search_paths(predict_observer_t *ret_observer);

/**
 * Read QTH information from file.
 *
 * \param qth_file QTH config file
 * \param ret_observer Returned observer structure
 * \return 0 on success, -1 otherwise
 **/
int qth_from_file(const char *qth_file, predict_observer_t *ret_observer);

/**
 * Write QTH information to specified file.
 *
 * \param qth_path File path
 * \param qth QTH information to write
 **/
void qth_to_file(const char *qth_path, predict_observer_t *qth);

/**
 * Get local user qth filepath (XDG_CONFIG_HOME/flyby/flyby.qth, AKA ~/.config/flyby/flyby.qth).
 **/
char* qth_default_writepath();

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

#endif
