#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "flyby_ui.h"
#include <stdlib.h>
#include "string_array.h"
#include <dirent.h>
#include "flyby_config.h"
#include <math.h>

//xdg basedir specification variables
#define XDG_DATA_DIRS "XDG_DATA_DIRS"
#define XDG_DATA_DIRS_DEFAULT "/usr/local/share/:/usr/share/"
#define XDG_DATA_HOME "XDG_DATA_HOME"
#define XDG_DATA_HOME_DEFAULT ".local/share/"

//default relative TLE data directory
#define TLE_FOLDER "flyby/tles/"

/**
 * \return XDG_DATA_DIRS variable, or the xdg basedir specification default if the environment variable is empty
 **/
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

/**
 * \return XDG_DATA_HOME variable, or the xdg basedir specification default if the environment variable is empty
 **/
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
 * Check if tle_1 is newer than tle_2.
 *
 * \param tle_1 First NORAD TLE
 * \param tle_2 Second NORAD TLE
 * \return True if tle_1 is newer than tle_2, false otherwise
 **/
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

/**
 * Defines whether to overwrite only older TLE entries or all existing TLE entries when merging two databases.
 **/
enum tle_merge_behavior {
	///Overwrite only old existing TLE entries
	TLE_OVERWRITE_OLD,
	///Overwrite all existing TLE entries
	TLE_OVERWRITE_ALL
};

/**
 * Merge two TLE databases.
 *
 * \param new_db New TLE database to merge into an existing one
 * \param main_db Existing TLE database into which new TLE database is to be merged
 * \param merge_opt Merge options
 **/
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

/**
 * Read TLEs from files in specified directory. When TLE entries are multiply defined
 * across TLE files, the TLE entry with the most recent epoch is chosen.
 *
 * \param dirpath Directory from which files are to be read
 * \param ret_tle_db Returned TLE database
 **/
void flyby_read_tle_from_directory(const char *dirpath, struct tle_db *ret_tle_db) {
	DIR *d;
	struct dirent *file;
	d = opendir(dirpath);
	if (d) {
		while ((file = readdir(d)) != NULL) {
			if (file->d_type == DT_REG) {
				char *full_path = (char*)malloc(sizeof(char)*(strlen(file->d_name) + strlen(dirpath)));
				full_path[0] = '\0';
				strcat(full_path, dirpath);
				strcat(full_path, file->d_name);

				//read into empty TLE db
				struct tle_db temp_db = {0};
				flyby_read_tle_file(full_path, &temp_db);
				free(full_path);

				//merge with existing TLE db
				tle_merge_db(&temp_db, ret_tle_db, TLE_OVERWRITE_OLD); //overwrite only entries with older epochs
			}
		}
		closedir(d);
	}
}

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
	//FIXME: TLE db entry needs information about TLE filename and what kind of file it is and/or should
	//solve autoupdate problem in a different way.
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

int flyby_read_qth_file(const char *qthfile, predict_observer_t *observer)
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

int flyby_read_tle_file(const char *tle_file, struct tle_db *ret_db)
{
	//copied from ReadDataFiles().

	ret_db->num_tles = 0;
	int x = 0, y = 0;
	char name[80], line1[80], line2[80];

	struct tle_db_entry *sats = ret_db->tles;

	FILE *fd=fopen(tle_file,"r");
	if (fd!=NULL) {
		strncpy(ret_db->filename, tle_file, MAX_NUM_CHARS);

		while (x<MAX_NUM_SATS && feof(fd)==0) {
			/* Initialize variables */

			name[0]=0;
			line1[0]=0;
			line2[0]=0;

			/* Read element set */

			fgets(name,75,fd);
			fgets(line1,75,fd);
			fgets(line2,75,fd);

			if (KepCheck(line1,line2) && (feof(fd)==0)) {
				/* We found a valid TLE! */

				/* Some TLE sources left justify the sat
				   name in a 24-byte field that is padded
				   with blanks.  The following lines cut
				   out the blanks as well as the line feed
				   character read by the fgets() function. */

				y=strlen(name);

				while (name[y]==32 || name[y]==0 || name[y]==10 || name[y]==13 || y==0) {
					name[y]=0;
					y--;
				}

				/* Copy TLE data into the sat data structure */

				strncpy(sats[x].name,name,24);
				strncpy(sats[x].line1,line1,69);
				strncpy(sats[x].line2,line2,69);

				/* Get satellite number, so that the satellite database can be parsed. */

				char *tle[2] = {sats[x].line1, sats[x].line2};
				predict_orbital_elements_t *temp_elements = predict_parse_tle(tle);
				sats[x].satellite_number = temp_elements->satellite_number;
				predict_destroy_orbital_elements(temp_elements);

				x++;

			}
		}

		fclose(fd);
		ret_db->num_tles=x;
	} else {
		return -1;
	}
	return 0;
}

