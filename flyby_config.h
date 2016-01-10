#ifndef FLYBY_CONFIG_H_DEFINED
#define FLYBY_CONFIG_H_DEFINED

/**
 * Read TLE entries from folders defined using the XDG file specification. TLEs are read
 * from files located in {XDG_DATA_DIRS}/flyby/tles and XDG_DATA_HOME/flyby/tles. 
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
 * Read transponder database from file.
 *
 * \param db_file .db file
 * \param tle_db Previously read TLE database, for which fields from transponder database are matched
 * \param ret_db Returned transponder database
 * \return 0 on success, -1 otherwise
 **/
int flyby_read_transponder_db(const char *db_file, const struct tle_db *tle_db, struct transponder_db *ret_db);

#endif
