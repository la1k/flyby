#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>
#include "flyby_defines.h"
#include "flyby_hamlib.h"
#include <predict/predict.h>
#include <math.h>
#include "flyby_ui.h"

//longopt value identificators for command line options without shorthand
#define FLYBY_OPT_ROTCTLD_PORT 201
#define FLYBY_OPT_UPLINK_PORT 202
#define FLYBY_OPT_UPLINK_VFO 203
#define FLYBY_OPT_DOWNLINK_PORT 204
#define FLYBY_OPT_DOWNLINK_VFO 205
#define FLYBY_OPT_ROTCTLD_ONCE_PER_SECOND 206

/**
 * Print flyby program usage to stdout.
 *
 * \param program_name Name of program, use argv[0]
 * \param long_options List of long options used in getopts_long
 * \param short_options List of short options used in getopts_long
 **/
void show_help(const char *program_name, struct option long_options[], const char *short_options);

/**
 * Dynamic size string array for the situations where we don't know in advance the number of filenames.
 * Used for accumulating TLE filenames. Exchange by std::vector if we start using C++ instead. :^)
 **/
typedef struct {
	int available_size; //available size within string array
	int num_strings; //current number of strings
	char **strings; //strings
} string_array_t;

/**
 * Add string to string array. Reallocates available space to twice the size when current available size is exceeded.
 *
 * \param string_array String array
 * \param string String to add
 * \return 0 on success, -1 on failure
 **/
int string_array_add(string_array_t *string_array, const char *string);

/**
 * Get string at specified index from string array.
 *
 * \param string_array String array
 * \param index Index at which we want to extract a string
 * \return String at specified index
 **/
const char* string_array_get(string_array_t *string_array, int index);

/**
 * Get string array size.
 *
 * \param string_array String array
 * \return Size
 **/
int string_array_size(string_array_t *string_array);

/**
 * Free memory allocated in string array.
 *
 * \param string_array String array to free
 **/
void string_array_free(string_array_t *string_array);

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

