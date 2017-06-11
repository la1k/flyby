#ifndef TRANSPONDER_EDITOR_H_DEFINED
#define TRANSPONDER_EDITOR_H_DEFINED

#include "transponder_db.h"

/**
 * Edit entries in transponder database. Updates user database defined in XDG_DATA_HOME on exit.
 *
 * \param start_index Selected index in the menu
 * \param tle_db TLE database, used for satellite names and numbers
 * \param sat_db Satellite database to edit
 **/
void transponder_database_editor(int start_index, struct tle_db *tle_db, struct transponder_db *sat_db);

#endif