int flyby_read_transponder_db(const char *dbfile, const struct tle_db *tle_db, struct transponder_db *ret_db)
{
	//copied from ReadDataFiles().

	/* Load satellite database file */
	ret_db->num_sats = tle_db->num_tles;

	//initialize
	for (int i=0; i < MAX_NUM_SATS; i++) {
		struct sat_db_entry temp = {0};
		ret_db->sats[i] = temp;
	}

	FILE *fd=fopen(dbfile,"r");
	long catnum;
	unsigned char dayofweek;
	char line1[80];
	int y = 0, match = 0, transponders = 0, entry = 0;
	if (fd!=NULL) {
		fgets(line1,40,fd);

		while (strncmp(line1,"end",3)!=0 && line1[0]!='\n' && feof(fd)==0) {
			/* The first line is the satellite
			   name which is ignored here. */

			fgets(line1,40,fd);
			sscanf(line1,"%ld",&catnum);

			/* Search for match */

			for (y=0, match=0; y<tle_db->num_tles && match==0; y++) {
				if (catnum==tle_db->tles[y].satellite_number)
					match=1;
			}

			if (match) {
				transponders=0;
				entry=0;
				y--;
			}

			fgets(line1,40,fd);

			if (match) {
				if (strncmp(line1,"No",2)!=0) {
					sscanf(line1,"%lf, %lf",&(ret_db->sats[y].alat), &(ret_db->sats[y].alon));
					ret_db->sats[y].squintflag=1;
				}

				else
					ret_db->sats[y].squintflag=0;
			}

			fgets(line1,80,fd);

			while (strncmp(line1,"end",3)!=0 && line1[0]!='\n' && feof(fd)==0) {
				if (entry<MAX_NUM_TRANSPONDERS) {
					if (match) {
						if (strncmp(line1,"No",2)!=0) {
							line1[strlen(line1)-1]=0;
							strcpy(ret_db->sats[y].transponder_name[entry],line1);
						} else
							ret_db->sats[y].transponder_name[entry][0]=0;
					}

					fgets(line1,40,fd);

					if (match)
						sscanf(line1,"%lf, %lf", &(ret_db->sats[y].uplink_start[entry]), &(ret_db->sats[y].uplink_end[entry]));

					fgets(line1,40,fd);

					if (match)
						sscanf(line1,"%lf, %lf", &(ret_db->sats[y].downlink_start[entry]), &(ret_db->sats[y].downlink_end[entry]));

					fgets(line1,40,fd);

					if (match) {
						if (strncmp(line1,"No",2)!=0) {
							dayofweek=(unsigned char)atoi(line1);
							ret_db->sats[y].dayofweek[entry]=dayofweek;
						} else
							ret_db->sats[y].dayofweek[entry]=0;
					}

					fgets(line1,40,fd);

					if (match) {
						if (strncmp(line1,"No",2)!=0)
							sscanf(line1,"%d, %d",&(ret_db->sats[y].phase_start[entry]), &(ret_db->sats[y].phase_end[entry]));
						else {
							ret_db->sats[y].phase_start[entry]=0;
							ret_db->sats[y].phase_end[entry]=0;
						}

						if (ret_db->sats[y].uplink_start[entry]!=0.0 || ret_db->sats[y].downlink_start[entry]!=0.0)
							transponders++;

						entry++;
					}
				}
				fgets(line1,80,fd);
			}
			fgets(line1,80,fd);

			if (match)
				ret_db->sats[y].num_transponders=transponders;

			entry=0;
			transponders=0;
		}

		fclose(fd);
	} else {
		return -1;
	}
	return 0;
}