int main(int argc, char **argv)
{
	//rotctl options
	bool use_rotctl = false;
	char rotctld_host[MAX_NUM_CHARS] = ROTCTLD_DEFAULT_HOST;
	char rotctld_port[MAX_NUM_CHARS] = ROTCTLD_DEFAULT_PORT;
	bool rotctld_once_per_second = false;
	double tracking_horizon = 0;

	//rigctl uplink options
	bool use_rigctld_uplink = false;
	char rigctld_uplink_host[MAX_NUM_CHARS] = RIGCTLD_UPLINK_DEFAULT_HOST;
	char rigctld_uplink_port[MAX_NUM_CHARS] = RIGCTLD_UPLINK_DEFAULT_PORT;
	char rigctld_uplink_vfo[MAX_NUM_CHARS] = {0};
	
	//rigctl downlink options
	bool use_rigctld_downlink = false;
	char rigctld_downlink_host[MAX_NUM_CHARS] = RIGCTLD_DOWNLINK_DEFAULT_HOST;
	char rigctld_downlink_port[MAX_NUM_CHARS] = RIGCTLD_DOWNLINK_DEFAULT_HOST;
	char rigctld_downlink_vfo[MAX_NUM_CHARS] = {0};

	//config files
	char qth_filename[MAX_NUM_CHARS] = {0};
	char db_filename[MAX_NUM_CHARS] = {0};
	char tle_filename[MAX_NUM_CHARS] = {0};

	//read config filenames
	//TODO: To be replaced with config paths from XDG-standard, issue #1.
	char *env = getenv("HOME");
	snprintf(qth_filename, MAX_NUM_CHARS, "%s/.flyby/flyby.qth", env);
	snprintf(db_filename, MAX_NUM_CHARS, "%s/.flyby/flyby.db", env);
	snprintf(tle_filename, MAX_NUM_CHARS, "%s/.flyby/flyby.tle", env);

	string_array_t tle_update_filenames = {0}; //TLE files to be used to update the TLE databases

	//command line options
	struct option long_options[] = {
		{"update-tle-db",		required_argument,	0,	'u'},
		{"tle-file",			required_argument,	0,	't'},
		{"qth-file",			required_argument,	0,	'q'},
		{"rotctl",			required_argument,	0,	'a'},
		{"rotctld-port",		required_argument,	0,	FLYBY_OPT_ROTCTLD_PORT},
		{"rotctld-horizon",		required_argument,	0,	'H'},
		{"rotctld-once-per-second",	no_argument,		0,	FLYBY_OPT_ROTCTLD_ONCE_PER_SECOND},
		{"rigctld-uplink",		required_argument,	0,	'U'},
		{"rigctld-uplink-port",		required_argument,	0,	FLYBY_OPT_UPLINK_PORT},
		{"rigctld-uplink-vfo",		required_argument,	0,	FLYBY_OPT_UPLINK_VFO},
		{"rigctld-downlink",		required_argument,	0,	'D'},
		{"rigctld-downlink-port",	required_argument,	0,	FLYBY_OPT_DOWNLINK_PORT},
		{"rigctld-downlink-vfo",	required_argument,	0,	FLYBY_OPT_DOWNLINK_VFO},
		{"help",			no_argument,		0,	'h'},
		{0, 0, 0, 0}
	};
	char short_options[] = "u:t:q:a:H:U:D:h";
	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'u': //updatefile
				string_array_add(&tle_update_filenames, optarg);
				break;
			case 't': //tlefile
				strncpy(tle_filename, optarg, MAX_NUM_CHARS);
				break;
			case 'q': //qth
				strncpy(qth_filename, optarg, MAX_NUM_CHARS);
				break;
			case 'a': //rotctl
				use_rotctl = true;
				strncpy(rotctld_host, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_ROTCTLD_PORT: //rotctl port
				strncpy(rotctld_port, optarg, MAX_NUM_CHARS);
				break;
			case 'H': //horizon
				tracking_horizon = strtod(optarg, NULL);
				break;
			case FLYBY_OPT_ROTCTLD_ONCE_PER_SECOND: //once per second-option
				rotctld_once_per_second = true;
				break;
			case 'U': //uplink
				use_rigctld_uplink = true;
				strncpy(rigctld_uplink_host, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_UPLINK_PORT: //uplink port
				strncpy(rigctld_uplink_port, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_UPLINK_VFO: //uplink vfo
				strncpy(rigctld_uplink_vfo, optarg, MAX_NUM_CHARS);
				break;
			case 'D': //downlink
				use_rigctld_downlink = true;
				strncpy(rigctld_downlink_host, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_DOWNLINK_PORT: //downlink port
				strncpy(rigctld_downlink_port, optarg, MAX_NUM_CHARS);
				break;
			case FLYBY_OPT_DOWNLINK_VFO: //downlink vfo
				strncpy(rigctld_downlink_vfo, optarg, MAX_NUM_CHARS);
				break;
			case 'h': //help
				show_help(argv[0], long_options, short_options);
				return 0;
				break;
		}
	}

	//read TLE database
	struct tle_db tle_db = {0};
	flyby_read_tle_file(tle_filename, &tle_db);

	//use tle update files to update the TLE database, if present
	int num_update_files = string_array_size(&tle_update_filenames);
	if (num_update_files > 0) {
		for (int i=0; i < num_update_files; i++) {
			printf("Updating TLE database using %s.\n", string_array_get(&tle_update_filenames, i));
			// FIXME: AutoUpdate(temp, num_sats, tle_db, NULL);
		}
		string_array_free(&tle_update_filenames);
		return 0;
	}

	//connect to rotctld
	rotctld_info_t rotctld = {0};
	rotctld.socket = -1;
	if (use_rotctl) {
		rotctld_connect(rotctld_host, rotctld_port, rotctld_once_per_second, tracking_horizon, &rotctld);
	}

	//connect to rigctld
	rigctld_info_t uplink = {0};
	uplink.socket = -1;
	if (use_rigctld_uplink) {
		rigctld_connect(rigctld_uplink_host, rigctld_uplink_port, rigctld_uplink_vfo, &uplink);
	}
	rigctld_info_t downlink = {0};
	downlink.socket = -1;
	if (use_rigctld_downlink) {
		rigctld_connect(rigctld_downlink_host, rigctld_downlink_port, rigctld_downlink_vfo, &downlink);
	}

	//read flyby config files
	predict_observer_t *observer = predict_create_observer("", 0, 0, 0);
	struct transponder_db transponder_db = {0};

	bool is_new_user = flyby_read_qth_file(qth_filename, observer) == -1;
	flyby_read_transponder_db(db_filename, &tle_db, &transponder_db);

	RunFlybyUI(is_new_user, qth_filename, observer, &tle_db, &transponder_db, &rotctld, &downlink, &uplink);

	//disconnect from rigctl and rotctl
	rigctld_disconnect(&downlink);
	rigctld_disconnect(&uplink);
	rotctld_disconnect(&rotctld);
}

/**
 * Returns true if specified option's value is a char within the short options char array (and has thus a short-hand version of the long option)
 *
 * \param short_options Array over short option chars
 * \param long_option Option struct to check
 * \returns True if option has a short option, false otherwise
 **/
bool has_short_option(const char *short_options, struct option long_option)
{
	const char *ptr = strchr(short_options, long_option.val);
	if (ptr == NULL) {
		return false;
	}
	return true;
}

void show_help(const char *name, struct option long_options[], const char *short_options)
{
	//display initial description
	printf("\nUsage:\n");
	printf("%s [options]\n\n", name);
	printf("Options:\n");

	int index = 0;
	while (true) {
		if (long_options[index].name == 0) {
			break;
		}

		//display short option
		if (has_short_option(short_options, long_options[index])) {
			printf(" -%c,", long_options[index].val);
		} else {
			printf("    ");
		}
		
		//display long option
		printf("--%s", long_options[index].name);

		//display usage information
		switch (long_options[index].val) {
			case 'u':
				printf("=FILE\t\tupdate TLE database with TLE file FILE. Multiple files can be specified using the same option multiple times (e.g. -u file1 -u file2 ...). %s will exit afterwards", name);
				break;
			case 't':
				printf("=FILE\t\t\tuse FILE as TLE database file. Overrides user and system TLE database files. Only a single file is supported.");
				break;
			case 'q':
				printf("=FILE\t\t\tuse FILE as QTH config file. Overrides existing QTH config file");
				break;
			case 'a':
				printf("=SERVER_HOST\t\tconnect to a rotctld server with hostname SERVER_HOST and enable antenna tracking");
				break;
			case FLYBY_OPT_ROTCTLD_PORT:
				printf("=SERVER_PORT\t\tspecify rotctld server port");
				break;
			case 'H':
				printf("=HORIZON\t\tspecify elevation threshold for when %s will start tracking an orbit", name);
				break;
			case FLYBY_OPT_ROTCTLD_ONCE_PER_SECOND:
				printf("\t\tSend updates to rotctld once per second instead of when (azimuth,elevation) changes");
				break;
			case 'U':
				printf("=SERVER_HOST\tconnect to specified rigctld server for uplink frequency steering");
				break;
			case FLYBY_OPT_UPLINK_PORT:
				printf("=SERVER_PORT\tspecify rigctld uplink port");
				break;
			case FLYBY_OPT_UPLINK_VFO:
				printf("=VFO_NAME\tspecify rigctld uplink VFO");
				break;
			case 'D':
				printf("=SERVER_HOST\tconnect to specified rigctld server for downlink frequency steering");
				break;
			case FLYBY_OPT_DOWNLINK_PORT:
				printf("=SERVER_PORT\tspecify rigctld downlink port");
				break;
			case FLYBY_OPT_DOWNLINK_VFO:
				printf("=VFO_NAME\tspecify rigctld downlink VFO");
				break;
			case 'h':
				printf("\t\t\t\tShow help");
				break;
			default:
				printf("DOCUMENTATION MISSING\n");
				break;
		}
		index++;
		printf("\n");
	}
}

int string_array_add(string_array_t *string_array, const char *string)
{
	//initialize
	if (string_array->available_size < 1) {
		string_array->strings = (char**)malloc(sizeof(char*));
		string_array->available_size = 1;
		string_array->num_strings = 0;
	}

	//extend size to twice the current size
	if (string_array->num_strings+1 > string_array->available_size) {
		char **temp = realloc(string_array->strings, sizeof(char*)*string_array->available_size*2);
		if (temp == NULL) {
			return -1;
		}
		string_array->available_size = string_array->available_size*2;
		string_array->strings = temp;
	}

	//copy string
	string_array->strings[string_array->num_strings] = (char*)malloc(sizeof(char)*MAX_NUM_CHARS);
	strncpy(string_array->strings[string_array->num_strings], string, MAX_NUM_CHARS);
	string_array->num_strings++;
	return 0;
}

const char* string_array_get(string_array_t *string_array, int index)
{
	if (index < string_array->num_strings) {
		return string_array->strings[index];
	}
	return NULL;
}

void string_array_free(string_array_t *string_array)
{
	for (int i=0; i < string_array->num_strings; i++) {
		free(string_array->strings[i]);
	}
	free(string_array->strings);
	string_array->num_strings = 0;
	string_array->available_size = 0;
	string_array->strings = NULL;
}

int string_array_size(string_array_t *string_array)
{
	return string_array->num_strings;
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
