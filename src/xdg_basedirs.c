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
 * Check if dirpath contains a backslash at the end, and append one if not.
 *
 * \param dirpath Directory path
 * \return Path with backslash appended
 **/
char *append_backslash(char *dirpath)
{
	if (strlen(dirpath) == 0) {
		return NULL;
	}

	if (dirpath[strlen(dirpath)-1] != '/') {
		int new_size = strlen(dirpath)+2;
		char *new_str = (char*)malloc(new_size);
		strncpy(new_str, dirpath, new_size);
		new_str[new_size-2] = '/';
		new_str[new_size-1] = '\0';
		return new_str;
	} else {
		return strdup(dirpath);
	}
}

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

	char *data_dirs_app = append_backslash(data_dirs);
	free(data_dirs);

	return data_dirs_app;
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

	char *data_dirs_app = append_backslash(data_dirs);
	free(data_dirs);

	return data_dirs_app;
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

	//create ~/.config
	char *config_home = xdg_config_home();
	struct stat s;
	int err = stat(config_home, &s);
	if ((err == -1) && (errno == ENOENT)) {
		mkdir(config_home, 0700);
	}

	//create ~/.config/flyby
	char config_path[MAX_NUM_CHARS] = {0};
	snprintf(config_path, MAX_NUM_CHARS, "%s%s", config_home, FLYBY_RELATIVE_ROOT_PATH);
	free(config_home);
	err = stat(config_path, &s);
	if ((err == -1) && (errno == ENOENT)) {
		mkdir(config_path, 0777);
	}

	//create ~/.local
	char *data_home = xdg_data_home();
	err = stat(data_home, &s);
	if ((err == -1) && (errno == ENOENT)) {
		mkdir(data_home, 0700);
	}

	//create ~/.local/share/flyby
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
