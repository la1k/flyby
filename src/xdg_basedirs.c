#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "ui.h"
#include <stdlib.h>
#include "string_array.h"
#include <dirent.h>
#include "xdg_basedirs.h"
#include <math.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

//xdg basedir specification variables
#define XDG_DATA_DIRS "XDG_DATA_DIRS"
#define XDG_DATA_DIRS_DEFAULT "/usr/local/share/:/usr/share/"
#define XDG_DATA_HOME "XDG_DATA_HOME"
#define XDG_DATA_HOME_DEFAULT ".local/share/"
#define XDG_CONFIG_DIRS "XDG_CONFIG_DIRS"
#define XDG_CONFIG_DIRS_DEFAULT "/etc/xdg/"
#define XDG_CONFIG_HOME "XDG_CONFIG_HOME"
#define XDG_CONFIG_HOME_DEFAULT ".config/"

/**
 * Helper function for creating an XDG_DIRS-variable. Use the value contained within environment variable varname, if empty use default_val.
 *
 * \param varname Environment variable name
 * \param default_val Default value
 **/
char *xdg_dirs(const char *varname, const char *default_val)
{
	char *data_dirs = getenv(varname);
	if ((data_dirs == NULL) || (strlen(data_dirs) == 0)) {
		data_dirs = (char*)default_val;
	}

	//duplicate string (string from getenv is static)
	data_dirs = strdup(data_dirs);

	return data_dirs;
}

/**
 * Helper function for constructing a XDG_HOME-variable. Use the value
 * contained within environment variable varname, if empty use $HOME/default_val.
 *
 * \param varname Environment variable name
 * \param default_val Default value
 **/
char *xdg_home(const char *varname, const char *default_val)
{
	char *data_dirs = getenv(varname);
	if ((data_dirs == NULL) || (strlen(data_dirs) == 0)) {
		char *home_env = getenv("HOME");
		int size = strlen(home_env) + strlen(default_val) + 2;
		data_dirs = (char*)malloc(sizeof(char)*size);
		snprintf(data_dirs, size, "%s%s%s", home_env, "/", default_val);
	} else {
		char *temp = data_dirs;
		data_dirs = (char*)malloc(sizeof(char)*(strlen(data_dirs) + 1));
		strcpy(data_dirs, temp);
	}
	return data_dirs;
}

char *xdg_data_dirs()
{
	return xdg_dirs(XDG_DATA_DIRS, XDG_DATA_DIRS_DEFAULT);
}

char *xdg_data_home()
{
	return xdg_home(XDG_DATA_HOME, XDG_DATA_HOME_DEFAULT);
}

char *xdg_config_dirs()
{
	return xdg_dirs(XDG_CONFIG_DIRS, XDG_CONFIG_DIRS_DEFAULT);
}

char *xdg_config_home()
{
	return xdg_home(XDG_CONFIG_HOME, XDG_CONFIG_HOME_DEFAULT);
}

void create_xdg_dirs()
{
	//create ~/.config/flyby
	char *config_home = xdg_config_home();
	char config_path[MAX_NUM_CHARS] = {0};
	snprintf(config_path, MAX_NUM_CHARS, "%s%s", config_home, FLYBY_RELATIVE_ROOT_PATH);
	free(config_home);
	struct stat s;
	int err = stat(config_path, &s);
	if ((err == -1) && (errno == ENOENT)) {
		mkdir(config_path, 0777);
	}

	//create ~/.local/share/flyby
	char *data_home = xdg_data_home();
	char data_path[MAX_NUM_CHARS] = {};
	snprintf(data_path, MAX_NUM_CHARS, "%s%s", data_home, FLYBY_RELATIVE_ROOT_PATH);
	err = stat(data_path, &s);
	if ((err == -1) && (errno == ENOENT)) {
		mkdir(data_path, 0777);
	}

	//create ~/.local/share/flyby/tles
	snprintf(data_path, MAX_NUM_CHARS, "%s%s", data_home, TLE_RELATIVE_DIR_PATH);
	err = stat(data_path, &s);
	if ((err == -1) && (errno == ENOENT)) {
		mkdir(data_path, 0777);
	}
	free(data_home);
}
