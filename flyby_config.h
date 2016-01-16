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
void flyby_read_tles_from_xdg(struct tle_db *ret_tle_db);

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
void flyby_read_transponder_db_from_xdg(const struct tle_db *tle_db, struct transponder_db *transponder_db);

/**
 * Read TLE database from file.
 *
 * \param tle_file TLE database file
 * \param ret_db Returned TLE database
 * \return 0 on success, -1 otherwise
 **/
int flyby_read_tle_file(const char *tle_file, struct tle_db *ret_db);

/**
 * Read QTH information from file.
 *
 * \param qth_file .qth file
 * \param ret_observer Returned observer structure
 * \return 0 on success, -1 otherwise
 **/
int flyby_read_qth_file(const char *qth_file, predict_observer_t *ret_observer);

/**
 * Write QTH information to specified file.
 *
 * \param qth_path File path
 * \param qth Qth information to write
 **/
void flyby_write_qth_to_file(const char *qth_path, predict_observer_t *qth);

/**
 * Get local user qth filepath (~/.config/flyby/flyby.qth).
 **/
char* flyby_get_xdg_qth_writepath();

/**
 * Read transponder database from file. Only fields matching the TLE database fields are modified.
 *
 * \param db_file .db file
 * \param tle_db Previously read TLE database, for which fields from transponder database are matched
 * \param ret_db Returned transponder database
 * \return 0 on success, -1 otherwise
 **/
int flyby_read_transponder_db(const char *db_file, const struct tle_db *tle_db, struct transponder_db *ret_db);

/**
 * Used for determining from where the QTH file was read.
 **/
enum qth_file_state {
	QTH_FILE_HOME, //read from XDG_CONFIG_HOME
	QTH_FILE_SYSTEMWIDE, //read from XDG_CONFIG_DIRS
	QTH_FILE_NOTFOUND //not found
};

/**
 * Read flyby from XDG filepaths. Try XDG_CONFIG_HOME/flyby/flyby.qth first, then the paths in XDG_CONFIG_DIRS/flyby/flyby.qth.
 *
 * \param ret_observer Returned QTH information
 * \return Where the QTH file was read from, user home, system dir or not found at all
 **/
enum qth_file_state flyby_read_qth_from_xdg(predict_observer_t *ret_observer);

#endif
