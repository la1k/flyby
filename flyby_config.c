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

//xdg basedir specification variables
#define XDG_DATA_DIRS "XDG_DATA_DIRS"
#define XDG_DATA_DIRS_DEFAULT "/usr/local/share/:/usr/share/"
#define XDG_DATA_HOME "XDG_DATA_HOME"
#define XDG_DATA_HOME_DEFAULT ".local/share/"
#define XDG_CONFIG_DIRS "XDG_CONFIG_DIRS"
#define XDG_CONFIG_DIRS_DEFAULT "/etc/xdg/"
#define XDG_CONFIG_HOME "XDG_CONFIG_HOME"
#define XDG_CONFIG_HOME_DEFAULT ".config/"

//default relative TLE data directory
#define TLE_PATH "flyby/tles/"

//default relative qth config filename
#define QTH_PATH "flyby/flyby.qth"

#define DB_PATH "flyby/flyby.db"

//default relative subdir
#define SUBDIR_PATH "flyby/"

char *xdg_dirs(const char *varname, const char *default_val)
{
	char *default_copy = strdup(default_val);
	char *data_dirs = getenv(varname);
	if (data_dirs == NULL) {
		default_copy = strdup(default_val);
		data_dirs = default_copy;
	}
	char *temp = data_dirs;
	data_dirs = (char*)malloc(sizeof(char)*(strlen(data_dirs) + 1));
	strcpy(data_dirs, temp);

	if (default_copy != NULL) {
		free(default_copy);
	}

	return data_dirs;
}

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
					strncpy(main_db->tles[j].filename, new_db->tles[i].filename, MAX_NUM_CHARS);
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
				int pathsize = strlen(file->d_name) + strlen(dirpath) + 1;
				char *full_path = (char*)malloc(sizeof(char)*pathsize);
				snprintf(full_path, pathsize, "%s%s", dirpath, file->d_name);

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
	char *data_dirs_str = xdg_data_dirs();
	string_array_t data_dirs = {0};
	stringsplit(data_dirs_str, &data_dirs);

	//read tles from system-wide data directories in opposide order of precedence
	for (int i=string_array_size(&data_dirs)-1; i >= 0; i--) {
		char dir[MAX_NUM_CHARS] = {0};
		snprintf(dir, MAX_NUM_CHARS, "%s%s", string_array_get(&data_dirs, i), TLE_PATH);

		struct tle_db temp_db = {0};
		flyby_read_tle_from_directory(dir, &temp_db);
		tle_merge_db(&temp_db, ret_tle_db, TLE_OVERWRITE_ALL); //overwrite existing TLEs on multiple entries
	}
	string_array_free(&data_dirs);
	free(data_dirs_str);

	//read tles from user directory
	char *data_home = xdg_data_home();
	char home_tle_dir[MAX_NUM_CHARS] = {0};
	snprintf(home_tle_dir, MAX_NUM_CHARS, "%s%s", data_home, TLE_PATH);
	struct tle_db temp_db = {0};
	flyby_read_tle_from_directory(home_tle_dir, &temp_db);
	tle_merge_db(&temp_db, ret_tle_db, TLE_OVERWRITE_ALL);
	free(data_home);
}

enum qth_file_state flyby_read_qth_from_xdg(predict_observer_t *ret_observer)
{
	//try to read QTH file from user home
	char *config_home = xdg_config_home();
	char qth_path[MAX_NUM_CHARS] = {0};
	snprintf(qth_path, MAX_NUM_CHARS, "%s%s", config_home, QTH_PATH);
	int readval = flyby_read_qth_file(qth_path, ret_observer);
	free(config_home);

	if (readval != 0) {
		//try to read from system default
		char *config_dirs = xdg_config_dirs();
		string_array_t dirs = {0};
		stringsplit(config_dirs, &dirs);
		bool qth_file_found = false;

		for (int i=0; i < string_array_size(&dirs); i++) {
			snprintf(qth_path, MAX_NUM_CHARS, "%s%s", string_array_get(&dirs, i), QTH_PATH);
			if (flyby_read_qth_file(qth_path, ret_observer) == 0) {
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

/**
 * Create ~/.config/flyby and ./local/share/flyby/tles/ if these do not exist.
 **/
void create_xdg_dirs()
{
	//create ~/.config/flyby
	char *config_home = xdg_config_home();
	char config_path[MAX_NUM_CHARS] = {0};
	snprintf(config_path, MAX_NUM_CHARS, "%s%s", config_home, SUBDIR_PATH);
	free(config_home);
	struct stat s;
	int err = stat(config_path, &s);
	if ((err == -1) && (errno == ENOENT)) {
		mkdir(config_path, 0777);
	}

	//create ~/.local/share/flyby
	char *data_home = xdg_data_home();
	char data_path[MAX_NUM_CHARS] = {};
	snprintf(data_path, MAX_NUM_CHARS, "%s%s", data_home, SUBDIR_PATH);
	err = stat(data_path, &s);
	if ((err == -1) && (errno == ENOENT)) {
		mkdir(data_path, 0777);
	}

	//create ~/.local/share/flyby/tles
	snprintf(data_path, MAX_NUM_CHARS, "%s%s", data_home, TLE_PATH);
	err = stat(data_path, &s);
	if ((err == -1) && (errno == ENOENT)) {
		mkdir(data_path, 0777);
	}
	free(data_home);
}

char* flyby_get_xdg_qth_writepath()
{
	create_xdg_dirs();

	char *config_home = xdg_config_home();
	char *qth_path = (char*)malloc(sizeof(char)*MAX_NUM_CHARS);
	snprintf(qth_path, MAX_NUM_CHARS, "%s%s", config_home, QTH_PATH);
	free(config_home);

	return qth_path;
}

void flyby_write_qth_to_file(const char *qth_path, predict_observer_t *qth)
{
	FILE *fd;

	fd=fopen(qth_path,"w");

	fprintf(fd,"%s\n",qth->name);
	fprintf(fd," %g\n",qth->latitude*180.0/M_PI);
	fprintf(fd," %g\n",-qth->longitude*180.0/M_PI); //convert from N/E to N/W
	fprintf(fd," %d\n",(int)floor(qth->altitude));

	fclose(fd);
}

void flyby_read_transponder_db_from_xdg(const struct tle_db *tle_db, struct transponder_db *transponder_db)
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
		snprintf(db_path, MAX_NUM_CHARS, "%s%s", string_array_get(&data_dirs, i), DB_PATH);

		//will overwrite existing entries at their correct positions automatically, and ignore everything else
		flyby_read_transponder_db(db_path, tle_db, transponder_db);
	}
	string_array_free(&data_dirs);
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

				strncpy(sats[x].filename, tle_file, MAX_NUM_CHARS);

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

			if (match) {
				ret_db->sats[y].num_transponders=transponders;
				ret_db->loaded = true;
			}

			entry=0;
			transponders=0;
		}

		fclose(fd);
	} else {
		return -1;
	}
	return 0;
}
