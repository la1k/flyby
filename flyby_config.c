#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "flyby_ui.h"
#include <stdlib.h>
#include "string_array.h"
#include <dirent.h>
#include "flyby_config.h"
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

//default relative subdir
#define FLYBY_RELATIVE_ROOT_PATH "flyby/"

//default relative TLE data directory
#define TLE_RELATIVE_DIR_PATH FLYBY_RELATIVE_ROOT_PATH "tles/"

//default relative qth config filename
#define QTH_RELATIVE_FILE_PATH FLYBY_RELATIVE_ROOT_PATH "flyby.qth"

//default relative transponder database filename
#define DB_RELATIVE_FILE_PATH FLYBY_RELATIVE_ROOT_PATH "flyby.db"

/**
 * Helper function for creating an XDG_DIRS-variable. Use the value contained within environment variable varname, if empty use default_val.
 *
 * \param varname Environment variable name
 * \param default_val Default value
 **/
char *xdg_dirs(const char *varname, const char *default_val)
{
	char *data_dirs = getenv(varname);
	if (data_dirs == NULL) {
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
	if (data_dirs == NULL) {
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

/**
 * \return XDG_DATA_DIRS variable, or the xdg basedir specification default if the environment variable is empty
 **/
char *xdg_data_dirs()
{
	return xdg_dirs(XDG_DATA_DIRS, XDG_DATA_DIRS_DEFAULT);
}

/**
 * \return XDG_DATA_HOME variable, or the xdg basedir specification default if the environment variable is empty
 **/
char *xdg_data_home()
{
	return xdg_home(XDG_DATA_HOME, XDG_DATA_HOME_DEFAULT);
}

/**
 * \return XDG_CONFIG_DIRS variable, or the xdg basedir specification default if XDG_CONFIG_DIRS is empty
 **/
char *xdg_config_dirs()
{
	return xdg_dirs(XDG_CONFIG_DIRS, XDG_CONFIG_DIRS_DEFAULT);
}

/**
 * \return XDG_CONFIG_HOME variable, or the xdg basedir specification default if XDG_CONFIG_HOME is empty
 **/
char *xdg_config_home()
{
	return xdg_home(XDG_CONFIG_HOME, XDG_CONFIG_HOME_DEFAULT);
}

/**
 * Create ~/.config/flyby and ./local/share/flyby/tles/ if these do not exist.
 **/
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

/**
 * Split string at ':'-delimiter.
 * \param string_list Input string list delimited by :
 * \param ret_string_list Returned string array
 **/
void stringsplit(const char *string_list, string_array_t *ret_string_list)
{
	char *copy = strdup(string_list);
	const char *delimiter = ":";
	char *token = strtok(copy, delimiter);
	while (token != NULL) {
		string_array_add(ret_string_list, token);
		token = strtok(NULL, delimiter);
	}
	free(copy);
}

/**
 * Get filename to put TLE updates in when the TLE to update are located within system-wide, non-writable locations.
 * Uses XDG_DATA_HOME/flyby/tle/tle-updatefile-[DATE]-[TIME]-[NUMBER].tle, and loops through [NUMBER] until a non-existing
 * file is found.
 **/
char *tle_db_updatefile_writepath()
{
	create_xdg_dirs();

	//create filepath basis
	char *ret_string = (char*)malloc(sizeof(char)*MAX_NUM_CHARS);
	char date_string[MAX_NUM_CHARS];
	time_t epoch = time(NULL);
	strftime(date_string, MAX_NUM_CHARS, "tle-updatefile-%F-%H%M%S-", gmtime(&epoch));
	char *data_home = xdg_data_home();

	//loop through [number] in [basename]-[number].tle until we encounter a file that does not exist yet
	int index = 0;
	while (true) {
		char temp_file[MAX_NUM_CHARS];
		snprintf(temp_file, MAX_NUM_CHARS, "%s%s%s%d.tle", data_home, TLE_RELATIVE_DIR_PATH, date_string, index);
		if (access(temp_file, F_OK) == -1) {
			strncpy(ret_string, temp_file, MAX_NUM_CHARS);
			break;
		}
		index++;
	}
	return ret_string;
}

/**
 * Write the parts of the TLE database that are read from the supplied filename to file again. Used for updating
 * the TLE files with new information, if any.
 *
 * \param tle_filename Filename to which corresponding entries are found and written
 * \param tle_db TLE database
 **/
void tle_db_update_file(const char *tle_filename, struct tle_db *tle_db)
{
	struct tle_db subset_db = {0};
	for (int i=0; i < tle_db->num_tles; i++) {
		if (strcmp(tle_db->tles[i].filename, tle_filename) == 0) {
			tle_db_add_entry(&subset_db, &(tle_db->tles[i]));
		}
	}
	tle_db_to_file(tle_filename, &subset_db);
}

void tle_db_update(const char *filename, struct tle_db *tle_db, bool *ret_was_updated, bool *ret_in_new_file)
{
	struct tle_db new_db = {0};
	int retval = tle_db_from_file(filename, &new_db);
	if (retval != 0) {
		return;
	}

	int num_tles_to_update = 0;
	int *newer_tle_indices = (int*)malloc(sizeof(int)*new_db.num_tles); //indices in new TLE db that should be used to update internal db
	int *tle_indices_to_update = (int*)malloc(sizeof(int)*new_db.num_tles); //indices in internal db that should be updated

	//find more recent entries
	for (int i=0; i < new_db.num_tles; i++) {
		int index = tle_db_find_entry(tle_db, new_db.tles[i].satellite_number);
		if (index != -1) {
			if (tle_db_entry_is_newer_than(new_db.tles[i], tle_db->tles[index])) {
				newer_tle_indices[num_tles_to_update] = i;
				tle_indices_to_update[num_tles_to_update] = index;
				num_tles_to_update++;
			}
		}
	}

	if (num_tles_to_update <= 0) {
		return;
	}

	int num_unwritable = 0;
	int *unwritable_tles = (int*)malloc(sizeof(int)*num_tles_to_update); //indices in the internal db that were updated, but cannot be written to the current file.

	//go over tles to update, collect tles belonging to one file in one update, update the file if possible, add to above array if not. Update internal db with new TLE information.
	for (int i=0; i < num_tles_to_update; i++) {
		if (newer_tle_indices[i] != -1) {
			char *tle_filename = tle_db->tles[tle_indices_to_update[i]].filename; //filename to be updated
			bool file_is_writable = access(tle_filename, W_OK) == 0;

			//find entries in tle database with corresponding filenames
			for (int j=i; j < num_tles_to_update; j++) {
				if (newer_tle_indices[j] != -1) {
					int tle_index = tle_indices_to_update[j];
					struct tle_db_entry *tle_update_entry = &(new_db.tles[newer_tle_indices[j]]);
					struct tle_db_entry *tle_entry = &(tle_db->tles[tle_index]);
					if (strcmp(tle_filename, tle_entry->filename) == 0) {
						//update tle db entry with new entry
						char keep_filename[MAX_NUM_CHARS];
						char keep_name[MAX_NUM_CHARS];
						strncpy(keep_filename, tle_entry->filename, MAX_NUM_CHARS);
						strncpy(keep_name, tle_entry->name, MAX_NUM_CHARS);

						tle_db_overwrite_entry(tle_index, tle_db, tle_update_entry);

						//keep old filename and name
						strncpy(tle_entry->filename, keep_filename, MAX_NUM_CHARS);
						strncpy(tle_entry->name, keep_name, MAX_NUM_CHARS);

						//set db indices to update to -1 in order to ignore them on the next update
						newer_tle_indices[j] = -1;

						if (ret_was_updated != NULL) {
							ret_was_updated[tle_index] = true;
						}

						if (!file_is_writable) {
							//add to list over unwritable TLE filenames
							unwritable_tles[num_unwritable] = tle_index;
							num_unwritable++;

							if (ret_in_new_file != NULL) {
								ret_in_new_file[tle_index] = true;
							}
						}
					}
				}
			}

			//write updated TLE entries to file
			if (file_is_writable) {
				tle_db_update_file(tle_filename, tle_db);
			}
		}
	}

	if ((num_unwritable > 0) && (tle_db->read_from_xdg)) {
		//write unwritable TLEs to new file
		char *new_tle_filename = tle_db_updatefile_writepath();

		struct tle_db unwritable_db = {0};
		for (int i=0; i < num_unwritable; i++) {
			tle_db_add_entry(&unwritable_db, &(tle_db->tles[unwritable_tles[i]]));
			strncpy(tle_db->tles[unwritable_tles[i]].filename, new_tle_filename, MAX_NUM_CHARS);
		}
		tle_db_to_file(new_tle_filename, &unwritable_db);

		free(new_tle_filename);
	}

	free(newer_tle_indices);
	free(tle_indices_to_update);
	free(unwritable_tles);
}

void tle_db_from_search_paths(struct tle_db *ret_tle_db)
{
	//read tles from user directory
	char *data_home = xdg_data_home();
	char home_tle_dir[MAX_NUM_CHARS] = {0};
	snprintf(home_tle_dir, MAX_NUM_CHARS, "%s%s", data_home, TLE_RELATIVE_DIR_PATH);
	tle_db_from_directory(home_tle_dir, ret_tle_db);
	free(data_home);

	char *data_dirs_str = xdg_data_dirs();
	string_array_t data_dirs = {0};
	stringsplit(data_dirs_str, &data_dirs);

	//read tles from system-wide data directories the order of precedence
	for (int i=0; i < string_array_size(&data_dirs); i++) {
		char dir[MAX_NUM_CHARS] = {0};
		snprintf(dir, MAX_NUM_CHARS, "%s%s", string_array_get(&data_dirs, i), TLE_RELATIVE_DIR_PATH);

		struct tle_db temp_db = {0};
		tle_db_from_directory(dir, &temp_db);
		tle_db_merge(&temp_db, ret_tle_db, TLE_OVERWRITE_NONE); //multiply defined TLEs in directories of less precedence are ignored
	}
	string_array_free(&data_dirs);
	free(data_dirs_str);

	ret_tle_db->read_from_xdg = true;
}

enum qth_file_state qth_from_search_paths(predict_observer_t *ret_observer)
{
	//try to read QTH file from user home
	char *config_home = xdg_config_home();
	char qth_path[MAX_NUM_CHARS] = {0};
	snprintf(qth_path, MAX_NUM_CHARS, "%s%s", config_home, QTH_RELATIVE_FILE_PATH);
	int readval = qth_from_file(qth_path, ret_observer);
	free(config_home);

	if (readval != 0) {
		//try to read from system default
		char *config_dirs = xdg_config_dirs();
		string_array_t dirs = {0};
		stringsplit(config_dirs, &dirs);
		bool qth_file_found = false;

		for (int i=0; i < string_array_size(&dirs); i++) {
			snprintf(qth_path, MAX_NUM_CHARS, "%s%s", string_array_get(&dirs, i), QTH_RELATIVE_FILE_PATH);
			if (qth_from_file(qth_path, ret_observer) == 0) {
				qth_file_found = true;
				break;
			}
		}
		free(config_dirs);

		if (qth_file_found) {
			return QTH_FILE_SYSTEMWIDE;
		} else {
			return QTH_FILE_NOTFOUND;
		}
	}
	return QTH_FILE_HOME;
}

char* qth_default_writepath()
{
	create_xdg_dirs();

	char *config_home = xdg_config_home();
	char *qth_path = (char*)malloc(sizeof(char)*MAX_NUM_CHARS);
	snprintf(qth_path, MAX_NUM_CHARS, "%s%s", config_home, QTH_RELATIVE_FILE_PATH);
	free(config_home);

	return qth_path;
}

void qth_to_file(const char *qth_path, predict_observer_t *qth)
{
	FILE *fd;

	fd=fopen(qth_path,"w");

	fprintf(fd,"%s\n",qth->name);
	fprintf(fd," %g\n",qth->latitude*180.0/M_PI);
	fprintf(fd," %g\n",-qth->longitude*180.0/M_PI); //convert from N/E to N/W
	fprintf(fd," %d\n",(int)floor(qth->altitude));

	fclose(fd);
}

int qth_from_file(const char *qthfile, predict_observer_t *observer)
{
	//copied from ReadDataFiles().

	char callsign[MAX_NUM_CHARS];
	FILE *fd=fopen(qthfile,"r");
	if (fd!=NULL) {
		fgets(callsign,16,fd);
		callsign[strlen(callsign)-1]=0;

		double latitude, longitude;
		int altitude;
		fscanf(fd,"%lf", &latitude);
		fscanf(fd,"%lf", &longitude);
		fscanf(fd,"%d", &altitude);
		fclose(fd);

		strncpy(observer->name, callsign, 16);
		observer->latitude = latitude*M_PI/180.0;
		observer->longitude = -longitude*M_PI/180.0; //convert from N/W to N/E
		observer->altitude = altitude*M_PI/180.0;
	} else {
		return -1;
	}
	return 0;
}

void transponder_db_from_search_paths(const struct tle_db *tle_db, struct transponder_db *transponder_db)
{
	string_array_t data_dirs = {0};
	char *data_home = xdg_data_home();
	string_array_add(&data_dirs, data_home);
	free(data_home);

	char *data_dirs_str = xdg_data_dirs();
	stringsplit(data_dirs_str, &data_dirs);
	free(data_dirs_str);

	//read transponder databases from system-wide data directories in opposide order of precedence, and then the home directory
	for (int i=string_array_size(&data_dirs)-1; i >= 0; i--) {
		char db_path[MAX_NUM_CHARS] = {0};
		snprintf(db_path, MAX_NUM_CHARS, "%s%s", string_array_get(&data_dirs, i), DB_RELATIVE_FILE_PATH);

		//will overwrite existing entries at their correct positions automatically, and ignore everything else
		transponder_db_from_file(db_path, tle_db, transponder_db);
	}
	string_array_free(&data_dirs);
}
