#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "flyby_ui.h"
#include <stdlib.h>
#include "string_array.h"
#include <dirent.h>

#define XDG_DATA_DIRS "XDG_DATA_DIRS"
#define XDG_DATA_DIRS_DEFAULT "/usr/local/share/:/usr/share/"
char *get_data_dirs()
{
	char *data_dirs = getenv(XDG_DATA_DIRS);
	if (data_dirs == NULL) {
		data_dirs = XDG_DATA_DIRS_DEFAULT;
	}
	char *temp = data_dirs;
	data_dirs = (char*)malloc(sizeof(char)*(strlen(data_dirs) + 1));
	strcpy(data_dirs, temp);
	return data_dirs;
}

#define XDG_DATA_HOME "XDG_DATA_HOME"
#define XDG_DATA_HOME_DEFAULT ".local/share/"
char *get_data_home()
{
	char *data_dirs = getenv(XDG_DATA_HOME);
	if (data_dirs == NULL) {
		char *home_env = getenv("HOME");
		data_dirs = (char*)malloc(sizeof(char)*(strlen(home_env) + strlen(XDG_DATA_HOME_DEFAULT) + 2));
		data_dirs[0] = '\0';
		strcat(data_dirs, home_env);
		strcat(data_dirs, "/");
		strcat(data_dirs, XDG_DATA_HOME_DEFAULT);
	} else {
		char *temp = data_dirs;
		data_dirs = (char*)malloc(sizeof(char)*(strlen(data_dirs) + 1));
		strcpy(data_dirs, temp);
	}
	return data_dirs;
}

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

#define TLE_FOLDER "flyby/tles/"
bool tle_is_newer_than(char *tle_1[2], char *tle_2[2])
{
	predict_orbital_elements_t *orbele_1 = predict_parse_tle(tle_1);
	predict_orbital_elements_t *orbele_2 = predict_parse_tle(tle_2);

	double epoch_1 = orbele_1->epoch_year*1000.0 + orbele_1->epoch_day;
	double epoch_2 = orbele_2->epoch_year*1000.0 + orbele_2->epoch_day;

	predict_destroy_orbital_elements(orbele_1);
	predict_destroy_orbital_elements(orbele_2);

	return epoch_1 > epoch_2;
}

int flyby_read_tle_file(const char *tle_file, struct tle_db *ret_db);

enum tle_merge_behavior {
	TLE_OVERWRITE_OLD,
	TLE_OVERWRITE_ALL
};

void tle_merge_db(struct tle_db *new_db, struct tle_db *main_db, enum tle_merge_behavior merge_opt)
{
	for (int i=0; i < new_db->num_tles; i++) {
		bool tle_exists = false;
		for (int j=0; j < main_db->num_tles; j++) {
			//check whether TLE already exists in the database
			if (new_db->tles[i].satellite_number == main_db->tles[j].satellite_number) {
				tle_exists = true;					
				char *new_tle[2] = {new_db->tles[i].line1, new_db->tles[i].line2};
				char *old_tle[2] = {main_db->tles[j].line1, main_db->tles[j].line2};

				bool should_overwrite = false;

				if (merge_opt == TLE_OVERWRITE_ALL) {
					should_overwrite = true;
				} else if (tle_is_newer_than(new_tle, old_tle)) {
					should_overwrite = true;
				}

				if (should_overwrite) {
					strncpy(main_db->tles[j].name, new_db->tles[i].name, MAX_NUM_CHARS);
					strncpy(main_db->tles[j].line1, new_db->tles[i].line1, MAX_NUM_CHARS);
					strncpy(main_db->tles[j].line2, new_db->tles[i].line2, MAX_NUM_CHARS);
				}
			}
		}

		//append TLE entry to main TLE database
		if (!tle_exists) {
			if (main_db->num_tles+1 < MAX_NUM_SATS) {
				main_db->tles[main_db->num_tles] = new_db->tles[i];
				main_db->num_tles++;
			}
		}
	}
}

