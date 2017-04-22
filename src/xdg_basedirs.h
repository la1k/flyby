#ifndef FLYBY_CONFIG_H_DEFINED
#define FLYBY_CONFIG_H_DEFINED

//default relative subdir
#define FLYBY_RELATIVE_ROOT_PATH "flyby/"

//default relative TLE data directory
#define TLE_RELATIVE_DIR_PATH FLYBY_RELATIVE_ROOT_PATH "tles/"

//default relative qth config filename
#define QTH_RELATIVE_FILE_PATH FLYBY_RELATIVE_ROOT_PATH "flyby.qth"

//default relative transponder database filename
#define DB_RELATIVE_FILE_PATH FLYBY_RELATIVE_ROOT_PATH "flyby.db"

//default relative whitelist filename
#define WHITELIST_RELATIVE_FILE_PATH FLYBY_RELATIVE_ROOT_PATH "flyby.whitelist"

//default relative multitrack settings filename
#define MULTITRACK_SETTINGS_FILE FLYBY_RELATIVE_ROOT_PATH "multitrack_settings.conf"

/**
 * \return XDG_DATA_DIRS variable, or the xdg basedir specification default if the environment variable is empty
 **/
char *xdg_data_dirs();

/**
 * \return XDG_DATA_HOME variable, or the xdg basedir specification default if the environment variable is empty
 **/
char *xdg_data_home();

/**
 * \return XDG_CONFIG_DIRS variable, or the xdg basedir specification default if XDG_CONFIG_DIRS is empty
 **/
char *xdg_config_dirs();

/**
 * \return XDG_CONFIG_HOME variable, or the xdg basedir specification default if XDG_CONFIG_HOME is empty
 **/
char *xdg_config_home();

/**
 * Create ~/.config/flyby and ./local/share/flyby/tles/ if these do not exist.
 **/
void create_xdg_dirs();

#endif