void flyby_read_tle_from_directory(const char *dirpath, struct tle_db *ret_tle_db) {
	printf("Reading from directory %s\n", dirpath);
	DIR *d;
	struct dirent *file;
	d = opendir(dirpath);
	if (d) {
		while ((file = readdir(d)) != NULL) {
			if (file->d_type == DT_REG) {
				printf("Reading from file %s\n", file->d_name);
				//read into empty TLE db
				struct tle_db temp_db = {0};
				flyby_read_tle_file(file->d_name, &temp_db);

				//merge with existing TLE db
				tle_merge_db(&temp_db, ret_tle_db, TLE_OVERWRITE_OLD); //overwrite only entries with older epochs
			}
		}
		closedir(d);
	}
	return;
}

/*
 * TLEs:
 * * Use union of TLE-entries in XDG_DATA_HOME/flyby/tles, XDG_DATA_DIRS/flyby/tles/
 * - For multiply defined TLE-entries, let the TLE defined in XDG_DATA_HOME (and TLEs in XDG_DATA_DIRS in order of preference) take precedence regardless of epoch. 
 * - For multiply defined TLE-entries within XDG_DATA_HOME, let the entry with the latest epoch take precedence. 
 *
 * Then we can also maintain TLE update functionality by writing updated TLE entry to XDG_DATA_HOME, and it will automatically override TLEs defined system-wide. 
 * - If TLE is defined in XDG_DATA_HOME: Update this TLE file. 
 * - If TLE is defined in TLE file supplied on the command line: Update this TLE file. 
 * - If TLE is defined system-wide: Add to update-file in XDG_DATA_HOME.  
 *   FIXME: TLE db entry needs information about TLE filename and what kind of file it is
 */
void flyby_read_tles_from_xdg(struct tle_db *ret_tle_db)
{
	char *data_dirs_str = get_data_dirs();
	string_array_t data_dirs = {0};
	stringsplit(data_dirs_str, &data_dirs);

	//read tles from system-wide data directories in opposide order of precedence
	for (int i=string_array_size(&data_dirs)-1; i >= 0; i--) {
		char dir[MAX_NUM_CHARS] = {0};
		strcat(dir, string_array_get(&data_dirs, i));
		strcat(dir, TLE_FOLDER);

		struct tle_db temp_db = {0};
		flyby_read_tle_from_directory(dir, &temp_db);
		tle_merge_db(&temp_db, ret_tle_db, TLE_OVERWRITE_ALL); //overwrite existing TLEs on multiple entries
	}
	string_array_free(&data_dirs);
	free(data_dirs_str);

	//read tles from user directory
	char *data_home = get_data_home();
	char home_tle_dir[MAX_NUM_CHARS] = {0};
	strcat(home_tle_dir, data_home);
	strcat(home_tle_dir, TLE_FOLDER);
	struct tle_db temp_db = {0};
	flyby_read_tle_from_directory(home_tle_dir, &temp_db);
	tle_merge_db(&temp_db, ret_tle_db, TLE_OVERWRITE_ALL);

	//FIXME: One problem with this approach is that home directory TLEs won't have precedence if TLE db size is exceeded. 
}

/*
 * .db-file:
 * * Use union of files in XDG_DATA_HOME/flyby/flyby.db and XDG_DATA_DIRS/flyby/flyby.db
 * * For multiply defined .db-entries, let the one in XDG_DATA_HOME take precedence, and then in XDG_DATA_DIRS in order of precedence
 *
 * Removing satellites/frequencies listed in the system-wide databases is covered by #20. Whitelist/blacklist should be placed in XDG_CONFIG_HOME/flyby/. 
 */
void flyby_read_transponder_db_from_xdg()
{
	//read entries from XDG_DATA_DIR in oppsite order of precedence
	
	//read entries from XDG_DATA_HOME
	//overwrite whenever a new entry is encountered
}
